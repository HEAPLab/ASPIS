# Testing

This directory contains utilities and scripts for testing ASPIS.

## Local Testing

For local testing, use the `test.py` script. Configure your tests using `tests.toml`. Configure the llvm_bin flag in `llvm_bin.toml`.
Then run:
```bash
pytest test.py
```

> To run pytest the modules listed in requirements.txt must be installed.
> To install the modules:
> - directly install them globally with `pip install -r requirements.txt`
> - use a tool like conda
> - setup a python environment:
>   - `python -m venv env`
>   - launch environment: `source env/bin/activate`

### Writing a configuration file

Test config files must be `.toml` files with the following structure for each test:

```toml
[[tests]]
test_name = <name_for_test>
source_file = <relative_path_to_src_file>
expected_output = <output_expected>
aspis_options = <compilation_flags>
```

> `<relative_path_to_src_file>` is a relative path from `./tests/` folder 

### Flags

It is possible to write different configuration test files.

Additional flags are:
- `--suffix <version>` : Operates the same way as aspis, searching for binaries versions denoted by `<version>`.
- `--tests-file <path_to_config_file_1> <path_to_config_file_2> ...` : Use the configuration files specified.

## Docker Testing

You can also test ASPIS using Docker with the `test_docker_pipeline.py` script. This Pytest script uses Docker Compose to manage the container and execute ASPIS.

To set up and run Docker tests:

1.  **Build the Docker Image:** Follow the instructions in the [docker/README.md](docker/README.md) to build the ASPIS Docker image.

2.  **Build Configuration:** Run `configuration_builder.py` to generate the test configuration. Ensure you set the desired `BASE_DIR_DEFAULT` as this will update or rewrite `test_docker_pipeline.py`.

3.  **Run Tests:** Execute the `test_docker_pipeline.py` script:
    ```bash
    pytest test_docker_pipeline.py
    ```
