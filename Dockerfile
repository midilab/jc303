# --------------------------------------------------------------
# JC-303 Linux builder environment - Debian 11 (Bullseye)
#
# Why?
# 1: Build binary release with broad Linux OS compatibility
#    uses glibc 2.31 as base (Debian 11)
# 2: Make use of CI/CD
#
# Debian 11 (Bullseye) + glibc 2.31 + Modern toolchain + All JUCE deps
# Mounts jc303 project root → /jc303
#
# Build the image (once)
# docker build -t jc303-linux-builder .
#
# Run – mounts current directory → /jc303
# docker run -it --rm -v "$(pwd):/jc303" jc303-linux-builder
# --------------------------------------------------------------
# Base image is always Debian 11 (host type doesn't matter)
FROM debian:11

ARG TARGETPLATFORM
ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------
# 0. Install Kitware CMake (same for both architectures)
# ------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y ca-certificates gpg wget && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] \
    https://apt.kitware.com/ubuntu/ focal main' \
    > /etc/apt/sources.list.d/kitware.list && \
    apt-get update && \
    apt-get install -y cmake && \
    cmake --version

# ------------------------------------------------------------
# 1. Universal native tools (always installed)
# ------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    git \
    wget \
    curl \
    pkg-config \
    ninja-build \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# 2. Conditional: If ARM host → install amd64 cross-toolchain
# ------------------------------------------------------------
RUN if [ "$TARGETPLATFORM" = "linux/arm64" ]; then \
    echo ">>> ARM64 HOST DETECTED — Installing cross environment"; \
    dpkg --add-architecture amd64 && apt-get update && \
    apt-get install -y \
    gcc-x86-64-linux-gnu \
    g++-x86-64-linux-gnu \
    # Graphics
    libx11-dev:amd64 \
    libxext-dev:amd64 \
    libxrandr-dev:amd64 \
    libxinerama-dev:amd64 \
    libxcursor-dev:amd64 \
    libxi-dev:amd64 \
    libgl1-mesa-dev:amd64 \
    libglu1-mesa-dev:amd64 \
    libfreetype6-dev:amd64 \
    # Audio
    libasound2-dev:amd64 \
    libjack-jackd2-dev:amd64 \
    libsamplerate0-dev:amd64 \
    libsndfile1-dev:amd64 \
    # Network / utils
    libcurl4-openssl-dev:amd64 \
    libavahi-client-dev:amd64 \
    # JUCE extras
    libgtk-3-dev:amd64 \
    libwebkit2gtk-4.0-dev:amd64 \
    libxml2-dev:amd64 \
    libzip-dev:amd64 \
    libfftw3-dev:amd64 \
    libjpeg-dev:amd64 \
    libpng-dev:amd64 \
    libgif-dev:amd64 \
    librsvg2-dev:amd64 \
    libbz2-dev:amd64 ; \
    else \
    echo ">>> AMD64 HOST DETECTED — Installing native libs"; \
    apt-get update && \
    apt-get install -y \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libfreetype6-dev \
    # Audio
    libasound2-dev \
    libjack-jackd2-dev \
    libsamplerate0-dev \
    libsndfile1-dev \
    # Network / utils
    libcurl4-openssl-dev \
    libavahi-client-dev \
    # JUCE extras
    libgtk-3-dev \
    libwebkit2gtk-4.0-dev \
    libxml2-dev \
    libzip-dev \
    libfftw3-dev \
    libjpeg-dev \
    libpng-dev \
    libgif-dev \
    librsvg2-dev \
    libbz2-dev ; \
    fi \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# 3. If ARM host → set default cross toolchain environment
# ------------------------------------------------------------
ENV CC_x86_64="x86_64-linux-gnu-gcc"
ENV CXX_x86_64="x86_64-linux-gnu-g++"

RUN if [ "$TARGETPLATFORM" = "linux/arm64" ]; then \
    echo "export CC=\"$CC_x86_64\""  >> /etc/bash.bashrc; \
    echo "export CXX=\"$CXX_x86_64\"" >> /etc/bash.bashrc; \
    echo "export CROSS_AMD64=1" >> /etc/bash.bashrc; \
    echo "export PKG_CONFIG_LIBDIR=/usr/lib/x86_64-linux-gnu/pkgconfig" >> /etc/bash.bashrc; \
    else \
    echo "export CROSS_AMD64=0" >> /etc/bash.bashrc; \
    fi

# ------------------------------------------------------------
# 4. Workdir
# ------------------------------------------------------------
WORKDIR /jc303

# ------------------------------------------------------------
# 5. Default shell
# ------------------------------------------------------------
CMD ["/bin/bash"]
