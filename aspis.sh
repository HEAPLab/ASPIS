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
clang_options=
eddi_options="-S"
cfc_options="-S"
llvm_bin=$(dirname $(which clang))
dup=0 # 0 = eddi,   1 = seddi,  2 = fdsc
cfc=0 # 0 = cfcss,  1 = rasm,   2 = inter-rasm
debug_enabled=false
verbose=false
cleanup=true
libstdcpp_added=false


raw_opts="$@"

for opt in $raw_opts; do
    case $parse_state in
        0)
            case $opt in
                -h | --help)
                    cat <<-EOF 
.-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-====-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-.
|                          _____ _____ _____  _____                |
!                   /\    / ____|  __ \_   _|/ ____|               !
:                  /  \  | (___ | |__) || | | (___                 :
:                 / /\ \  \___ \|  ___/ | |  \___ \                :
.                / ____ \ ____) | |    _| |_ ____) |               .
:               /_/    \_\_____/|_|   |_____|_____/                :
:                                                                  :
!  ASPIS: Automatic Software-based Protection and Integrity Suite  !
|                                                                  |
'--.. .- -. . .-.. .-.. .- -.-. ..- .-.. ---=-=-=-=-=-=-=-=-=-=-=-='

Usage: aspis.sh [options] file(s)...

The specified files can be any C source code files. 
By default, the compiler performs EDDI+CFCSS hardening.

Options:
    -h, --help          Display available options.
    -v, --verbose       Turn on verbose mode
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
    --no-cleanup        Does not remove the intermediate .ll files generated,
                        which might be useful for debug purposes.

Hardening mechanism:
    --eddi              (Default) Enable EDDI.
    --seddi             Enable Selective-EDDI.
    --fdsc              Enable Full Duplication with Selective Checking.

    --cfcss             (Default) Enable CFCSS.
    --rasm              Enable RASM.
    --inter-rasm        Enable inter-RASM with the default signature -0xDEAD.
    --no-cfc            Completely disable control-flow checking

Hardening options:
    --alternate-memmap  When set, alternates the definition of original and 
                        duplicate variables. By default they are allocated in 
                        groups maximizing the distance between original and
                        duplicated value.

EOF
                    exit 0
                    ;;
                -v | --verbose)
                    verbose=true;
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
                --no-cfc)
                    cfc=-1
                    ;;
                --alternate-memmap)
                    eddi_options="$eddi_options $opt=true";
                    ;;
                -g)
                    debug_enabled=true;
                    clang_options="$clang_options $opt";
                    ;;
                --no-cleanup)
                    cleanup=false;
                    ;;
                -lstdc++)
                    libstdcpp_added=true
                    ;;
                *.c | *.cpp)
                    input_files="$input_files $opt";
                    # Check if it's a .cpp file and if -lstdc++ hasn't been added yet
                     if [[ "$opt" == *.cpp ]] && [[ "$libstdcpp_added" == false ]]; then
                        clang_options="$clang_options -lstdc++"
                        libstdcpp_added=true 
                    fi
                    
                    ;;
                *)
                    clang_options="$clang_options $opt";
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

if [[ $verbose == true ]]; then
    echo "Verbose mode ON"
    # Function to display commands
    exe() { echo -e "\t\$ $@" ; "$@" ; }
else
    exe() { "$@" ; }
fi

CLANG="${llvm_bin}/clang" 
OPT="${llvm_bin}/opt"
LLVM_LINK="${llvm_bin}/llvm-link"

if [[ -n "$config_file" ]]; then
    CLANG="${CLANG} --config ${config_file}"
fi;

###############################################################################

echo -e "=== Front-end and pre-processing ==="

## FRONTEND
exe $CLANG $input_files $clang_options -S -emit-llvm -O0 -Xclang -disable-O0-optnone

## LINK & PREPROCESS
exe $LLVM_LINK *.ll -o out.ll -opaque-pointers

echo -e "\xE2\x9C\x94 Emitted and linked IR."



if [[ $debug_enabled == false ]]; then
    exe $OPT --enable-new-pm=1 --passes="strip" out.ll -o out.ll
    echo -e "\xE2\x9C\x94 Debug mode disabled, stripped debug symbols."
fi

    exe $OPT --enable-new-pm=1 --passes="lowerswitch" out.ll -o out.ll

## FuncRetToRef
exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="func-ret-to-ref" out.ll -o out.ll

echo -e "\n=== ASPIS transformations =========="
## DATA PROTECTION
case $dup in
    0) 
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="eddi-verify" out.ll -o out.ll $eddi_options
        ;;
    1) 
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libSEDDI.so --passes="eddi-verify" out.ll -o out.ll $eddi_options
        ;;
    2) 
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libFDSC.so --passes="eddi-verify" out.ll -o out.ll $eddi_options
        ;;
esac
echo -e "\xE2\x9C\x94 Applied data protection passes."

$OPT --enable-new-pm=1 --passes="dce,simplifycfg" out.ll -o out.ll


## CONTROL-FLOW CHECKING
case $cfc in
    0) 
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libCFCSS.so --passes="cfcss-verify" out.ll -o out.ll $cfc_options
        ;;
    1) 
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libRASM.so --passes="rasm-verify" out.ll -o out.ll $cfc_options
        ;;
    2) 
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libINTER_RASM.so --passes="rasm-verify" out.ll -o out.ll $cfc_options
        ;;
    *)
        echo -e "\t--no-cfc specified!"
esac
echo -e "\xE2\x9C\x94 Applied CFC passes."

if [[ -n "$exclude_file" ]]; then
    # scan the directories of excluded files
    while IFS= read -r line 
    do
    excluded_files="${excluded_files} ${line}"
    done < "$exclude_file";

    ## Frontend & linking
    exe mv out.ll out.ll.bak
    exe rm *.ll
    exe mv out.ll.bak out.ll
    exe $CLANG $clang_options -O0 -Xclang -disable-O0-optnone -emit-llvm -S $excluded_files #out.ll
    exe $LLVM_LINK *.ll -o out.ll
fi;
echo -e "\xE2\x9C\x94 Linked excluded files to the compilation."

## DuplicateGlobals
exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="duplicate-globals" out.ll -o out.ll -S $eddi_options
echo -e "\xE2\x9C\x94 Duplicated globals."

echo -e "\n=== Back-end ======================="
if [[ -n "$asm_file" ]]; then
    # scan the directories of excluded files
    while IFS= read -r line 
    do
    asm_files="${asm_files} ${line}"
    done < "$asm_file";
fi;

## Backend
exe $CLANG $clang_options -O0 out.ll $asm_files -o $output_file 
echo -e "\xE2\x9C\x94 Binary emitted."

#Cleanup
if [[ $cleanup == true ]]; then
    rm *.ll
    echo -e "\xE2\x9C\x94 Cleaned cached files."
fi

echo -e "\nDone!"
