#!/bin/sh

set -e

sudo apt update
sudo apt install -y cmake unzip qemu-system-riscv64

WORKDIR=${PWD}/ci_workdir

mkdir build && cd build
# build with debug define on, so we get all register states
cmake -D CMAKE_C_FLAGS="-DRISCV_EM_DEBUG" ..
make

mkdir -p ${WORKDIR} && cd ${WORKDIR}

wget -q https://nightly.link/franzflasch/build-tools-gcc/workflows/riscv64-baremetal/master/toolchain_built_from_tar.zip
unzip -q toolchain_built_from_tar.zip && rm toolchain_built_from_tar.zip
tar xf *.tar.xz && rm -rf *.tar.xz

#sudo mv toolchain /
#/toolchain/riscv64-none-elf/bin/riscv64-none-elf-gcc --version
#riscv64-none-elf-gcc --version

#echo "${WORKDIR}/toolchain/riscv64-none-elf/bin" >> $GITHUB_PATH

cd ${WORKDIR}/toolchain/bin && export PATH=${PWD}:${PATH}
cd ${WORKDIR}

riscv64-none-elf-gcc --version

git clone --recursive https://github.com/franzflasch/riscv-test-scripts.git
cd riscv-test-scripts

./apply_patches.sh
./build_qemu_tests.sh 64
