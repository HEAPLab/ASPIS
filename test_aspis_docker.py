import os
import subprocess
import pytest
import json

# Default configurations
ASPIS_SCRIPT = "./aspis.sh"  # Path to the ASPIS compilation script
TEST_DIR = "./tests/"  # Directory containing the test cases

# Load the test configuration
def load_config():
    with open("test_docker_config.json", "r") as file:
        return json.load(file)

# Utility functions
def run_command(command, cwd=None):
  """Runs a shell command and returns its output, error, and exit code.
  """
  process = subprocess.run(
    command, cwd=cwd, text=True, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
  )
  return process.stdout, process.stderr, process.returncode

def compile_with_aspis(source_file, output_file, options, build_dir):
  """Compile a file using docker compose + ASPIS with specified options.
  """
  command = f"docker compose run --rm aspis /workspace/{source_file} {options} -o {output_file} --build-dir ./{build_dir} --verbose"
  print(command)
  stdout, stderr, exit_code = run_command(command)
  if exit_code != 0:
    raise RuntimeError(f"Compilation failed: {stderr}")
  return stdout

def execute_binary(binary_file):
  """Execute the compiled binary inside the Docker container.
  """
  command = f"docker compose run --rm --entrypoint bash aspis -c '/workspace/{binary_file}'"
  stdout, stderr, exit_code = run_command(command)
  if exit_code != 0:
    raise RuntimeError(f"Execution failed: {stderr}")
  return stdout.strip()

# Tests
@pytest.mark.parametrize("test", load_config()["tests"])
def test_aspis_docker(test):
  """Run a single ASPIS test.
  """
  config = load_config()
  test_name = test["test_name"]
  build_dir = test_name
  source_file = test["source_file"]
  aspis_options = test["aspis_options"]
  expected_output = test["expected_output"]

  output_file = f"{test_name}.out"
  source_path = os.path.join(TEST_DIR, source_file)

  # Compile the source file
  compile_with_aspis(source_file=source_path, output_file=output_file, 
                     options=aspis_options, build_dir=build_dir)

  # Execute the binary and check output
  result = execute_binary(f"./{build_dir}/{output_file}")
  assert result == expected_output, f"Test {test_name} failed: {result}"

if __name__ == "__main__":
  pytest.main()
