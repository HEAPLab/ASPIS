#!/usr/bin/env python3

import argparse
from pathlib import Path
import os
import signal
import sys
import shutil
import subprocess
import shlex
import tomllib
from collections.abc import Sequence

TESTING_DIR = Path(__file__).resolve().parent.parent
ASPIS_SCRIPT = TESTING_DIR.parent / "aspis.sh"
TEST_DIR = TESTING_DIR / "tests"
LLVM_CONFIG = TESTING_DIR / "config" / "llvm.toml"

# lldb exit codes set by the breakpoint commands on the handlers
DATA_EXIT_CODE = 10
SIG_EXIT_CODE = 11

DATA_DETECTED = "data corruption detected"
SIG_DETECTED = "signature mismatch detected"

# Same base techniques used by test.py, without the leading dashes
DATA_TECHNIQUES = ["no-dup", "eddi", "reddi", "seddi", "fdsc"]
CFC_TECHNIQUES = ["no-cfc", "cfcss", "rasm", "racfed", "inter-rasm"]

DETECTION_METHODS = ("data", "cfc")
FAULT_KEYS = ("name", "source_file", "break_at", "variable", "type", "value")


def run_command(
    command: Sequence[str],
    timeout: float | None = None,
    check: bool = False,
    verbose: bool = False,
    cwd: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    if verbose:
        print(shlex.join(str(x) for x in command))
    return subprocess.run(
        command,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
        timeout=timeout,
        check=check,
        cwd=cwd,
    )


def classify_result(result: subprocess.CompletedProcess[str]) -> str:
    if result.returncode == DATA_EXIT_CODE:
        return DATA_DETECTED
    if result.returncode == SIG_EXIT_CODE:
        return SIG_DETECTED
    if "stop reason = signal" in result.stdout:
        return "crash"
    if "exited with status = " in result.stdout:
        return "not detected"
    return "unknown"


def technique_flags(technique: str) -> str:
    """Turns 'eddi multiple-errbb' into '--eddi --multiple-errbb'."""
    return " ".join(f"--{word}" for word in technique.split())


def expected_outcomes(fault: dict, data_technique: str, cfc_technique: str) -> set[str]:
    """Returns the outcomes that count as a pass; an empty set means skip."""
    expected = set()
    if "data" in fault["detection_method"] and data_technique != "no-dup":
        expected.add(DATA_DETECTED)
    if "cfc" in fault["detection_method"] and cfc_technique != "no-cfc":
        expected.add(SIG_DETECTED)
    return expected


def load_symbols(nm_bin: str, binary: Path, verbose: bool) -> dict[str, str]:
    """Maps each demangled symbol name to its mangled name."""
    mangled_symbols = {}
    output = run_command(
        [nm_bin, "--defined-only", binary], check=True, verbose=verbose
    ).stdout
    for line in output.split("\n"):
        line = line.strip()
        if len(line) == 0:
            continue
        parts = line.split(" ")
        address = parts[0]
        mangled_name = " ".join(parts[2:])
        mangled_symbols[address] = mangled_name

    demangled_symbols = {}
    output = run_command(
        [nm_bin, "-C", "--defined-only", binary], check=True, verbose=verbose
    ).stdout
    for line in output.split("\n"):
        line = line.strip()
        if len(line) == 0:
            continue
        parts = line.split(" ")
        address = parts[0]
        demangled_name = " ".join(parts[2:])
        demangled_symbols[address] = demangled_name

    demangled_to_mangled_symbols = {}
    for address, demangled_name in demangled_symbols.items():
        if address not in mangled_symbols:
            continue
        demangled_to_mangled_symbols[demangled_name] = mangled_symbols[address]
    return demangled_to_mangled_symbols


def build_lldb_command(
    lldb_bin: str, binary: Path, fault: dict, symbols: dict[str, str]
) -> list[str]:
    for key in ("break_at", "variable"):
        if fault[key] not in symbols:
            raise KeyError(f"symbol '{fault[key]}' ({key}) not found in {binary}")
    mangled_break_at = symbols[fault["break_at"]]
    mangled_variable = symbols[fault["variable"]]

    return [
        lldb_bin,
        "--batch",
        "--no-lldbinit",
        "-o",
        "settings set target.disable-aslr false",
        "-o",
        "breakpoint set -n DataCorruption_Handler",
        "-o",
        f"breakpoint command add -o 'quit {DATA_EXIT_CODE}' 1",
        "-o",
        "breakpoint set -n SigMismatch_Handler",
        "-o",
        f"breakpoint command add -o 'quit {SIG_EXIT_CODE}' 2",
        "-o",
        f"breakpoint set -n {mangled_break_at}",
        "-o",
        "run",
        "-o",
        f"expression -l c -- *({fault['type']}*)&{mangled_variable} = {fault['value']}",
        "-o",
        "continue",
        "-k",
        "quit",
        "--",
        str(binary),
    ]


def kill_session(session_id: int):
    """Kills every process in the given session (Linux only)."""
    for stat_file in Path("/proc").glob("[0-9]*/stat"):
        try:
            stat = stat_file.read_text()
        except OSError:
            continue
        # fields after the command name: state, ppid, pgrp, session, ...
        fields = stat[stat.rfind(")") + 2 :].split()
        if int(fields[3]) == session_id:
            try:
                os.kill(int(stat_file.parent.name), signal.SIGKILL)
            except ProcessLookupError:
                pass


def inject(
    lldb_bin: str,
    binary: Path,
    fault: dict,
    symbols: dict[str, str],
    timeout: float,
    verbose: bool,
) -> str:
    command = build_lldb_command(lldb_bin, binary, fault, symbols)
    if verbose:
        print(shlex.join(command))

    # lldb runs the program through lldb-server, which puts it in its own
    # process group: on a hang, kill the whole session so none of them keeps
    # running in the background.
    with subprocess.Popen(
        command,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
        start_new_session=True,
    ) as process:
        try:
            output, _ = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            kill_session(process.pid)
            process.communicate()
            return "hang"

    if verbose:
        print(output)
    return classify_result(
        subprocess.CompletedProcess(command, process.returncode, output)
    )


def compile_with_aspis(
    source: Path, output: str, flags: str, llvm_bin: str, build_dir: Path, verbose: bool
) -> Path:
    # aspis.sh writes some csv files in the current directory: keep them in build_dir.
    # -g keeps the symbol names of internal globals, which are needed to find
    # the variables to corrupt.
    command = [
        ASPIS_SCRIPT,
        "--llvm-bin",
        llvm_bin,
        "-g",
        *flags.split(),
        source,
        "-o",
        output,
        "--build-dir",
        build_dir,
    ]
    result = run_command(command, verbose=verbose, cwd=build_dir)
    if verbose:
        print(result.stdout)
    if result.returncode != 0:
        raise RuntimeError(f"aspis.sh exited with code {result.returncode}")
    return build_dir / output


def load_faults(config: Path, only: list[str] | None) -> list[dict]:
    with open(config, "rb") as f:
        faults = tomllib.load(f).get("faults", [])

    names = set()
    for i, fault in enumerate(faults):
        missing = [key for key in FAULT_KEYS if key not in fault]
        if missing:
            raise ValueError(f"{config}: fault #{i} is missing {', '.join(missing)}")
        if fault["name"] in names:
            raise ValueError(f"{config}: duplicate fault name '{fault['name']}'")
        names.add(fault["name"])
        fault["source_file"] = TEST_DIR / fault["source_file"]
        fault.setdefault("detection_method", ["data"])
        for method in fault["detection_method"]:
            if method not in DETECTION_METHODS:
                raise ValueError(
                    f"{config}: fault '{fault['name']}' has unknown detection method '{method}'"
                )

    if only is not None:
        unknown = set(only) - names
        if unknown:
            raise ValueError(f"{config}: no fault named {', '.join(sorted(unknown))}")
        faults = [fault for fault in faults if fault["name"] in only]
    return faults


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="fault_injection",
        description="Compiles test programs with ASPIS and checks with lldb that "
        "injected faults trigger the expected handler",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--config",
        type=Path,
        help="toml file with the [[faults]] to run one after the other",
    )
    mode.add_argument(
        "--source",
        type=Path,
        help="single source file to check, with the fault described by the options below",
    )

    single = parser.add_argument_group("single fault (only with --source)")
    single.add_argument("--break-at", help="function where the fault is injected")
    single.add_argument("--variable", help="global variable to corrupt")
    single.add_argument("--type", help="C type of the variable (e.g. int)")
    single.add_argument("--value", help="value written into the variable")
    single.add_argument(
        "--detection-method",
        nargs="+",
        choices=DETECTION_METHODS,
        help="technique families expected to detect the fault (default: data)",
    )

    parser.add_argument(
        "--only",
        nargs="+",
        help="with --config, run only the faults with these names",
    )
    parser.add_argument(
        "--data",
        nargs="+",
        default=DATA_TECHNIQUES,
        help="data techniques to compile with, without leading dashes "
        "(e.g. 'eddi' or 'eddi multiple-errbb')",
    )
    parser.add_argument(
        "--cfc",
        nargs="+",
        default=CFC_TECHNIQUES,
        help="control-flow techniques to compile with, without leading dashes",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=TESTING_DIR / "build" / "fault_injection",
        help="where the hardened binaries are placed",
    )
    parser.add_argument(
        "-t",
        "--timeout",
        type=float,
        default=5.0,
        help="seconds after which a run of the executable is considered hung",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="print the commands being run and their output",
    )

    args = parser.parse_args()

    fault_options = ("break_at", "variable", "type", "value", "detection_method")
    if args.config is not None:
        given = [opt for opt in fault_options if getattr(args, opt) is not None]
        if given:
            parser.error(
                "--config cannot be used with "
                + ", ".join("--" + opt.replace("_", "-") for opt in given)
            )
        try:
            faults = load_faults(args.config, args.only)
        except (OSError, ValueError, tomllib.TOMLDecodeError) as e:
            parser.error(str(e))
    else:
        if args.only is not None:
            parser.error("--only can only be used with --config")
        missing = [opt for opt in fault_options[:-1] if getattr(args, opt) is None]
        if missing:
            parser.error(
                "--source requires "
                + ", ".join("--" + opt.replace("_", "-") for opt in missing)
            )
        if not args.source.is_file():
            parser.error(f"{args.source} does not exist or is not a regular file")
        faults = [
            {
                "name": args.source.stem,
                "source_file": args.source.resolve(),
                "break_at": args.break_at,
                "variable": args.variable,
                "type": args.type,
                "value": args.value,
                "detection_method": args.detection_method or ["data"],
            }
        ]

    lldb_bin = shutil.which("lldb")
    if lldb_bin is None:
        parser.error("No lldb found.")

    nm_bin = shutil.which("nm")
    if nm_bin is None:
        parser.error("No nm found.")

    with open(LLVM_CONFIG, "rb") as f:
        llvm_bin = tomllib.load(f)["llvm_bin"]

    build_dir = args.build_dir.resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    faults_by_source: dict[Path, list[dict]] = {}
    for fault in faults:
        faults_by_source.setdefault(fault["source_file"], []).append(fault)

    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}

    def report(fault: dict, data: str, cfc: str, outcome: str, status: str):
        counts[status] += 1
        print(f"{fault['name']:<30} {data:<10} {cfc:<10} {outcome:<30} {status}")

    for source, source_faults in faults_by_source.items():
        for data in args.data:
            for cfc in args.cfc:
                to_run = []
                for fault in source_faults:
                    expected = expected_outcomes(fault, data, cfc)
                    if expected:
                        to_run.append((fault, expected))
                    else:
                        report(fault, data, cfc, "-", "SKIP")
                if not to_run:
                    continue

                output = f"{source.stem}_{source.suffix[1:]}_{data}_{cfc}.out"
                output = output.replace(" ", "_")
                flags = f"{technique_flags(data)} {technique_flags(cfc)}"
                try:
                    binary = compile_with_aspis(
                        source, output, flags, llvm_bin, build_dir, args.verbose
                    )
                    symbols = load_symbols(nm_bin, binary, args.verbose)
                except (RuntimeError, subprocess.CalledProcessError) as e:
                    for fault, _ in to_run:
                        report(fault, data, cfc, "compile error", "FAIL")
                    print(f"  {e}")
                    continue

                for fault, expected in to_run:
                    try:
                        outcome = inject(
                            lldb_bin, binary, fault, symbols, args.timeout, args.verbose
                        )
                    except KeyError as e:
                        report(fault, data, cfc, "missing symbol", "FAIL")
                        print(f"  {e.args[0]}")
                        continue
                    report(
                        fault,
                        data,
                        cfc,
                        outcome,
                        "PASS" if outcome in expected else "FAIL",
                    )

    print()
    print(", ".join(f"{status}: {n}" for status, n in counts.items()))

    return 1 if counts["FAIL"] > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
