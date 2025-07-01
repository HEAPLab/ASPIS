# Docker Directory

This directory contains the necessary files to build and run ASPIS within a Docker environment. The `docker-compose.yaml` defines the aspis_runner service, which is structured to support CLI usage of the container. It mounts the `testing/tests/` directory as a shared volume, allowing ASPIS to access files from there and place compiled binaries back into that location.

## Example Usage

To run ASPIS using Docker Compose, you can execute the following command from the CLI (from the docker directory):

```bash
docker compose run --rm aspis_runner /workspace/ASPIS/tmp/c/control_flow/function_pointer.c -o /tmp/build/function_pointer.out
```