ARG LLVM_VERSION="20"

FROM ubuntu:22.04
 
ARG LLVM_VERSION
 
# Update package lists and install the necessary tools
RUN apt update && apt install -y --no-install-recommends \
    software-properties-common ca-certificates gpg wget \
    && rm -rf /var/lib/apt/lists/*
 
# Enable LLVM repositories with their own fresh releases
RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc && \
    echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-${LLVM_VERSION} main" > /etc/apt/sources.list.d/llvm.list

# Install required tools.
RUN apt-get -qq update && \
    apt-get -qq --no-install-recommends install vim git ca-certificates \
    automake autoconf libtool make cmake pkg-config libgmp3-dev libyaml-dev \
    opencl-c-headers ocl-icd-opencl-dev clinfo libpocl-dev pocl-opencl-icd clinfo \
    clang-20 libclang-20-dev llvm-20-dev && \
    rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/bin/llvm-config-${LLVM_VERSION} /usr/bin/llvm-config && \
    ln -s /usr/bin/clang-${LLVM_VERSION} /usr/bin/clang && \
    ln -s /usr/bin/clang++-${LLVM_VERSION} /usr/bin/clang++

COPY . /ppcg

WORKDIR /ppcg

RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/opt/ppcg .. && \
    cmake --build . -- -j8 && \
    ctest && \
    cmake --install .

