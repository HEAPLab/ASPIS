# =====================
# 1st Stage: Builder Image
# =====================
FROM ubuntu:24.04 as builder
ARG DEBIAN_FRONTEND=noninteractive

# Set installation paths
ENV LLVM_DIR=/opt/llvm
ENV ASPIS_DIR=/opt/aspis

# ==== Install Dependencies ====
RUN apt-get update && apt-get install -y --no-install-recommends \
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
    && rm -rf /var/lib/apt/lists/*

# Set GCC 13 as the default compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# ==== CMake 3.22.1 ====
RUN wget https://github.com/Kitware/CMake/releases/download/v3.22.1/cmake-3.22.1-linux-x86_64.tar.gz \
    && tar -zxvf cmake-3.22.1-linux-x86_64.tar.gz \
    && mv cmake-3.22.1-linux-x86_64 /opt/cmake \
    && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake \
    && rm cmake-3.22.1-linux-x86_64.tar.gz

# ==== LLVM 16.0.6 ====
WORKDIR $LLVM_DIR
RUN wget https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.6/llvm-project-16.0.6.src.tar.xz \
    && tar -xf llvm-project-16.0.6.src.tar.xz --strip-components=1 \
    && rm llvm-project-16.0.6.src.tar.xz

# Additional LLVM dependencies
RUN wget https://apt.llvm.org/llvm.sh  \
    && chmod +x llvm.sh \
    && ./llvm.sh \
    && rm llvm.sh  # Clean up after install

# ==== LLVM - Configure & Build ====
WORKDIR $LLVM_DIR
RUN mkdir build \
    && cd build \
    && cmake -S ../llvm -B . -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS='clang' \
    -DCMAKE_C_COMPILER=clang-16 \
    -DCMAKE_CXX_COMPILER=clang++-16 \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DLLVM_LINK_LLVM_DYLIB=ON \
    -DLLVM_ENABLE_RTTI=OFF \
    -DLLVM_ENABLE_EH=OFF \
    && ninja -j$(( ($(nproc) - 2 )/ 2))

# Set LLVM binaries in PATH
ENV PATH="${LLVM_DIR}/build/bin:${PATH}"

# ==== Install ASPIS ====
WORKDIR $ASPIS_DIR

RUN git clone https://github.com/HEAPLab/ASPIS.git . \
    && mkdir build \
    && cmake -B build -DLLVM_DIR=${LLVM_DIR}/build/lib/cmake/llvm \
    && cmake --build build


# =====================
# 2nd Stage: Runtime Image
# =====================
FROM ubuntu:24.04 AS runtime

# Install minimal runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
    ca-certificates \
    libc++-dev \
    libc++abi-dev \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# LLVM executables and libraries
COPY --from=builder /opt/llvm/build/bin/ /usr/local/bin/
COPY --from=builder /opt/llvm/build/lib/ /usr/local/lib/
COPY --from=builder /usr/lib/ /usr/lib/
COPY --from=builder /lib/ /lib/ 

ENV PATH="/usr/local/bin:${PATH}"
ENV LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH}"

# Copy ASPIS
COPY --from=builder /opt/aspis/ /opt/aspis/