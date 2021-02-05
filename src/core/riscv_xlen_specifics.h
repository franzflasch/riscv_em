#ifndef RISCV_XLEN_SPECIFICS_H
#define RISCV_XLEN_SPECIFICS_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include <riscv_config.h>
#include <riscv_types.h>

#ifdef RISCV_EM_DEBUG
#define DEBUG_PRINT(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define DEBUG_PRINT(...) do{ } while ( 0 )
#endif

#ifdef RV64
    #define PRINTF_FMT "%016lx"
    #define PRINTF_FMTU "%lu"
    #define XLEN_INT_MIN 0x8000000000000000

    #define SHIFT_OP_MASK 0x3F

    /* Generic SIGN extension */
    #define SIGNEX(v, sb) ((v) | (((v) & (1LL << (sb))) ? ~((1LL << (sb))-1LL) : 0))

    #define UMUL umul64wide
    #define MUL mul64wide
    #define MULHSU mulhsu64wide
#else
    #define PRINTF_FMT "%08x"
    #define PRINTF_FMTU "%u"
    #define XLEN_INT_MIN 0x80000000

    #define SHIFT_OP_MASK 0x1F

    /* Generic SIGN extension */
    #define SIGNEX(v, sb) ((v) | (((v) & (1 << (sb))) ? ~((1 << (sb))-1) : 0))

    #define UMUL umul32wide
    #define MUL mul32wide
    #define MULHSU mulhsu32wide
#endif

#define GEN_SIGNEX_FUNC(_bit) \
static inline rv_int_xlen SIGNEX_BIT_##_bit(rv_int_xlen input) \
{ \
    struct {rv_int_xlen x:(_bit+1);} s = { .x = input }; \
    return s.x; \
}

GEN_SIGNEX_FUNC(7)
GEN_SIGNEX_FUNC(11)
GEN_SIGNEX_FUNC(12)
GEN_SIGNEX_FUNC(15)
GEN_SIGNEX_FUNC(19)
GEN_SIGNEX_FUNC(20)
GEN_SIGNEX_FUNC(31)

#endif /* RISCV_XLEN_SPECIFICS_H */
