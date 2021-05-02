#!/bin/sh

set -e

ROOT_DIR=${PWD}
DEFAULT_PATH=${PATH}

TOOLCHAIN_DIR32=${ROOT_DIR}/toolchain32
cd ${ROOT_DIR}
cd build32 && export PATH=${PWD}:${DEFAULT_PATH}
cd ${TOOLCHAIN_DIR32}/toolchain/bin && export PATH=${PWD}:${PATH}

riscv32-none-elf-gcc --version

cd ${ROOT_DIR}/riscv-test-scripts
./build_riscv_em_tests.sh 32 ${ROOT_DIR}/dts/riscv_em.dtb


TOOLCHAIN_DIR64=${ROOT_DIR}/toolchain64
cd ${ROOT_DIR}
cd build64 && export PATH=${PWD}:${DEFAULT_PATH}
cd ${TOOLCHAIN_DIR64}/toolchain/bin && export PATH=${PWD}:${PATH}

riscv64-none-elf-gcc --version

cd ${ROOT_DIR}/riscv-test-scripts
./build_riscv_em_tests.sh 64 ${ROOT_DIR}/dts/riscv_em.dtb
