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
llvm_bin=$(dirname $(which clang &> /dev/null) &> /dev/null)
build_dir="."
dup=0 # 0 = eddi,   1 = seddi,  2 = fdsc
cfc=0 # 0 = cfcss,  1 = rasm,   2 = inter-rasm
debug_enabled=false
verbose=false
cleanup=true
libstdcpp_added=false
enable_profiling=false

# Check if the shell supports colors
if [ -t 1 ]; then
	ncolors=$(tput colors)
	if test -n "$ncolors" && test $ncolors -ge 8; then
		color_bold="$(tput bold)"
		color_underline="$(tput smul)"
		color_standout="$(tput smso)"
		color_normal="$(tput sgr0)"
		color_black="$(tput setaf 0)"
		color_red="$(tput setaf 1)"
		color_green="$(tput setaf 2)"
		color_yellow="$(tput setaf 3)"
		color_blue="$(tput setaf 4)"
		color_magenta="$(tput setaf 5)"
		color_cyan="$(tput setaf 6)"
		color_white="$(tput setaf 7)"
	fi
fi

error_msg () {
    # Print error message
    echo -e "\n${color_red}ERROR:${color_normal}" $@
    exit 1
}

success_msg() {
    echo -e "${color_green}\xE2\x9C\x94" $@ "${color_normal}"
}

title_msg () {
    echo -e "\n${color_bold}===" $@ "===${color_normal}"
}


perform_platform_checks() {
    if [ ! -f $1 ]; then
        error_msg "\nCommand clang not found. Expected path: ${1}. Please check --llvm_bin parameter."
    fi

    if [ ! -f $2 ]; then
        error_msg "\nCommand opt not found. Expected path: ${2}. Please check --llvm_bin parameter."
    fi

    if [ ! -f $3 ]; then
        error_msg "\nCommand llvm-link not found. Expected path: ${3}. Please check --llvm_bin parameter."
    fi
    
}


parse_commands() {

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
        --build-dir <path>  Specify the directory where to place all the build
                            files.
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
        --no-dup            Completely disable data duplication
        
        --cfcss             (Default) Enable CFCSS.
        --rasm              Enable RASM.
        --inter-rasm        Enable inter-RASM with the default signature -0xDEAD.
        --no-cfc            Completely disable control-flow checking

    Hardening options:
        --alternate-memmap  When set, alternates the definition of original and 
                            duplicate variables. By default they are allocated in 
                            groups maximizing the distance between original and
                            duplicated value.

        --enable-profiling  When set, enable the insertion of profiling function calls 
                            at synchonization points, which can be used to trace where
                            consistency checks are executed.

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
                    --build-dir*)
                        if [[ ${#opt} -eq 11 ]]; then
                            parse_state=6;
                        else
                            build_dir=`echo "$opt" | cut -b 10`;
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
                    --no-dup)
                        dup=-1
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
                    --enable-profiling)
                        eddi_options="$eddi_options $opt=true";
                        cfc_options="$cfc_options $opt=true";
                        enable_profiling=true;
                        ;;
                    -g)
                        debug_enabled=true;
                        clang_options="$clang_options $opt";
                        eddi_options="$eddi_options --debug-enabled=true";
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
            6) 
                build_dir="$opt";
                parse_state=0;
                ;;
        esac
    done

    if [[ $verbose == true ]]; then
        echo "Verbose mode ON"
        # Function to display commands
        exe() { 
            echo -e "\t\$ $@"
            "$@"
            local status=$?
            if [[ $status -ne 0 ]]; then
                error_msg "Command FAILED: $@"
            fi
        }
    else
        exe() { 
            "$@"
            local status=$?
            if [[ $status -ne 0 ]]; then
                error_msg "Command FAILED: $@"
            fi 
        }
    fi

    CLANG="${llvm_bin}/clang" 
    OPT="${llvm_bin}/opt"
    LLVM_LINK="${llvm_bin}/llvm-link"

    if [[ -n "$config_file" ]]; then
        CLANG="${CLANG} --config ${config_file}"
    fi;
}

