import os
import subprocess
import pytest
import json

# Configuration
ASPIS_SCRIPT = "./aspis.sh"  # Not used in current code, can be removed
TEST_DIR = "./tests/"
DOCKER_SHARED_VOLUME = "/workspace/ASPIS/tmp"
LOCAL_SHARED_VOLUME = "./tests/"
DOCKER_COMPOSE_FILE = "../docker/docker-compose.yml"
CONFIG_FILE = "docker_test_config.json"

# Load test configuration
def load_config():
    with open(CONFIG_FILE, "r") as file:
        return json.load(file)

# Utility function to run shell commands
def run_command(command, cwd=None):
    process = subprocess.run(
        command, cwd=cwd, text=True, shell=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    return process.stdout, process.stderr, process.returncode

# Compile a file using ASPIS with docker-compose
def compile_with_aspis(source_file, output_file, aspis_options, build_dir):
    command = (
        f"docker compose -f {DOCKER_COMPOSE_FILE} run --rm aspis_runner {source_file} {aspis_options} -o {output_file} --build-dir {build_dir} --verbose"
    )
    print(f"[DEBUG] Running: {command}")
    stdout, stderr, exit_code = run_command(command, cwd=os.path.dirname(DOCKER_COMPOSE_FILE))
    if exit_code != 0:
        raise RuntimeError(f"Compilation failed: {stderr}")
    return stdout

# Execute compiled binary and return output
def execute_binary(binary_file, test_name):
    stdout, stderr, exit_code = run_command(binary_file)
    if exit_code != 0:
        raise RuntimeError(f"[{test_name}] Execution failed: {stderr}")
    return stdout

@pytest.mark.parametrize("test", load_config()["tests"])
def test_aspis(test):
    test_name = test["test_name"]
    docker_build_dir = f"{DOCKER_SHARED_VOLUME}/build/{test_name}"
    local_build_dir = f"{LOCAL_SHARED_VOLUME}/build/{test_name}"
    source_file = os.path.join(DOCKER_SHARED_VOLUME, test["source_file"])
    aspis_options = test["aspis_options"]
    expected_output = test["expected_output"]

    output_file = f"{test_name}.out"

    # Compile the source file
    print(f"DEBUG: {source_file}")
    aspis_stdout = compile_with_aspis(
        source_file=source_file,
        output_file=output_file,
        aspis_options=aspis_options,
        build_dir=docker_build_dir
    )
    print(f"[DEBUG] Compilation output: {aspis_stdout}")

    # Execute the binary and validate output
    result = execute_binary(f"./{local_build_dir}/{output_file}", test_name)
    assert result == expected_output, f"Test {test_name} failed: {result}"

if __name__ == "__main__":
    pytest.main()
