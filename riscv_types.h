#ifndef RISCV_TYPES_H
#define RISCV_TYPES_H

#include <stdint.h>

#include <riscv_config.h>

#ifdef RV64
typedef uint64_t rv_uint_xlen;
typedef int64_t rv_int_xlen;
#else
typedef uint32_t rv_uint_xlen;
typedef int32_t rv_int_xlen;
#endif


#endif /* RISCV_TYPES_H */