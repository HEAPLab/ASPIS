import os
import subprocess
import pytest
import tomllib

# Default configurations
ASPIS_SCRIPT = "../aspis.sh"  # Path to the ASPIS compilation script
TEST_DIR = "./tests"  # Directory containing the test cases

DOCKER_SHARED_VOLUME = "/workspace/ASPIS/tmp"
LOCAL_SHARED_VOLUME = "./tests/"
DOCKER_COMPOSE_FILE = "../docker/docker-compose.yml"

# Load the test configuration
def load_config():
  assert os.path.exists("../aspis.sh") and "Cannot find aspis.sh, please run the tests from the ASPIS testing directory as `pytest test.py`"
  with open("config/llvm.toml", "rb") as f:
    config = tomllib.load(f)
  with open("config/tests.toml", "rb") as f:
    tests = tomllib.load(f)
  config.update(tests)  # Merge the dictionaries
  return config

# Utility functions
def run_command(command, cwd=None):
  """Runs a shell command and returns its output, error, and exit code."""
  process = subprocess.run(
    command, cwd=cwd, text=True, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
  )
  return process.stdout, process.stderr, process.returncode

def compile_with_aspis(source_file, output_file, options, llvm_bin, build_dir):
  """Compile a file using ASPIS with specified options."""
  command = f"{ASPIS_SCRIPT} --llvm-bin {llvm_bin} {options} {source_file} -o {output_file} --build-dir ./{build_dir} --verbose"
  print(command)
  stdout, stderr, exit_code = run_command(command)
  if exit_code != 0:
    raise RuntimeError(f"[{test_name}] Compilation failed: {stderr}")
  return stdout

def execute_binary(binary_file):
  """Execute the compiled binary and return its output."""
  stdout, stderr, exit_code = run_command(binary_file)
  if exit_code != 0:
    raise RuntimeError(f"[{test_name}] Execution failed: {stderr}")
  return stdout


# Tests
@pytest.mark.parametrize("test", load_config()["tests"])
def test_aspis(test, use_container):
  """Run a single ASPIS test."""
  global test_name
  config = load_config()
  llvm_bin = config["llvm_bin"]
  test_name = test["test_name"]
  source_file = test["source_file"]
  aspis_options = test["aspis_options"]
  expected_output = test["expected_output"]
  # use docker compose rather than ASPIS if --use-container is set
  if use_container:
    ASPIS_SCRIPT = f"docker compose -f {DOCKER_COMPOSE_FILE} run --rm aspis_runner"
    docker_build_dir = f"{DOCKER_SHARED_VOLUME}/build/{test_name}"
    local_build_dir = f"{DOCKER_SHARED_VOLUME}/build/{test_name}"
  else:
    #in this case they coincide
    docker_build_dir = "./build/test/"+test_name
    local_build_dir = "./build/test/"+test_name


  output_file = f"{test_name}.out"
  source_path = os.path.join(TEST_DIR, source_file)

  # Compile the source file
  compile_with_aspis(source_path, output_file, aspis_options, llvm_bin, docker_build_dir)

  # Execute the binary and check output
  result = execute_binary(f"./{local_build_dir}/{output_file}")
  assert result == expected_output, f"Test {test_name} failed: {result}"

if __name__ == "__main__":
  pytest.main()
