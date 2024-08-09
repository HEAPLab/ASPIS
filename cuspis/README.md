# CUSPIS - CUDA Software-based Protection and Integrity Suite

CUSPIS is an header-only library part of ASPIS for protecting CUDA code. 

## Features

## Usage

### Example
Compile the `add.cu` example with cuspis using the following commands:

```sh
cd ../examples/cuda/

nvcc -c add.cu -o add.o -I../../cuspis

g++ add.o -o add -L/usr/lib/cuda/lib64/ -ldl -lrt -pthread -I../../cuspis -lcuda -lcudart
```

And execute with `./add`.