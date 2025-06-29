FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# ==== Dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    software-properties-common \
    wget \
    git \
    curl \
    gnupg \
    libssl-dev \
    ninja-build \
    gcc-13 \
    g++-13 \
    clang-16 \
    && apt-get clean

# Set GCC 13 as the default GCC version
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# ==== Download LLVM 16.0.6
WORKDIR /workspace/

RUN wget https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.6/llvm-project-16.0.6.src.tar.xz \
    && tar -xf llvm-project-16.0.6.src.tar.xz 

# Download and install CMake 3.22.1
RUN wget https://github.com/Kitware/CMake/releases/download/v3.22.1/cmake-3.22.1-linux-x86_64.tar.gz \
    && tar -zxvf cmake-3.22.1-linux-x86_64.tar.gz \
    && mv cmake-3.22.1-linux-x86_64 /opt/cmake \
    && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake \
    && rm cmake-3.22.1-linux-x86_64.tar.gz

# Set the environment variables for Clang
ENV CXX=/usr/local/bin/clang++-16
ENV CC=/usr/local/bin/clang-16

RUN wget https://apt.llvm.org/llvm.sh  \
    &&  chmod +x llvm.sh \
    && ./llvm.sh

# LLVM CMAKE CONFIG 

WORKDIR /workspace/llvm-project-16.0.6.src

RUN mkdir build \
    && cd build \
    && cmake -S ../llvm -B . -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS='clang' -DLLVM_INSTALL_UTILS=ON -DLLVM_BUILD_LLVM_DYLIB=ON -DLLVM_LINK_LLVM_DYLIB=ON -DLLVM_OPTIMIZED_TABLEGEN=ON -DLLVM_INCLUDE_EwebXAMPLES=OFF -DLLVM_USE_LINKER=NO  -DCMAKE_C_COMPILER=clang-16 -DCMAKE_CXX_COMPILER=clang++-16 -DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_EH=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_CROSSCOMPILING=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
    && ninja -j10

RUN export PATH=/home/ubuntu/llvm-project-16.0.6.src/build/bin:$PATH

# Install ASPIS
WORKDIR /workspace/

ENV CC=''
ENV CXX=''

RUN git clone https://github.com/HEAPLab/ASPIS.git \
    && cd ASPIS \
    && mkdir build \
    && cmake -B build -DLLVM_DIR=/workspace/llvm-project-16.0.6.src/build/lib/cmake/llvm \
    && cmake --build build

# Set the working directory to where ASPIS is located
WORKDIR /workspace/ASPIS

# Export path to add llvm binaries
ENV PATH=/workspace/llvm-project-16.0.6.src/build/bin:$PATH