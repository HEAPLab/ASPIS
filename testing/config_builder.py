"""
This script generates a JSON test configuration file for the ASPIS automated test suite.

It recursively scans BASE_DIR_DEFAULT directory for `.c` source files, extracts expected outputs
from special comment annotations, and produces multiple test cases for each file using
different ASPIS instrumentation options as listed in ASPIS_VARIANTS.

Expected output format in the source file, add these line at the end of the file.
    // expected output
    // <value>

NOTE: pytest expects exact match, unmatched spaces or newlines will trigger an error.    
"""

import os
import json
import argparse


# === Builder Options ===
BASE_DIR_DEFAULT = "./tests/c/"
OUTPUT_FILE_DEFAULT = "docker_test_config.json"

ASPIS_VARIANTS = [
    "--eddi --cfcss",
    "--seddi --rasm",
    "--fdsc --rasm",
    "--eddi --no-cfc"
]


def parse_expected_output(filepath):
    with open(filepath, "r") as f:
        lines = f.readlines()
    for i in range(len(lines) - 1):
        if lines[i].strip().lower() == "// expected output":
            return lines[i + 1].strip().lstrip("//").strip()
    return ""

def collect_source_files(base_dir):
    source_files = []
    for root, _, files in os.walk(base_dir):
        for file in files:
            if file.endswith(".c"):
                full_path = os.path.join(root, file)
                source_files.append(full_path)
    return sorted(source_files)

def confirm_settings(base_dir, output_file, source_files):
    print("\n📁 Base Directory:")
    print(f"  {base_dir}")

    print("\n📝 Output Config File:")
    print(f"  {output_file}")

    print("\n📦 C Source Files Found:")
    for f in source_files:
        print(f"  - {f}")

    return input("\nProceed with generating test configuration? [y/N]: ").strip().lower() == "y"

def generate_tests(source_files, base_dir):
    tests = []
    for source_file in source_files:
        expected_output = parse_expected_output(source_file)
        if not expected_output:
            print(f"[!] Warning: No expected output found in {source_file}. Skipping.")
            continue
        base_name = os.path.splitext(os.path.basename(source_file))[0]
        relative_path = os.path.relpath(source_file, "./tests/")

        for i, opts in enumerate(ASPIS_VARIANTS):
            test_name = f"{base_name}_v{i}"
            tests.append({
                "test_name": test_name,
                "source_file": relative_path,
                "expected_output": expected_output,
                "aspis_options": opts
            })
    return tests

def load_existing_tests(output_file):
    if not os.path.exists(output_file):
        return []
    with open(output_file, "r") as f:
        try:
            return json.load(f).get("tests", [])
        except json.JSONDecodeError:
            print(f"[!] Warning: Could not parse existing JSON. Ignoring contents of {output_file}")
            return []

def merge_tests(existing, new):
    existing_by_name = {t["test_name"]: t for t in existing}
    added = 0
    updated = 0
    overwrite_all = False

    for test in new:
        name = test["test_name"]
        if name not in existing_by_name:
            existing_by_name[name] = test
            added += 1
        else:
            if overwrite_all:
                existing_by_name[name] = test
                updated += 1
                continue

            print(f"\n⚠️ Test '{name}' already exists.")
            print(f"Existing: {existing_by_name[name]}")
            print(f"New     : {test}")
            choice = input("  [s]kip / [o]verwrite / [oa]overwrite all / [a]bort / [k]eep all existing: ").strip().lower()

            if choice == "o":
                existing_by_name[name] = test
                updated += 1
            elif choice == "oa":
                overwrite_all = True
                existing_by_name[name] = test
                updated += 1
            elif choice == "a":
                print("Aborted.")
                exit(1)
            elif choice == "k":
                return list(existing_by_name.values()), added, updated
            else:
                continue

    return list(existing_by_name.values()), added, updated


def preview_tests(tests):
    print("\n🔍 Preview of First 3 Generated Tests:")
    for t in tests[:3]:
        print(json.dumps(t, indent=2))
    if len(tests) > 3:
        print(f"...and {len(tests) - 3} more tests.\n")
    else:
        print()
    return input("Continue and save this config? [y/N]: ").strip().lower() == "y"

def write_json(tests, output_file):
    with open(output_file, "w") as f:
        json.dump({"tests": tests}, f, indent=2)
    print(f"\n✅ Saved {len(tests)} total tests to '{output_file}'")

def main():
    parser = argparse.ArgumentParser(
        description="🔧 Generate or update docker_test_config.json for ASPIS automated test suite.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "-d", "--dir",
        default=BASE_DIR_DEFAULT,
        help="Base directory to search for .c files (default: ./tests/c)"
    )
    parser.add_argument(
        "-o", "--output",
        default=OUTPUT_FILE_DEFAULT,
        help="Output JSON file name (default: docker_test_config.json)"
    )

    args = parser.parse_args()
    print("\n=== ASPIS Test Configuration Generator ===")

    source_files = collect_source_files(args.dir)
    if not source_files:
        print(f"\n❌ No .c files found in directory: {args.dir}")
        return

    if not confirm_settings(args.dir, args.output, source_files):
        print("Aborted.")
        return

    new_tests = generate_tests(source_files, args.dir)
    if not new_tests:
        print("\n❌ No valid tests generated. Ensure expected output comments exist.")
        return

    if not preview_tests(new_tests):
        print("Aborted.")
        return

    existing_tests = load_existing_tests(args.output)
    merged_tests, added, updated = merge_tests(existing_tests, new_tests)
    write_json(merged_tests, args.output)

    print(f"➕ Added: {added}, 🔁 Updated: {updated}, 📊 Total: {len(merged_tests)}")

if __name__ == "__main__":
    main()
