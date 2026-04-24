import os
import subprocess
import pytest

import pytest_timeout
import tomllib

# Default configurations
ASPIS_SCRIPT = "../aspis.sh"  # Path to the ASPIS compilation script
TEST_DIR = "./tests"  # Directory containing the test cases

DOCKER_SHARED_VOLUME = "/workspace/ASPIS/tmp"
LOCAL_SHARED_VOLUME = "./tests/"
DOCKER_COMPOSE_FILE = "../docker/docker-compose.yml"

data_techniques = ["--no-dup", "--eddi", "--seddi", "--fdsc"]
cfc_techniques =   ["--no-cfc", "--cfcss", "--rasm", "--racfed", "--inter-rasm"]

# Load the test configuration
def load_config():
  assert os.path.exists("../aspis.sh") and "Cannot find aspis.sh, please run the tests from the ASPIS testing directory as `pytest test.py`"
  with open("config/llvm.toml", "rb") as f:
    config = tomllib.load(f)
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
  command = f"{ASPIS_SCRIPT} --llvm-bin {llvm_bin} {options} {source_file} -o {output_file}.out --build-dir ./{build_dir} --verbose"
  print(command)
  stdout, stderr, exit_code = run_command(command)
  if exit_code != 0:
    raise RuntimeError(f"[{output_file}] Compilation failed: {stderr}")
  return stdout

# Compile without ASPIS to get expected output
def compile_without_aspis(source_file, output_file, llvm_bin, build_dir):
  """Compile a file without ASPIS."""
  command = f"{llvm_bin}/clang++ {source_file} -o {build_dir}/{output_file}.out --verbose"
  print(command)
  stdout, stderr, exit_code = run_command(command)
  if exit_code != 0:
    raise RuntimeError(f"[{output_file}] Compilation failed: {stderr}")
  return stdout

def execute_binary(local_build_dir, test_name):
  """Execute the compiled binary and return its output."""
  binary_file = f"./{local_build_dir}/{test_name}.out";
  stdout, stderr, exit_code = run_command(binary_file)
  if exit_code != 0:
    raise RuntimeError(f"[{test_name}] Execution failed: {stderr}")
  return stdout.strip()

def pytest_generate_tests(metafunc):
    """Custom hook to parametrize tests based on the CLI --tests-file flag."""
    if "test_data" in metafunc.fixturenames:
        tests_file_paths = metafunc.config.getoption("--tests-file")
        test_list = []
        for file_path in tests_file_paths:
            with open(file_path, "rb") as f:
             config_data = tomllib.load(f)

            # Access the 'tests' list inside the TOML dictionary
            test_list.extend(config_data.get("tests", []))

            # Use 'test_name' for better output in the terminal
            ids = [t.get("test_name", str(i)) for i, t in enumerate(test_list)]

        metafunc.parametrize("test_data", test_list, ids=ids)
    
# Tests
@pytest.mark.timeout(60) # Set a timeout of 60 seconds for each test
@pytest.mark.parametrize("data_technique", data_techniques)
@pytest.mark.parametrize("cfc_technique", cfc_techniques)
def test_aspis(test_data, use_container, aspis_addopt, data_technique, cfc_technique):
  """Run a single ASPIS test."""
  config = load_config()
  llvm_bin = config["llvm_bin"]
  test_name = test_data["test_name"]
  source_file = test_data["source_file"]

  if("black_list" in test_data):
    black_list = test_data["black_list"]
    if(data_technique in black_list or cfc_technique in black_list):
      pytest.skip(f"Skipping {test_name} for {data_technique} and {cfc_technique} as it is in the black list")

  # use docker compose rather than ASPIS if --use-container is set
  if use_container:
    ASPIS_SCRIPT = f"docker compose -f {DOCKER_COMPOSE_FILE} run --rm aspis_runner"
    docker_build_dir = f"{DOCKER_SHARED_VOLUME}/build/{test_name}"
    local_build_dir = f"{DOCKER_SHARED_VOLUME}/build/{test_name}"
  else:
    #in this case they coincide
    docker_build_dir = "./build/test/"+test_name
    local_build_dir = "./build/test/"+test_name

  # Create build directory if it doesn't exist
  os.makedirs(local_build_dir, exist_ok=True)

  source_path = os.path.join(TEST_DIR, source_file)
  if not os.path.exists(docker_build_dir + "/" + test_name + ".out"):
    print("Compiling without ASPIS to get expected output...")
    compile_without_aspis(source_path, test_name, llvm_bin, docker_build_dir)

  print("Executing binary compiled without ASPIS...")
  expected_output = execute_binary(local_build_dir, test_name)
  print(f"Expected output: {expected_output}")

  aspis_options = aspis_addopt + " " + data_technique + " " + cfc_technique

  test_name_complete = f"{test_name}_{data_technique}_{cfc_technique}"

  # Compile the source file
  compile_with_aspis(source_path, test_name_complete, aspis_options, llvm_bin, docker_build_dir)

  # Execute the binary and check output
  result = execute_binary(local_build_dir, test_name_complete)
  assert result == expected_output, f"Test {test_name_complete} failed: {result}"

if __name__ == "__main__":
  pytest.main()
