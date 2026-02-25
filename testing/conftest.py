import pytest
import os

def pytest_addoption(parser):
    parser.addoption(
        "--use-container",
        action="store_true",
        default=False,
        help="Run tests inside a container",
    )
    parser.addoption(
        "--tests-file",
        action="store",
        nargs="+",
        default=["config/tests.toml"],
        help="Path to the configuration file",
    )
    parser.addoption(
        "--suffix",
        action="store",
        help="LLVM version suffix",
    )

@pytest.fixture(scope="session")
def use_container(pytestconfig):
    return pytestconfig.getoption("use_container")

# Optional: Add a check to ensure the file exists before any tests start
def pytest_configure(config):
    tests_file_paths = config.getoption("--tests-file")
    for file_path in tests_file_paths:
        if not os.path.exists(file_path):
            pytest.exit(f"Config file not found: {file_path}")

@pytest.fixture(scope="session")
def aspis_addopt(pytestconfig):
    addopt = {}
    suffix_opt = pytestconfig.getoption("--suffix")
    if suffix_opt is None:
        addopt["suffix"] = ""
    else:
        addopt["suffix"] = "--suffix " + suffix_opt

    return addopt["suffix"]
