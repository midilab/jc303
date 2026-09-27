# --------------------------------------------------------------
# JC-303 Linux builder environment - Debian 11 (Bullseye)
#
# Why?
# 1: Build binary release with broad Linux OS compatibility
# uses glibc 2.31 as base (Debian 11)
# 2: Make use of CI/CD
#
# Debian 11 (Bullseye) + glibc 2.31 + Modern toolchain + All JUCE deps
# Mounts jc303 project root → /jc303
#
# Build x86_64 release (default behavior)
# docker build --build-arg RELEASE_ARCH=X86_64 -t jc303-linux-builder-x86_64 .
# Build arm64 release
# docker build --build-arg RELEASE_ARCH=ARM64 -t jc303-linux-builder-arm64 .
#
# Run – mounts current directory → /jc303
# docker run -it --rm -v "$(pwd):/jc303" jc303-linux-builder-{arch}
# --------------------------------------------------------------
FROM debian:11

ARG TARGETPLATFORM
ARG RELEASE_ARCH=X86_64

ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------
# Determine build flavor based on host + desired release architecture
# ------------------------------------------------------------
# RELEASE_ARCH = X86_64  → produce x86_64 binary
# RELEASE_ARCH = ARM64   → produce arm64 binary
#
# Logic:
#   Host linux/amd64 + RELEASE_ARCH=X86_64   → native build
#   Host linux/amd64 + RELEASE_ARCH=ARM64    → cross-compile to arm64
#   Host linux/arm64 + RELEASE_ARCH=X86_64   → cross-compile to x86_64
#   Host linux/arm64 + RELEASE_ARCH=ARM64    → native build
# ------------------------------------------------------------
RUN echo ">>> TARGETPLATFORM = $TARGETPLATFORM | RELEASE_ARCH = $RELEASE_ARCH" && \
    case "$TARGETPLATFORM:$RELEASE_ARCH" in \
        "linux/amd64:X86_64") \
            echo ">>> Native x86_64 build"; \
            echo "BUILD_NATIVE=1" >> /etc/environment; \
            echo "TARGET_ARCH=x86_64" >> /etc/environment; \
            echo "CROSS_COMPILE=0" >> /etc/environment;; \
        "linux/amd64:ARM64") \
            echo ">>> Cross-compiling to ARM64 from x86_64"; \
            echo "BUILD_NATIVE=0" >> /etc/environment; \
            echo "TARGET_ARCH=arm64" >> /etc/environment; \
            echo "CROSS_COMPILE=1" >> /etc/environment;; \
        "linux/arm64:X86_64") \
            echo ">>> Cross-compiling to X86_64 from arm64"; \
            echo "BUILD_NATIVE=0" >> /etc/environment; \
            echo "TARGET_ARCH=x86_64" >> /etc/environment; \
            echo "CROSS_COMPILE=1" >> /etc/environment;; \
        "linux/arm64:ARM64") \
            echo ">>> Native arm64 build"; \
            echo "BUILD_NATIVE=1" >> /etc/environment; \
            echo "TARGET_ARCH=arm64" >> /etc/environment; \
            echo "CROSS_COMPILE=0" >> /etc/environment;; \
        *) \
            echo "ERROR: Unsupported combination" && exit 1;; \
    esac

# ------------------------------------------------------------
# 0. Install Kitware CMake
# ------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y ca-certificates gpg wget && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ focal main' \
    > /etc/apt/sources.list.d/kitware.list && \
    apt-get update && \
    apt-get install -y cmake ninja-build && \
    cmake --version

