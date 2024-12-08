FROM jkammerland/cppglue:llvm-runtime

USER root

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

# Set working directory permissions
WORKDIR /app

# Copy LLVM installation from llvm-runtime stage
COPY --from=jkammerland/cppglue:llvm-runtime /usr/local /usr/local

# SET ENVIRONMENT VARIABLES for LLVM/Clang
RUN echo "export LD_LIBRARY_PATH=/usr/local/lib:\$LD_LIBRARY_PATH" >> /root/.bashrc
# ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Default command
CMD ["/bin/bash"]

