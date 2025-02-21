FROM jkammerland/cppglue:py-gen

USER root

RUN dnf update -y && dnf install -y \
    boost-devel \
    wget \
    expat-devel \
    && dnf clean all \
    && rm -rf /var/cache/dnf/*

RUN \
    git clone https://github.com/COVESA/vsomeip.git && \
    cd vsomeip && git submodule update --init --recursive && \
    mkdir build && cd build && \
    cmake -E env CXXFLAGS="-Wno-error=stringop-overflow" \ 
    # -DENABLE_SIGNAL_HANDLING=1  so that CTRL+C works, without any problems
    # (otherwise it might be that the shared memory segment /dev/shm/vsomeip is not be correctly removed when you stop the application with Ctrl-C).
    cmake .. -DBoost_INCLUDE_DIR=/usr/include/boost -DENABLE_SIGNAL_HANDLING=1 -G Ninja && \
    ninja && ninja install

RUN \
    # Download and install latest custom vsomeip fork
    git clone https://github.com/jkammerland/capicxx-core-runtime.git && \
    cd capicxx-core-runtime && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja && \
    ninja && ninja install 

RUN \
    git clone https://github.com/COVESA/capicxx-someip-runtime.git && \
    cd capicxx-someip-runtime && \
    mkdir build && cd build && \
    cmake .. -DUSE_INSTALLED_COMMONAPI=ON -DCMAKE_BUILD_TYPE=Release -G Ninja && \
    ninja && ninja install

RUN \
    DBUS_VERSION=1.14.10 && \
    wget https://dbus.freedesktop.org/releases/dbus/dbus-${DBUS_VERSION}.tar.xz && \
    tar -xf dbus-${DBUS_VERSION}.tar.xz && \
    cd dbus-${DBUS_VERSION} && \
    ./configure --prefix=/usr/local && \
    make -C dbus && \
    make -C dbus install && \
    make install-pkgconfigDATA && \
    cd .. && \
    rm -rf dbus-${DBUS_VERSION}* && \
    echo "PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" >> /root/.bashrc

# RUN \
#     dnf update -y && dnf install -y \
#     dbus-devel \
#     && dnf clean all \
#     && rm -rf /var/cache/dnf/*

# RUN \
#     git clone https://github.com/COVESA/capicxx-dbus-runtime.git && \
#     cd capicxx-dbus-runtime && \
#     mkdir build && cd build && \
#     cmake .. -DCMAKE_PREFIX_PATH="/usr/local" -DUSE_INSTALLED_COMMONAPI=ON -DCMAKE_BUILD_TYPE=Release -G Ninja && \
#     ninja && ninja install

# Default command
CMD ["/bin/bash"]