run_aspis() {
    if [[ -z ${input_files} ]]; then
        error_msg "No input files provided."
    fi

    exe mkdir -p $build_dir
    exe rm -f $build_dir/*.ll

    title_msg "Front-end and pre-processing"

    ## FRONTEND
    for input_file in $input_files; do
        # Extract the filename without extension
        filename=$(basename "$input_file" | sed 's/\.[^.]*$//')
        # Compile the file to LLVM IR (.ll) and save it in the build directory
        exe $CLANG "$input_file" $clang_options -S -emit-llvm -O0 -Xclang -disable-O0-optnone -o "$build_dir/$filename.ll" 
    done

    ## LINK & PREPROCESS
    exe $LLVM_LINK $build_dir/*.ll -o $build_dir/out.ll -opaque-pointers

    success_msg "Emitted and linked IR."

    if [[ $debug_enabled == false ]]; then
        exe $OPT --enable-new-pm=1 --passes="strip" $build_dir/out.ll -o $build_dir/out.ll
        echo "  Debug mode disabled, stripped debug symbols."
    fi

        #exe $OPT --enable-new-pm=1 --passes="default<O2>,lowerswitch" $build_dir/out.ll -o $build_dir/out.ll
        exe $OPT --enable-new-pm=1 --passes="lowerswitch" $build_dir/out.ll -o $build_dir/out.ll

    ## FuncRetToRef
    if [[ dup != -1 ]]; then
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="func-ret-to-ref" $build_dir/out.ll -o $build_dir/out.ll
    fi;

    title_msg "ASPIS transformations"
    ## DATA PROTECTION
    case $dup in
        0) 
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        1) 
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libSEDDI.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        2) 
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libFDSC.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        *)
            echo -e "\t--no-dup specified!"
    esac
    success_msg "Applied data protection passes."

    exe $OPT --enable-new-pm=1 --passes="simplifycfg" $build_dir/out.ll -o $build_dir/out.ll


    ## CONTROL-FLOW CHECKING
    case $cfc in
        0) 
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libCFCSS.so --passes="cfcss-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
            ;;
        1) 
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libRASM.so --passes="rasm-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
            ;;
        2) 
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libINTER_RASM.so --passes="rasm-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
            ;;
        *)
            echo -e "\t--no-cfc specified!"
    esac
    success_msg "Applied CFC passes."

    if [[ -n "$exclude_file" ]]; then
        # scan the directories of excluded files
        while IFS= read -r line 
        do
        excluded_files="${excluded_files} ${line}"
        done < "$exclude_file";

        ## Frontend & linking
        exe mv $build_dir/out.ll $build_dir/out.ll.bak
        exe rm $build_dir/*.ll
        exe mv $build_dir/out.ll.bak $build_dir/out.ll
        for input_file in $excluded_files; do
            # Extract the filename without extension
            filename=$(basename "$input_file" | sed 's/\.[^.]*$//')
            # Compile the file to LLVM IR (.ll) and save it in the build directory
            exe $CLANG "$input_file" $clang_options -S -emit-llvm -Xclang -disable-O0-optnone -o "$build_dir/$filename.ll" 
        done
        exe $LLVM_LINK $build_dir/*.ll -o $build_dir/out.ll
    fi;
    success_msg "Linked excluded files to the compilation."

    ## DuplicateGlobals
    if [[ dup != -1 ]]; then
        exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="duplicate-globals" $build_dir/out.ll -o $build_dir/out.ll -S $eddi_options
        success_msg "Duplicated globals."
    fi;

    title_msg "Back-end"
    if [[ -n "$asm_file" ]]; then
        # scan the directories of excluded files
        while IFS= read -r line 
        do
        asm_files="${asm_files} ${line}"
        done < "$asm_file";
    fi;

    if [[ -n "$enable_profiling" ]]; then
# full O2
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),function<eager-inv>(early-cse<>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(instcombine),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(early-cse<memssa>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-rotate))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(gvn<>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-rotate)),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(instcombine),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(instcombine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(instcombine),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
# O2 w/o instcombine
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),function<eager-inv>(early-cse<>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(early-cse<memssa>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-rotate))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(gvn<>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-rotate)),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
# O2 w/o cse 
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(instcombine),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-rotate))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(gvn<>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-rotate)),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(instcombine),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(instcombine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(instcombine),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
# O2 w/o instcombine and cse
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-rotate))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(gvn<>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-rotate)),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
# O2 w/o instcombine, cse, and gvn
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-rotate))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-rotate)),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
# O2 w/o looprotate
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),function<eager-inv>(early-cse<>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(instcombine),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(early-cse<memssa>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(gvn<>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(instcombine))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(instcombine),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(instcombine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(instcombine),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
# O2 w/o looprotate, instcombine, cse, and gvn
        #passes="annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(lower-expect),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function<eager-inv>(sroa<modify-cfg>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>)),cgscc(devirt<4>(inline)),cgscc(devirt<4>(function-attrs)),cgscc(devirt<4>(openmp-opt-cgscc)),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(speculative-execution))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(libcalls-shrinkwrap))),cgscc(devirt<4>(function<eager-inv>(tailcallelim))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(reassociate))),cgscc(devirt<4>(function<eager-inv>(require<opt-remark-emit>))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(loop-instsimplify)))),cgscc(devirt<4>(function<eager-inv>(loop-simplifycfg))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<no-allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(simple-loop-unswitch<no-nontrivial;trivial>))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>))),cgscc(devirt<4>(function<eager-inv>(loop(loop-idiom)))),cgscc(devirt<4>(function<eager-inv>(loop(indvars)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-deletion)))),cgscc(devirt<4>(function<eager-inv>(loop(loop-unroll-full)))),cgscc(devirt<4>(function<eager-inv>(sroa<modify-cfg>))),cgscc(devirt<4>(function<eager-inv>(vector-combine))),cgscc(devirt<4>(function<eager-inv>(mldst-motion<no-split-footer-bb>))),cgscc(devirt<4>(function<eager-inv>(sccp))),cgscc(devirt<4>(function<eager-inv>(bdce))),cgscc(devirt<4>(function<eager-inv>(jump-threading))),cgscc(devirt<4>(function<eager-inv>(correlated-propagation))),cgscc(devirt<4>(function<eager-inv>(adce))),cgscc(devirt<4>(function<eager-inv>(memcpyopt))),cgscc(devirt<4>(function<eager-inv>(dse))),cgscc(devirt<4>(function<eager-inv>(loop-mssa(licm<allowspeculation>)))),cgscc(devirt<4>(function<eager-inv>(coro-elide))),cgscc(devirt<4>(function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>))),cgscc(devirt<4>(coro-split)),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-function-attrs,recompute-globalsaa,function<eager-inv>(float2int),function<eager-inv>(lower-constant-intrinsics),function<eager-inv>(loop(loop-deletion)),function<eager-inv>(loop-distribute),function<eager-inv>(inject-tli-mappings),function<eager-inv>(loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>),function<eager-inv>(loop-load-elim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>),function<eager-inv>(slp-vectorizer),function<eager-inv>(vector-combine),function<eager-inv>(loop-unroll<O2>),function<eager-inv>(transform-warning),function<eager-inv>(sroa<preserve-cfg>),function<eager-inv>(require<opt-remark-emit>),function<eager-inv>(loop-mssa(licm<allowspeculation>)),function<eager-inv>(alignment-from-assumptions),function<eager-inv>(loop-sink),function<eager-inv>(instsimplify),function<eager-inv>(div-rem-pairs),function<eager-inv>(tailcallelim),function<eager-inv>(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),verify"
        suffix=$(echo -n $passes | sha256sum | cut -c1-6)

        IFS=',' read -r -a items <<< "$passes"

        percentages_f=$suffix.$output_file.analysis.percentages
        passes_f=$suffix.$output_file.analysis.passes

        > $percentages_f
        > $passes_f

        mv $build_dir/out.ll $build_dir/preopt.ll
        while [ "${#items[@]}" -gt 0 ]; do
            break
            # Print the current array as comma-separated values
            seq=$(IFS=','; echo "${items[*]}")

            ## Backend
            exe $OPT $build_dir/preopt.ll -o $build_dir/out.ll -S --enable-new-pm=1 --passes=$seq # -print-pipeline-passes
            
            title_msg "ASPIS Profiling"
            exe $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libPROFILER.so --passes="aspis-insert-check-profile" $build_dir/out.ll -o $build_dir/out.ll -S
            success_msg "Code instrumented."

            exe $CLANG $clang_options $build_dir/out.ll $asm_files -o $build_dir/$output_file 
            success_msg "Instrumented binary emitted."

            exe $build_dir/$output_file
            success_msg "Profiled code executed."

            echo -e "Analyzing..."
            $OPT --enable-new-pm=1 -load-pass-plugin=$DIR/build/passes/libPROFILER.so --passes="aspis-check-profile" $build_dir/out.ll -o $build_dir/out.ll -S 2>> $percentages_f

            # add the pass to the list of analyzed passes
            echo ${items[-1]} >> $passes_f

            # Remove the last element
            unset 'items[-1]'
        done
        # emit the optimized binary
        exe $OPT $build_dir/preopt.ll -o $build_dir/preopt.ll -S --enable-new-pm=1 --passes=$passes # -print-pipeline-passes
        exe $CLANG $clang_options $build_dir/preopt.ll $asm_files -o $build_dir/$output_file 

        sed -E 's/.*\(([0-9]+)\).*/\1/' $percentages_f > tmpfile && mv tmpfile $percentages_f
        exit
    fi;
    exe $CLANG $clang_options $build_dir/out.ll $asm_files -o $build_dir/$output_file 
    success_msg "Binary emitted."

    #Cleanup
    if [[ $cleanup == true ]]; then
        rm -f $build_dir/*.ll
        success_msg "Cleaned cached files."
    fi

    success_msg "Done!"
}

parse_commands $@
perform_platform_checks $CLANG $OPT $LLVM_LINK
run_aspis