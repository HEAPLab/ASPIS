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
lib_files=""
input_files=""
clang_options=
rustc_options=()
eddi_options="-S"
cfc_options="-S"
llvm_bin=$(dirname "$(which clang)")
suffix=""
build_dir="."
dup=0 # 0 = eddi,   1 = seddi,  2 = fdsc
cfc=0 # 0 = cfcss,  1 = rasm,   2 = inter-rasm
debug_enabled=false
verbose=false
cleanup=true
cpp_input=false
rust_input=false
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
        error_msg "\nCommand clang not found. Expected path: ${1}. Please check --llvm-bin parameter."
    fi

    if [ ! -f $2 ]; then
        error_msg "\nCommand opt not found. Expected path: ${2}. Please check --llvm-bin parameter."
    fi

    if [ ! -f $3 ]; then
        error_msg "\nCommand llvm-link not found. Expected path: ${3}. Please check --llvm-bin parameter."
    fi
    
}


parse_commands() {

    ## Everything after `--` is passed verbatim to rustc. It is kept as an array, unlike the other
    ## options, so that arguments containing spaces (e.g. `--check-cfg 'cfg(feature, values(...))'`)
    ## survive.
    local aspis_opts=()
    while (($# > 0)); do
        if [[ "$1" == "--" ]]; then
            shift
            rustc_options=("$@")
            break
        fi
        aspis_opts+=("$1")
        shift
    done

    raw_opts="${aspis_opts[*]}"

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

    Usage: aspis.sh [options] file(s)... [-- rustc-options...]

    The specified files can be any C, C++, or Rust source code files, or LLVM IR
    files (.ll/.bc) that are linked in as they are, skipping the front-end.
    By default, the compiler performs EDDI+CFCSS hardening.

    Options:
        -h, --help          Display available options.
        -v, --verbose       Turn on verbose mode
        -o <file>           Write the compilation output to <file>.
        --build-dir <path>  Specify the directory where to place all the build
                            files.
        --llvm-bin  <path>  Set the path to the llvm binaries (clang, opt, 
                            llvm-link) to <path>.
        --suffix    <value> Set the suffix of the binaries used (clang, opt, 
                            llvm-link) to <value>.
        --exclude   <file>  Set the files to exclude from the compilation. The 
                            content of <file> is the list of files to 
                            exclude, one for each line (wildcard * allowed).
        --asmfiles  <file>  Defines the set of assembly files required for the
                            compilation. The content of <file> is the list of 
                            assembly files to pass to the linker at compilation 
                            termination, one for each line (wildcard * allowed).   
        --no-cleanup        Does not remove the intermediate .ll files generated,
                            which might be useful for debug purposes.

    Hardening mechanism:
        --eddi              (Default) Enable EDDI.
        --reddi             Enable Recursive-EDDI.
        --seddi             Enable Selective-EDDI.
        --fdsc              Enable Full Duplication with Selective Checking.
        --no-dup            Completely disable data duplication.

        --cfcss             (Default) Enable CFCSS.
        --rasm              Enable RASM.
        --inter-rasm        Enable inter-RASM with the default signature -0xDEAD.
        --racfed            Enable RACFED.
        --no-cfc            Completely disable control-flow checking.

    Hardening options:
        --alternate-memmap  When set, alternates the definition of original and 
                            duplicate variables. By default they are allocated in 
                            groups maximizing the distance between original and
                            duplicated value.

        --enable-profiling  When set, enable the insertion of profiling function calls 
                            at synchonization points, which can be used to trace where
                            consistency checks are executed.

        --multiple-errbb    Enables multiple error basic blocks in EDDI

        --coarse-grained    Enables coarse-grained duplication in EDDI

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
                            output_file=${opt##"-o"};
                        fi;
                        ;;
                    --llvm-bin*)
                        if [[ ${#opt} -eq 10 ]]; then
                            parse_state=3;
                        else
                            llvm_bin=${opt##"--llvm-bin="};
                        fi;
                        ;;
                    --suffix*)
                        if [[ ${#opt} -eq 8 ]]; then
                            parse_state=7;
                        else
                            suffix='-';
                            suffix+=${opt##"--suffix="};
                        fi;
                        ;;
                    --exclude*)
                        if [[ ${#opt} -eq 9 ]]; then
                            parse_state=4;
                        else
                            exclude_file=$(echo "$opt" | cut -b 9);
                        fi;
                        ;;
                    --asmfiles*)
                        if [[ ${#opt} -eq 10 ]]; then
                            parse_state=5;
                        else
                            asm_file=$(echo "$opt" | cut -b 10);
                        fi;
                        ;;
                    --build-dir*)
                        if [[ ${#opt} -eq 11 ]]; then
                            parse_state=6;
                        else
                            build_dir=$(echo "$opt" | cut -b 10);
                        fi;
                        ;;
                    --eddi)
                        dup=0
                        ;;
                    --reddi)
                        dup=3
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
                    --racfed)
                        cfc=3
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
                    --multiple-errbb)
                        eddi_options="$eddi_options -multiple-errbb=true";
                        ;;
                    --coarse-grained)
                        eddi_options="$eddi_options -coarse-grained=true";
                        ;;
                    -g)
                        debug_enabled=true;
                        clang_options="$clang_options $opt";
                        eddi_options="$eddi_options --debug-enabled=true";
                        ;;
                    --no-cleanup)
                        cleanup=false;
                        ;;
                    *.c | *.cpp)
                        input_files="$input_files $opt";
                        if [[ "$opt" == *.cpp ]]; then
                            cpp_input=true
                        fi
                        ;;
                    *.rs)
                        input_files="$input_files $opt";
                        rust_input=true;
                        ;;
                    *.ll | *.bc)
                        input_files="$input_files $opt";
                        ;;
                    *.a | *.rlib)
                        lib_files="$lib_files $opt";
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
            7)
              suffix="-$opt";
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

    CLANG="${llvm_bin}/clang${suffix}"
    CLANGXX="${llvm_bin}/clang++${suffix}"
    OPT="${llvm_bin}/opt${suffix}"
    LLVM_LINK="${llvm_bin}/llvm-link${suffix}"
    RUSTC="rustc"

    if [[ -n "$config_file" ]]; then
        CLANG="${CLANG} --config ${config_file}"
        CLANGXX="${CLANGXX} --config ${config_file}"
    fi;

    if [[ "$cpp_input" == true ]]; then
        LINKER=$CLANGXX
    else
        LINKER=$CLANG
    fi
}

run_aspis() {
    if [[ -z ${input_files} ]]; then
        error_msg "No input files provided."
    fi

    exe mkdir -p $build_dir

    ## Intermediate .ll files emitted by the frontend, linked into out.ll
    ll_files=()
    excluded_ll_files=()
    ## IR files given as input, linked as they are (and never removed by the cleanup)
    ir_input_files=()

    title_msg "Front-end and pre-processing"

    ## Rust sources may use the #[aspis(...)] macro from the rust-annotations crate. It only emits
    ## the annotations when compiled with `--cfg aspis`, so a plain rustc build ignores them.
    rust_annotations_lib="$DIR/rust-annotations/target/release/libaspis_rust_annotations.so"
    rust_annotations_extern=(--extern aspis_rust_annotations="$rust_annotations_lib")
    if [[ " ${rustc_options[*]} " == *" aspis_rust_annotations="* ]]; then
        rust_annotations_extern=()
    elif [[ "$rust_input" == true && ! -f "$rust_annotations_lib" ]]; then
        error_msg "Rust annotations crate not found at $rust_annotations_lib. Build it with 'cargo build --release' in $DIR/rust-annotations."
    fi

    ## FRONTEND
    for input_file in $input_files; do
        if [[ "$input_file" == *.ll || "$input_file" == *.bc ]]; then
            ir_input_files+=("$input_file")
            continue
        fi
        # Extract the filename without extension
        filename=$(basename "$input_file" | sed 's/\.[^.]*$//')
        # Compile the file to LLVM IR (.ll) and save it in the build directory
        if [[ "$input_file" == *.rs ]]; then
            ## Defaults, unless the user passed their own after `--`
            rustc_defaults=()
            if [[ " ${rustc_options[*]} " != *" --crate-type"* ]]; then
                rustc_defaults+=(--crate-type=bin)
            fi
            if [[ " ${rustc_options[*]} " != *" --edition"* ]]; then
                rustc_defaults+=(--edition 2024)
            fi

            exe $RUSTC \
                "${rustc_defaults[@]}" \
                "${rustc_options[@]}" \
                --cfg aspis \
                --check-cfg 'cfg(aspis)' \
                "${rust_annotations_extern[@]}" \
                -C opt-level=0 \
                -C panic=abort \
                --emit=llvm-ir \
                "$input_file" -o "$build_dir/$filename.ll"
        else
            exe $CLANG "$input_file" $clang_options -S -emit-llvm -O0 -Xclang -disable-O0-optnone -o "$build_dir/$filename.ll"
        fi
        ll_files+=("$build_dir/$filename.ll")
    done

    ## Rust programs reference symbols from Rust's standard library, so the final link must pull in
    ## the toolchain's libstd.
    rust_link_flags=""
    if [[ "$rust_input" == true ]]; then
        rust_lib_dir="$($RUSTC --print target-libdir)"
        rust_std_lib="$(basename "$(ls "$rust_lib_dir"/libstd-*.so | head -n 1)" .so)"
        rust_link_flags="-L$rust_lib_dir -Wl,-rpath,$rust_lib_dir -l${rust_std_lib#lib}"
    fi

    ## LINK & PREPROCESS
    exe $LLVM_LINK "${ll_files[@]}" "${ir_input_files[@]}" -o $build_dir/out.ll

    success_msg "Emitted and linked IR."

    ## Explicitly use the Rust annotation bridge before anything else.
    exe $OPT -load-pass-plugin=$DIR/build/passes/libRUSTBRIDGE.so --passes="aspis-rust-annotation-bridge" $build_dir/out.ll -o $build_dir/out.ll
    success_msg "Translated Rust annotations."

    if [[ $debug_enabled == false ]]; then
        exe $OPT --passes="strip" $build_dir/out.ll -o $build_dir/out.ll
        echo "  Debug mode disabled, stripped debug symbols."
    fi
    
    exe $OPT --passes="lower-switch" $build_dir/out.ll -o $build_dir/out.ll

    ## FuncRetToRef
    if [[ dup -ne -1 ]]; then
        exe $OPT -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="func-ret-to-ref" $build_dir/out.ll -o $build_dir/out.ll
    fi;

    title_msg "ASPIS transformations"
    ## DATA PROTECTION
    case $dup in
        0) 
            exe $OPT -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        1) 
            exe $OPT -load-pass-plugin=$DIR/build/passes/libSEDDI.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        2) 
            exe $OPT -load-pass-plugin=$DIR/build/passes/libFDSC.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        3)
            exe $OPT -load-pass-plugin=$DIR/build/passes/libREDDI.so --passes="eddi-verify" $build_dir/out.ll -o $build_dir/out.ll $eddi_options
            ;;
        *)
            echo -e "\t--no-dup specified!"
    esac
    success_msg "Applied data protection passes."

    ## simplifycfg can fold chains of comparisons back into switches, which the CFC passes
    ## do not support, so lower them again.
    exe $OPT --passes="simplifycfg,lower-switch" $build_dir/out.ll -o $build_dir/out.ll

    ## CONTROL-FLOW CHECKING
    case $cfc in
        0) 
            exe $OPT -load-pass-plugin=$DIR/build/passes/libCFCSS.so --passes="cfcss-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
            ;;
        1) 
            exe $OPT -load-pass-plugin=$DIR/build/passes/libRASM.so --passes="rasm-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
            ;;
        2) 
            exe $OPT -load-pass-plugin=$DIR/build/passes/libINTER_RASM.so --passes="rasm-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
            ;;
        3)
            exe $OPT -load-pass-plugin=$DIR/build/passes/libRACFED.so --passes="racfed-verify" $build_dir/out.ll -o $build_dir/out.ll $cfc_options
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
        exe rm -f "${ll_files[@]}"
        exe mv $build_dir/out.ll.bak $build_dir/out.ll
        for input_file in $excluded_files; do
            # Extract the filename without extension
            filename=$(basename "$input_file" | sed 's/\.[^.]*$//')
            # Compile the file to LLVM IR (.ll) and save it in the build directory
            exe $CLANG "$input_file" $clang_options -S -emit-llvm -Xclang -disable-O0-optnone -o "$build_dir/$filename.ll"
            excluded_ll_files+=("$build_dir/$filename.ll")
        done
        exe $LLVM_LINK $build_dir/out.ll "${excluded_ll_files[@]}" -o $build_dir/out.ll
    fi;
    success_msg "Linked excluded files to the compilation."

    ## DuplicateGlobals
    if [[ dup -ne -1 ]]; then
        exe $OPT -load-pass-plugin=$DIR/build/passes/libEDDI.so --passes="duplicate-globals" $build_dir/out.ll -o $build_dir/out.ll -S $eddi_options
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

    ## Libraries go after out.ll
    ## The --start-group/--end-group lets them depend on each other in any order.
    lib_link_flags=""
    if [[ -n "$lib_files" ]]; then
        lib_link_flags="-Wl,--start-group $lib_files -Wl,--end-group"
    fi

    ## Backend
    exe $OPT $build_dir/out.ll -o $build_dir/out.ll -S $opt_flags
    if [[ "$enable_profiling" == "true" ]]; then
        title_msg "ASPIS Profiling"
        exe $OPT -load-pass-plugin=$DIR/build/passes/libPROFILER.so --passes="aspis-insert-check-profile" $build_dir/out.ll -o $build_dir/out.ll -S
        success_msg "Code instrumented."

        exe $LINKER $clang_options $build_dir/out.ll $asm_files $lib_link_flags $rust_link_flags -o $build_dir/$output_file
        success_msg "Instrumented binary emitted."

        exe $build_dir/$output_file
        success_msg "Profiled code executed."

        echo -e "Analyzing..."
        exe $OPT -load-pass-plugin=$DIR/build/passes/libPROFILER.so --passes="aspis-check-profile" $build_dir/out.ll -o $build_dir/out.ll -S
        exit
    fi;

    case $output_file in 
        *.ll)
            exe cp $build_dir/out.ll $build_dir/$output_file.bak
            ;;
        *)
            exe $LINKER $clang_options $build_dir/out.ll $asm_files $lib_link_flags $rust_link_flags -o $build_dir/$output_file
            ;;
    esac
    success_msg "Binary emitted."

    #Cleanup
    if [[ $cleanup == true ]]; then
        rm -f "${ll_files[@]}" "${excluded_ll_files[@]}" $build_dir/out.ll
        success_msg "Cleaned cached files."
    fi

    case $output_file in 
        *.ll)
            exe mv $build_dir/$output_file.bak $build_dir/$output_file
            ;;
    esac

    success_msg "Done!"
}

parse_commands "$@"
perform_platform_checks $CLANG $OPT $LLVM_LINK
if [[ "$rust_input" == true ]] && ! command -v "$RUSTC" >/dev/null 2>&1; then
    error_msg "\nCommand rustc not found on PATH. Rust source files require rustc."
fi
run_aspis
