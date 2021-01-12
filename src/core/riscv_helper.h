#ifndef RISCV_HELPER_H
#define RISCV_HELPER_H

#include <stdlib.h>
#include <stdarg.h>

#include <riscv_config.h>
#include <riscv_types.h>

#ifdef RISCV_EM_DEBUG
#define DEBUG_PRINT(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define DEBUG_PRINT(...) do{ } while ( 0 )
#endif

#ifdef RV64
    #define PRINTF_FMT "%016lx"
    #define XLEN_INT_MIN 0x8000000000000000

    static inline void umul64wide (uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
    {
        uint64_t a_lo = (uint32_t)a;
        uint64_t a_hi = a >> 32;
        uint64_t b_lo = (uint32_t)b;
        uint64_t b_hi = b >> 32;

        uint64_t p0 = a_lo * b_lo;
        uint64_t p1 = a_lo * b_hi;
        uint64_t p2 = a_hi * b_lo;
        uint64_t p3 = a_hi * b_hi;

        uint32_t cy = (uint32_t)(((p0 >> 32) + (uint32_t)p1 + (uint32_t)p2) >> 32);

        *lo = p0 + (p1 << 32) + (p2 << 32);
        *hi = p3 + (p1 >> 32) + (p2 >> 32) + cy;
    }

    static inline void mul64wide (int64_t a, int64_t b, int64_t *hi, int64_t *lo)
    {
        umul64wide ((uint64_t)a, (uint64_t)b, (uint64_t *)hi, (uint64_t *)lo);
        if (a < 0LL) *hi -= b;
        if (b < 0LL) *hi -= a;
    }

    static inline void mulhsu64wide (int64_t a, uint64_t b, int64_t *hi, int64_t *lo)
    {
        umul64wide ((uint64_t)a, (uint64_t)b, (uint64_t *)hi, (uint64_t *)lo);
        if (a < 0LL) *hi -= b;
    }

    #define UMUL umul64wide
    #define MUL mul64wide
    #define MULHSU mulhsu64wide
#else
    #define PRINTF_FMT "%08x"
    #define XLEN_INT_MIN 0x80000000

    #define UMUL umul32wide
    #define MUL mul32wide
    #define MULHSU mulhsu32wide
#endif

    static inline void umul32wide (uint32_t a, uint32_t b, uint32_t *hi, uint32_t *lo)
    {
        uint32_t a_lo = (uint16_t)a;
        uint32_t a_hi = a >> 16;
        uint32_t b_lo = (uint16_t)b;
        uint32_t b_hi = b >> 16;

        uint32_t p0 = a_lo * b_lo;
        uint32_t p1 = a_lo * b_hi;
        uint32_t p2 = a_hi * b_lo;
        uint32_t p3 = a_hi * b_hi;

        uint32_t cy = (uint16_t)(((p0 >> 16) + (uint16_t)p1 + (uint16_t)p2) >> 16);

        *lo = p0 + (p1 << 16) + (p2 << 16);
        *hi = p3 + (p1 >> 16) + (p2 >> 16) + cy;
    }

    static inline void mul32wide (int32_t a, int32_t b, int32_t *hi, int32_t *lo)
    {
        umul32wide ((uint32_t)a, (uint32_t)b, (uint32_t *)hi, (uint32_t *)lo);
        if (a < 0LL) *hi -= b;
        if (b < 0LL) *hi -= a;
    }

    static inline void mulhsu32wide (int32_t a, uint32_t b, int32_t *hi, int32_t *lo)
    {
        umul32wide ((uint32_t)a, (uint32_t)b, (uint32_t *)hi, (uint32_t *)lo);
        if (a < 0LL) *hi -= b;
    }

#define die_msg(...) { printf(__VA_ARGS__); exit(-1); }

#define SET_BIT(_out_var,_nbit)   ((_out_var) |=  (1<<(_nbit)))
#define CLEAR_BIT(_out_var,_nbit) ((_out_var) &= ~(1<<(_nbit)))
#define FLIP_BIT(_out_var,_nbit)  ((_out_var) ^=  (1<<(_nbit)))
#define CHECK_BIT(_out_var,_nbit) ((_out_var) &   (1<<(_nbit)))

static inline void assign_u8_bit(uint8_t *out_var, uint8_t nbit, uint8_t bit_value)
{
    *out_var = (*out_var & ~(1UL << nbit)) | ((bit_value & 1) << nbit);
}

static inline void assign_u32_bit(uint32_t *out_var, uint32_t nbit, uint32_t bit_value)
{
    *out_var = (*out_var & ~(1UL << nbit)) | ((bit_value & 1) << nbit);
}

static inline void assign_xlen_bit(rv_uint_xlen *out_var, rv_uint_xlen nbit, rv_uint_xlen bit_value)
{
    *out_var = (*out_var & ~(1UL << nbit)) | ((bit_value & 1) << nbit);
}

static inline uint8_t extract8(uint8_t value, int start, int length)
{
    return (value >> start) & (0xFF >> (8 - length));
}

static inline uint32_t extract32(uint32_t value, int start, int length)
{
    return (value >> start) & (0xFFFFFFFF >> (32 - length));
}

#define ADDR_WITHIN(_addr, _start, _size) ( (_addr >= _start) && (_addr < _start + _size) )

#define ASSIGN_MIN(a,b) (((a)<(b))?(a):(b))
#define ASSIGN_MAX(a,b) (((a)>(b))?(a):(b))

#endif /* RISCV_HELPER_H */
