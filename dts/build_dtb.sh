#!/bin/sh
dtc -O dtb -o riscv_em.dtb riscv_em.dts
dtc -O dtb -o riscv_em32_linux.dtb riscv_em32_linux.dts
