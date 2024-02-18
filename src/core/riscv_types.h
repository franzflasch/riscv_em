#ifndef RISCV_TYPES_H
#define RISCV_TYPES_H

#include <stdint.h>

#include <riscv_config.h>

#define __PACKED __attribute__((__packed__)) 

#ifdef RV64
    #define XLEN 64
    typedef uint64_t rv_uint_xlen;
    typedef int64_t rv_int_xlen;
#else
    #define XLEN 32
    typedef uint32_t rv_uint_xlen;
    typedef int32_t rv_int_xlen;
#endif

typedef enum
{
    rv_ok,
    rv_err

} rv_ret;

typedef enum  
{
    priv_level_unknown = -1, /* This just ensures that the enum is signed, which might be needed in down counting for loops */
    user_mode = 0,
    supervisor_mode = 1,
    reserved_mode = 2, /* Hypervisor ?? */
    machine_mode = 3,
    supervisor_39_mode = 8,
    priv_level_max = 4

} privilege_level;

typedef enum
{
    bus_read_access = 0,
    bus_write_access,
    bus_instr_access,

    bus_access_type_max

} bus_access_type;

typedef rv_ret (*bus_access_func)(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen addr, void *value, uint8_t len);

#endif /* RISCV_TYPES_H */
