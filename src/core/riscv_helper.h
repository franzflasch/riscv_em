#ifndef RISCV_HELPER_H
#define RISCV_HELPER_H

#include <stdlib.h>
#include <stdarg.h>

#include <riscv_config.h>
#include <riscv_types.h>

#ifdef DEBUG
#define DEBUG_PRINT(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define DEBUG_PRINT(...) do{ } while ( 0 )
#endif

#ifdef RV64
#define PRINTF_FMT "%016lx"
#else
#define PRINTF_FMT "%08x"
#endif

#define die_msg(...) { printf(__VA_ARGS__); exit(-1); }

#define SET_BIT(_out_var,_nbit)   ((_out_var) |=  (1<<(_nbit)))
#define CLEAR_BIT(_out_var,_nbit) ((_out_var) &= ~(1<<(_nbit)))
#define FLIP_BIT(_out_var,_nbit)  ((_out_var) ^=  (1<<(_nbit)))
#define CHECK_BIT(_out_var,_nbit) ((_out_var) &   (1<<(_nbit)))
static inline void assign_xlen_bit(rv_uint_xlen *out_var, rv_uint_xlen nbit, rv_uint_xlen bit_value)
{
    *out_var = (*out_var & ~(1UL << nbit)) | ((bit_value & 1) << nbit);
}

#define ADDR_WITHIN(_addr, _start, size) ( (_addr >= _start) && (_addr < _start + size) )

#endif /* RISCV_HELPER_H */
