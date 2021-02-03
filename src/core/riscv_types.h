#ifndef RISCV_TYPES_H
#define RISCV_TYPES_H

#include <stdint.h>

#include <riscv_config.h>

#ifdef RV64
    #define XLEN 64
    typedef uint64_t rv_uint_xlen;
    typedef int64_t rv_int_xlen;
#else
    #define XLEN 32
    typedef uint32_t rv_uint_xlen;
    typedef int32_t rv_int_xlen;
#endif

#define RV_MEM_ACCESS_OK 0
#define RV_MEM_ACCESS_ERR 1

typedef enum  
{
    user_mode = 0,
    supervisor_mode = 1,
    reserved_mode = 2, /* Hypervisor ?? */
    machine_mode = 3,

} privilege_level;

#endif /* RISCV_TYPES_H */