# ------------------------------------------------------------
# 1. Universal native tools
# ------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    build-essential git wget curl pkg-config \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# 2. Install dev libraries (native or cross)
# ------------------------------------------------------------
RUN if [ "$TARGETPLATFORM" = "linux/arm64" ] && [ "$RELEASE_ARCH" = "X86_64" ]; then \
        # ARM64 host → cross to x86_64
        dpkg --add-architecture amd64 && apt-get update && \
        apt-get install -y \
        # X86_64 dependencies
        gcc-x86-64-linux-gnu g++-x86-64-linux-gnu \
        libfontconfig1-dev:amd64 libx11-dev:amd64 libxrender-dev:amd64 \
        libxext-dev:amd64 libxcursor-dev:amd64 libxi-dev:amd64 \
        libxinerama-dev:amd64 libxrandr-dev:amd64 libgl1-mesa-dev:amd64 \
        libasound2-dev:amd64 libjack-jackd2-dev:amd64 libgtk-3-dev:amd64 \
        libwebkit2gtk-4.0-dev \
        # ARM64 dependencies
        libfontconfig1-dev \
        libx11-dev \
        libxrender-dev \
        libxext-dev \
        libfreetype6-dev \
        libglib2.0-dev \
        libgtk-3-dev \
        libwebkit2gtk-4.0-dev \
        && apt-get clean; \
    elif [ "$TARGETPLATFORM" = "linux/amd64" ] && [ "$RELEASE_ARCH" = "ARM64" ]; then \
        # X86_64 host → cross to ARM64
        dpkg --add-architecture arm64 && apt-get update && \
        apt-get install -y \
        # ARM64 dependencies
        gcc-aarch64-linux-gnu g++-aarch64-linux-gnu pkg-config-aarch64-linux-gnu \
        libfontconfig1-dev:arm64 libx11-dev:arm64 libxrender-dev:arm64 \
        libxext-dev:arm64 libx11-xcb-dev:arm64 libxcursor-dev:arm64 \
        libxi-dev:arm64 libxinerama-dev:arm64 libxrandr-dev:arm64 \
        libgl1-mesa-dev:arm64 libfreetype6-dev:arm64 \
        libasound2-dev:arm64 libjack-jackd2-dev:arm64 \
        libgtk-3-dev:arm64 libwebkit2gtk-4.0-dev:arm64 \
        libxml2-dev:arm64 libzip-dev:arm64 \
        # X86_64 dependencies
        libfontconfig1-dev \
        libx11-dev \
        libxrender-dev \
        libxext-dev \
        libfreetype6-dev \
        libglib2.0-dev \
        libgtk-3-dev \
        libwebkit2gtk-4.0-dev \
        && apt-get clean; \
    else \
        # Native build
        apt-get update && \
        apt-get install -y \
        libfontconfig1-dev libx11-dev libxrender-dev libxext-dev \
        libxcursor-dev libxi-dev libxinerama-dev libxrandr-dev \
        libgl1-mesa-dev libfreetype6-dev \
        libasound2-dev libjack-jackd2-dev \
        libgtk-3-dev libwebkit2gtk-4.0-dev \
        libxml2-dev libzip-dev \
        && apt-get clean; \
    fi && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# 4. Environment variables
# ------------------------------------------------------------
RUN if [ "$TARGETPLATFORM" = "linux/amd64" ] && [ "$RELEASE_ARCH" = "ARM64" ]; then \
    echo "export CC=aarch64-linux-gnu-gcc" >> /etc/bash.bashrc; \
    echo "export CXX=aarch64-linux-gnu-g++" >> /etc/bash.bashrc; \
    echo "export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig" >> /etc/bash.bashrc; \
    echo "export CMAKE_TOOLCHAIN_FILE=/toolchain-aarch64.cmake" >> /etc/bash.bashrc; \
    elif [ "$TARGETPLATFORM" = "linux/arm64" ] && [ "$RELEASE_ARCH" = "X86_64" ]; then \
    echo "export CC=x86_64-linux-gnu-gcc" >> /etc/bash.bashrc; \
    echo "export CXX=x86_64-linux-gnu-g++" >> /etc/bash.bashrc; \
    echo "export PKG_CONFIG_LIBDIR=/usr/lib/x86_64-linux-gnu/pkgconfig" >> /etc/bash.bashrc; \
    fi

WORKDIR /jc303

CMD ["/bin/bash"]
