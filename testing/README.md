# Testing

This directory contains utilities and scripts for testing ASPIS.

## Local Testing

For local testing, use the `test.py` script. Configure your tests using `test_config.json`.

```bash
pytest test.py
```

## Docker Testing

You can also test ASPIS using Docker with the `test_docker_pipeline.py` script. This Pytest script uses Docker Compose to manage the container and execute ASPIS.

To set up and run Docker tests:

1.  **Build the Docker Image:** Follow the instructions in the [docker/README.md](docker/README.md) to build the ASPIS Docker image.

2.  **Build Configuration:** Run `configuration_builder.py` to generate the test configuration. Ensure you set the desired `BASE_DIR_DEFAULT` as this will update or rewrite `test_docker_pipeline.py`.

3.  **Run Tests:** Execute the `test_docker_pipeline.py` script:
    ```bash
    pytest test_docker_pipeline.py
    ```