# ASPIS - Automatic Software-based Protection and Integrity Suite

ASPIS (from the ancient Greek Ἀσπίς Aspís, *shield*) is an out-of-tree plugin for LLVM that acts on the Intermediate-Representation (IR) in order to harden the code against Single-Event Upsets (SEUs). 

The hardening process is done by the sequence of passes depicted in the following figure:
<p align=center>
<img src="figures/compiler_scheme.jpg" alt="drawing" width="400"/>
</p>

## Pre-requisites

The toolchain has been tested with the following versions:
- CMake 3.22.1
- LLVM 16.0.0

During the development of ASPIS, done mostly on LLVM 15, we discovered a bug in the [`splitBasicBlock()`](https://llvm.org/doxygen/classllvm_1_1BasicBlock.html#a2bc5caaabd6841e4ab97237ebcaeb86d) procedure. The bug has been fixed in LLVM 16, so we recommend using it rather than applying the patch to the previous versions. 

## Building

To build ASPIS, type the following commands:

```bash
mkdir build
cmake -B build -DLLVM_DIR=<your/llvm/dir>
cmake --build build
```

Where `your/llvm/dir` is the directory where LLVMConfig.cmake is found (check here [here](https://llvm.org/docs/CMake.html) for further information).

# Usage

In order to apply ASPIS, you can use the built-in compilation pipeline provided by the `aspis.sh` shell script, or you can make your own custom compilation pipeline using LLVM `opt`.

## Fault Handler 
In one of your compilation unit, you must declare two extern functions having these prototypes:
```C
void DataCorruption_Handler(void) {

}
```

and

```C
void SigMismatch_Handler(void) {

}
```

These functions are invoked by ASPIS when a fault is detected.

## Annotations

When compiling `C`/`C++`, it is possible to use clang annotations in the source to manually tell the compiler what to do with specific variables and/or functions. The syntax for the annotation is the following:

```C
__attribute__((annotate(<annotation>)))
```

The following describes the possibilities for `<annotation>`.

### The `to_duplicate` annotation

```C
__attribute__((annotate("to_duplicate")))
```

When a function is declared this way, ASPIS does not duplicate the function body, but duplicates the call to the function.

When a global variable outside the compilation unit is declared this way, ASPIS duplicates it.


### The `exclude` annotation

```C
__attribute__((annotate("exclude")))
```
ASPIS does not compile the annotated function or does not duplicate the annotated global variable.

## Built-in compilation pipeline
`aspis.sh` is a simple command-line interface that allows users to run the entire compilation pipeline specifying a few command-line arguments. The arguments that are not recognised are passed directly to the front-end, hence all the `clang` arguments are admissible.

### Options
 - `-h`, `--help`: Display available options.
 - `-o <file>`: Write the compilation output to `<file>`.
 - `--llvm-bin <path>`: Set the path to the llvm binaries (clang, opt, llvm-link) to `<path>`.
 - `--exclude <file>`: Set the files to exclude from the compilation. The content of `<file>` is the list of files to exclude, one for each line (wildcard `*` allowed).
 - `--asmfiles <file>`: Defines the set of assembly files required for the compilation. The content of `<file>` is the list of assembly files to pass to the linker at compilation termination, one for each line (wildcard `*` allowed).

### Hardening
 - `--eddi`: **(Default)** Enable EDDI.
 - `--seddi`: Enable Selective-EDDI.
 - `--fdsc`: Enable Full Duplication with Selective Checking.

 - `--cfcss`: **(Default)** Enable CFCSS.
 - `--rasm`: Enable RASM.
 - `--inter-rasm`: Enable inter-RASM with the default signature `-0xDEAD`.

### Example

Sample `excludefile.txt` content:

```
dir/of/excluded/files/*.c
file_to_esclude.c
```

Sample `asmfiles.txt` content:
```
dir/of/asm/files/*.s
asmfile_to_link.s
```

Compile the files `file1.c`, `file2.c`, and `file3.c` as:

```bash
./aspis.sh --llvm-bin your/llvm/bin/ --exclude excludefile.txt --asmfiles asmfiles.txt --seddi --rasm file1.c file2.c file3.c -o <out_filename>.c
```

## Create a custom compilation pipeline
Once ASPIS has been built, you can apply the passes using `opt`.

The compiled passes can be found as shared object files (`.so`) into the `build/passes` directory, and are described in the following. In order to apply the optimization, you must use LLVM  `opt` to load the respective shared object file.

### Data protection
Developers can select one of the following passes for data protection using the `-eddi-verify` flag:

- `libEDDI.so` with the `-eddi-verify` flag is the implementation of EDDI in LLVM;
- `libFDSC.so` with the `-eddi-verify` flag is the implementation of Full Duplication with Selective Checking, an extension of EDDI in which consistency checks are only inserted at basic blocks having multiple predecessors.
- `libSEDDI.so` with the `-eddi-verify` flag is the implementation of selective-EDDI (sEDDI), an extension of EDDI in which consistency checks are inserted only at `branch` and `call` instructions (no `store`).

Before and after the application of the `-eddi-verify` passes, developers must apply the `-func-ret-to-ref` and the `-duplicate-globals` passes, respectively.

### Control-Flow Checking
These are the alternative passes for control-flow checking:
- `libCFCSS.so` with the `-cfcss-verify` is the implementation of CFCSS in LLVM;
- `libRASM.so` with the `-rasm-verify` is the implementation of RASM in LLVM;
- `libINTER_RASM` with the `-rasm-verify` is the implementation of RASM that achieves inter-function CFC.

### Example of compilation with ASPIS (sEDDI + RASM)
First, compile the codebase with the appropriate front-end.

```bash
clang <files.c> -emit-llvm -S
```

The output files are IR files having an `.ll` extension. It is required to link them using `llvm-link` as follows:

```bash
llvm-link -S *.ll -o out.ll
```

Now, `out.ll` is a huge `.ll` file containing all the IR of the code passed through the clang frontend. The `out.ll` file is then transformed by our passes in the following order:

- FuncRetToRef
- sEDDI
- RASM
- DuplicateGlobals

With the addition of some built-in LLVM passes (`lowerswitch` and `simplifycfg`).

Run the following:

```bash
opt -lowerswitch out.ll -o out.ll
opt --enable-new-pm=0 -S -load </path/to/ASPIS/>build/passes/libEDDI.so -func-ret-to-ref out.ll -o out.ll
opt --enable-new-pm=0 -S -load </path/to/ASPIS/>build/passes/libSEDDI.so -eddi-verify out.ll -o out.ll
opt -passes=simplifycfg out.ll -o out.ll
opt --enable-new-pm=0 -S -load </path/to/ASPIS/>build/passes/libRASM.so -rasm-verify out.ll -o out.ll
```
You may also want to include other files in the compilation, that are previously excluded because of some architecture-dependent features. This is done with the following commands, which first remove the previously emitted single `.ll` files, then compile the excluded code and link it with the hardened code:

```bash
mv out.ll out.ll.bak
rm *.ll
clang <excluded_files.c> -emit-llvm -S
llvm-link -S *.ll out.ll.bak -o out.ll
```

Then, apply the last pass and emit the executable: 

```bash
opt --enable-new-pm=0 -S -load </path/to/ASPIS/>build/passes/libEDDI.so -duplicate-globals out.ll -o out.ll
clang out.ll -o out.elf
```

## References
If you are using this tool in scientific works, please cite the following article:
- Davide Baroffio, Federico Reghenzani, and William Fornaciari. 2024. Enhanced Compiler Technology for Software-based Hardware Fault Detection. ACM Trans. Des. Autom. Electron. Syst. 29, 5, Article 91 (September 2024), 23 pages. https://doi.org/10.1145/3660524

