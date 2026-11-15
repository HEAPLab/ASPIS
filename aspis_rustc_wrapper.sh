#!/usr/bin/env bash
#
# RUSTC_WRAPPER that hardens the binary crates of a cargo project with aspis.sh:
#   ASPIS_FLAGS="--reddi --no-cfc" RUSTC_WRAPPER=/path/to/aspis_rustc_wrapper.sh cargo build ...

set -euo pipefail

DIR=$(cd -P "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" >/dev/null 2>&1 && pwd)

RUSTC="$1"
shift

src=""
crate_name=""
crate_types=""
out_dir=""
extra_filename=""
deps_dir=""

aspis_rustc_options=()
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
    arg="${args[i]}"
    next="${args[i + 1]:-}"
    case "$arg" in
        --crate-name) crate_name="$next" ;;
        --crate-type) crate_types+=",$next" ;;
        --out-dir) out_dir="$next"; ((++i)); continue ;;
        --emit=*) continue ;;
        -C) [[ "$next" == extra-filename=* ]] && { extra_filename="${next#extra-filename=}"; ((++i)); continue; } ;;
        # With --target, cargo passes the target's dependency directory first, then the host's
        -L) [[ "$next" == dependency=* && -z "$deps_dir" ]] && deps_dir="${next#dependency=}" ;;
        *.rs) [[ -f "$arg" ]] && { src="$arg"; continue; } ;;
    esac
    aspis_rustc_options+=("$arg")
done

if [[ -z "$src" || ",$crate_types," != *",bin,"* || "$crate_name" == build_script_* ]]; then
    exec "$RUSTC" "$@"
fi
if [[ -z "$crate_name" || -z "$out_dir" || -z "$deps_dir" ]]; then
    echo "aspis_rustc_wrapper: cannot find --crate-name/--out-dir/-L dependency= in: $*" >&2
    exit 1
fi

"$RUSTC" "$@"

# aspis.sh's output goes to stderr, where cargo shows it as rustc's output
exec 1>&2

stem="$out_dir/$crate_name$extra_filename"
mkdir -p "$stem.aspis"
rlibs=("$deps_dir"/*.rlib)

# shellcheck disable=SC2086 # ASPIS_FLAGS is a list of options
"$DIR/aspis.sh" ${ASPIS_FLAGS:-} \
    --build-dir "$stem.aspis" -o "$crate_name" "$src" "${rlibs[@]}" \
    -- "${aspis_rustc_options[@]}"

mv "$stem.aspis/$crate_name" "$stem"
