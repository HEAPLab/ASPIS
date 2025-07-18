import pytest

def pytest_addoption(parser):
    parser.addoption(
        "--use-container",
        action="store_true",
        default=False,
        help="Run tests inside a container",
    )

@pytest.fixture(scope="session")
def use_container(pytestconfig):
    return pytestconfig.getoption("use_container")