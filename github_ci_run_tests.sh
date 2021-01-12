#!/bin/sh

set -e

WORKDIR=${PWD}/ci_workdir

cd build && export PATH=${PWD}:${PATH}
cd ${WORKDIR}/toolchain/bin && export PATH=${PWD}:${PATH}
cd ${WORKDIR}

riscv64-none-elf-gcc --version

cd riscv-test-scripts

./build_riscv_em_tests.sh 64
