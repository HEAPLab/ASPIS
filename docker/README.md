# Docker Directory

This directory contains the necessary files to build and run ASPIS within a Docker environment. The `docker-compose.yaml` defines the aspis_runner service, which is structured to support CLI usage of the container. It mounts the `testing/tests/` directory as a shared volume, allowing ASPIS to access files from there and place compiled binaries back into that location.

## Building and Running


### _Building the Docker image_

To build the aspis Docker image, navigate to this directory and execute the following command:

```bash
docker build -t aspis .
```

### _Running ASPIS with Docker Compose_

- Docker compose features:
    - **shared volume**: ` .././testing/tests/:/workspace/ASPIS/tmp/`: Mounts the host's testing/tests/ directory (relative to the docker directory) to /workspace/ASPIS/tmp/ inside the container. This allows ASPIS to access input files and store output files in a shared location.
    - **entrypoint**: `aspis.sh` with predefined `llvm-bin` will be executed with the provided arguments


To run ASPIS using Docker Compose, execute the following command:

```bash
docker compose run --rm aspis_runner /workspace/ASPIS/tmp/c/control_flow/function_pointer.c -o /tmp/build/function_pointer.out
```

