#!/bin/sh

set -e

sudo apt update
sudo apt install -y cmake unzip qemu-system-riscv32 qemu-system-riscv64 device-tree-compiler

ROOT_DIR=${PWD}

############################# RV32 #############################

TOOLCHAIN_DIR32=${ROOT_DIR}/toolchain32
mkdir -p ${TOOLCHAIN_DIR32} && cd ${TOOLCHAIN_DIR32}
wget -q https://nightly.link/franzflasch/build-tools-gcc/workflows/riscv32-baremetal/master/toolchain_built_from_tar.zip
unzip -q toolchain_built_from_tar.zip && rm toolchain_built_from_tar.zip
tar xf *.tar.xz && rm -rf *.tar.xz

cd ${TOOLCHAIN_DIR32}/toolchain/bin && export PATH=${PWD}:${PATH}

riscv32-none-elf-gcc --version

# Build device trees
cd ${ROOT_DIR}/dts
./build_dtb.sh

cd ${ROOT_DIR}
mkdir build32 && cd build32
# build with debug define on, so we get all register states
cmake -DRV_ARCH=32 -DRISCV_EM_DEBUG=1 ..
make

############################# RV64 #############################

TOOLCHAIN_DIR64=${ROOT_DIR}/toolchain64
mkdir -p ${TOOLCHAIN_DIR64} && cd ${TOOLCHAIN_DIR64}
wget -q https://nightly.link/franzflasch/build-tools-gcc/workflows/riscv64-baremetal/master/toolchain_built_from_tar.zip
unzip -q toolchain_built_from_tar.zip && rm toolchain_built_from_tar.zip
tar xf *.tar.xz && rm -rf *.tar.xz

cd ${TOOLCHAIN_DIR64}/toolchain/bin && export PATH=${PWD}:${PATH}

riscv64-none-elf-gcc --version

cd ${ROOT_DIR}
mkdir build64 && cd build64
# build with debug define on, so we get all register states
cmake -DRV_ARCH=64 -DRISCV_EM_DEBUG=1 ..
make


### Prepare qemu register states
cd ${ROOT_DIR}

git clone --recursive https://github.com/franzflasch/riscv-test-scripts.git
cd riscv-test-scripts

./apply_patches.sh
./build_qemu_tests.sh 32
./build_qemu_tests.sh 64
