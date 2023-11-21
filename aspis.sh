#!/usr/bin/env bash

SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

parse_state=0
output_file="out"
config_file=""
exclude_file=""
excluded_files=""
asm_file=""
asm_files=""
input_files=""
opts=
llvm_bin=$(dirname $(which clang))
dup=0 # 0 = eddi,   1 = seddi,  2 = fdsc
cfc=0 # 0 = cfcss,  1 = rasm,   2 = inter-rasm
raw_opts="$@"

for opt in $raw_opts; do
    case $parse_state in
        0)
            case $opt in
                -h | --help)
                    cat <<-EOF 
ASPIS: Automatic Software-based Protection to Improve Safety
Usage: aspis.sh [options] file(s)...

The specified files can be any C source code files. 
By default, the compiler performs EDDI+CFCSS hardening.

Options:
    -h, --help          Display available options.
    -o <file>           Write the compilation output to <file>.
    --llvm-bin <path>   Set the path to the llvm binaries (clang, opt, 
                        llvm-link) to <path>.
    --exclude <file>    Set the files to exclude from the compilation. The 
                        content of <file> is the list of files to 
                        exclude, one for each line (wildcard * allowed).
    --asmfiles <file>   Defines the set of assembly files required for the
                        compilation. The content of <file> is the list of 
                        assembly files to pass to the linker at compilation 
                        termination, one for each line (wildcard * allowed).

Hardening:
    --eddi              (Default) Enable EDDI.
    --seddi             Enable Selective-EDDI.
    --fdsc              Enable Full Duplication with Selective Checking.

    --cfcss             (Default) Enable CFCSS.
    --rasm              Enable RASM.
    --inter-rasm        Enable inter-RASM with the default signature -0xDEAD.
EOF
                    exit 0
                    ;;

                -o*)
                    if [[ ${#opt} -eq 2 ]]; then
                        parse_state=1;
                    else
                        output_file=`echo "$opt" | cut -b 2`;
                    fi;
                    ;;
                --llvm-bin*)
                    if [[ ${#opt} -eq 10 ]]; then
                        parse_state=3;
                    else
                        llvm_bin=`echo "$opt" | cut -b 10`;
                    fi;
                    ;;
                --exclude*)
                    if [[ ${#opt} -eq 9 ]]; then
                        parse_state=4;
                    else
                        exclude_file=`echo "$opt" | cut -b 9`;
                    fi;
                    ;;
                --asmfiles*)
                    if [[ ${#opt} -eq 10 ]]; then
                        parse_state=5;
                    else
                        asm_file=`echo "$opt" | cut -b 10`;
                    fi;
                    ;;
                --eddi)
                    dup=0
                    ;;
                --seddi)
                    dup=1
                    ;;
                --fdsc)
                    dup=2
                    ;;
                --cfcss)
                    cfc=0
                    ;;
                --rasm)
                    cfc=1
                    ;;
                --inter-rasm)
                    cfc=2
                    ;;
                *.c)
                    input_files="$input_files $opt";
                    ;;
                *)
                    opts="$opts $opt";
                    ;;
            esac;
            ;;
        1)
            output_file="$opt";
            parse_state=0;
            ;;
        2)
            config_file="$opt";
            parse_state=0;
            ;;
        3)
            llvm_bin="$opt";
            parse_state=0;
            ;;
        4)
            exclude_file="$opt";
            parse_state=0;
            ;;
        5)
            asm_file="$opt";
            parse_state=0;
            ;;
    esac
done

CLANG="${llvm_bin}/clang" 
OPT="${llvm_bin}/opt"
LLVM_LINK="${llvm_bin}/llvm-link"

if [[ -n "$config_file" ]]; then
    CLANG="${CLANG} --config ${config_file}"
fi;

###############################################################################

## FRONTEND
$CLANG $input_files $opts -S -emit-llvm -O0 -Xclang -disable-O0-optnone 

## LINK & PREPROCESS
$LLVM_LINK *.ll -o out.ll
$OPT -strip-debug out.ll -o out.ll
$OPT -lowerswitch out.ll -o out.ll

## FuncRetToRef
$OPT --enable-new-pm=0 -load $DIR/build/passes/libEDDI.so -func_ret_to_ref out.ll -o out.ll

## DATA PROTECTION
case $dup in
    0) 
        $OPT --enable-new-pm=0 -load $DIR/build/passes/libEDDI.so -eddi_verify out.ll -o out.ll
        ;;
    1) 
        $OPT --enable-new-pm=0 -load $DIR/build/passes/libSEDDI.so -eddi_verify out.ll -o out.ll
        ;;
    2) 
        $OPT --enable-new-pm=0 -load $DIR/build/passes/libFDSC.so -eddi_verify out.ll -o out.ll
        ;;
esac

$OPT -simplifycfg out.ll -o out.ll

## CONTROL-FLOW CHECKING
case $cfc in
    0) 
        $OPT --enable-new-pm=0 -load $DIR/build/passes/libCFCSS.so -cfcss_verify out.ll -o out.ll
        ;;
    1) 
        $OPT --enable-new-pm=0 -load $DIR/build/passes/libRASM.so -rasm_verify out.ll -o out.ll
        ;;
    2) 
        $OPT --enable-new-pm=0 -load $DIR/build/passes/libINTER_RASM.so -rasm_verify out.ll -o out.ll
        ;;
esac

if [[ -n "$exclude_file" ]]; then
    # scan the directories of excluded files
    while IFS= read -r line 
    do
    excluded_files="${excluded_files} ${line}"
    done < "$exclude_file";

    ## Frontend & linking
    mv out.ll out.ll.bak
    rm *.ll
    $CLANG $opts -O0 -Xclang -disable-O0-optnone -emit-llvm -S $excluded_files out.ll.bak
    $LLVM_LINK *.ll out.ll.bak -o out.ll
fi;

## DuplicateGlobals
$OPT --enable-new-pm=0 -load $DIR/build/passes/libEDDI.so -duplicate_globals out.ll -o out.ll

if [[ -n "$asm_file" ]]; then
    # scan the directories of excluded files
    while IFS= read -r line 
    do
    asm_files="${asm_files} ${line}"
    done < "$asm_file";
fi;

## Backend
$CLANG $opts -O0 out.ll $asm_files -o $output_file 

## Cleanup
rm *.ll
rm out.ll.bak