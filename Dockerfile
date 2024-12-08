# Start with Fedora 41
FROM fedora:41 AS llvm-builder

# Create a non-root user
RUN useradd -r -s /bin/false llvmuser

# Set working directory permissions
WORKDIR /llvm-project
RUN chown llvmuser:llvmuser /llvm-project

# Install necessary packages
RUN dnf update -y && dnf install -y \
    git \
    cmake \
    gcc \
    gcc-c++ \
    ninja-build \
    python3 \
    make \
    which \
    perl \
    tar \
    xz \
    python-devel \
    ncurses-devel \
    findutils \
    && dnf clean all \
    && rm -rf /var/cache/dnf/*

# Clone LLVM project with specific version
RUN git clone --depth 1 --branch llvmorg-19.1.4 https://github.com/llvm/llvm-project.git . 

# Create build and output directories
RUN mkdir /llvm-project/build

# Configure and build LLVM/Clang
# -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra": Enables building Clang and extra tools.
# -DCMAKE_BUILD_TYPE=Release: Sets the build type to Release.
# -DLLVM_BUILD_TESTS=OFF: Disables building tests.
# -DLLVM_ENABLE_RTTI=ON: Enables Run-Time Type Information.
# -DLLVM_ENABLE_EH=ON: Enables exception handling.
# -DCMAKE_INSTALL_PREFIX=/app/output/llvm-19.1.4: Sets the installation directory.

WORKDIR /llvm-project/build
RUN cmake -G Ninja ../llvm \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_ENABLE_EH=ON \
    && ninja 

# Install LLVM/Clang
RUN ninja install

# Switch to non-root user
USER llvmuser

# Change working directory
WORKDIR /app

# Default command
CMD ["/bin/bash"]


FROM fedora:41 AS llvm-runtime

# Create a non-root user with normal configs
RUN useradd -m -s /bin/bash llvmuser

# Set working directory permissions
WORKDIR /app
RUN chown llvmuser:llvmuser /app

# Copy LLVM installation from builder
COPY --from=llvm-builder /usr/local /usr/local

# Create a script to install LLVM to host
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Switch to non-root user
USER llvmuser

WORKDIR /home/llvmuser

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

# Default command
# CMD ["/bin/bash"]

#docker run \
# --user $(id -u):$(id -g) \
# -v /path/on/host:/app/output:rw \
# --security-opt=no-new-privileges \
# --cap-drop=ALL \
# your-image-name [arguments]