# Testing 

This directory contains utilities and scripts for testing ASPIS.

***Local***

- `test.py`: Pytest script for local testing.
- `test_config.json`: Configuration file for local tests.


***Docker***

- `test_docker_pipeline.py`: Pytest script to execute ASPIS tests within a Docker Compose environment.
- `docker_test_config`: Configuration for `test_docker_pipeline.py`.


***Utils***

- `configuration_builder.py`: minimal CLI tool for generating testing configurations. 