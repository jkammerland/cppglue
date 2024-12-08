# Note: this does not work fully, it can only compile LLVM/Clang, but not run it without problems.

FROM alpine:3.21 AS llvm-builder

# Install necessary packages
RUN apk update && apk add --no-cache \
    git \
    cmake \
    gcc \
    g++ \
    ninja \
    python3 \
    python3-dev \
    make \
    which \
    perl \
    tar \
    xz \
    ncurses-dev \
    findutils \
    musl-dev \
    linux-headers \
    && rm -rf /var/cache/apk/*

# Create working directory
WORKDIR /llvm-project

# Clone LLVM project
RUN git clone https://github.com/llvm/llvm-project.git . && \
    git checkout llvmorg-19.1.4

# Create build directory
RUN mkdir build && cd build

# Configure and build LLVM/Clang with RTTI enabled
WORKDIR /llvm-project/build
RUN cmake -G Ninja ../llvm \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_TESTS=ON \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_ENABLE_EH=ON \
    && ninja && ninja install

# Clean up
RUN cd / && rm -rf /llvm-project

# Create a working directory
WORKDIR /app

CMD ["/bin/sh"]