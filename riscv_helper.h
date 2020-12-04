#ifndef RISCV_HELPER_H
#define RISCV_HELPER_H

#include <riscv_config.h>
#include <stdarg.h>

#ifdef DEBUG
#define DEBUG_PRINT(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define DEBUG_PRINT(...) do{ } while ( 0 )
#endif

#ifdef RV64
#define XLEN 64
#define PRINTF_FMT "%016lx"
#else
#define XLEN 32
#define PRINTF_FMT "%08x"
#endif

#define die_msg(...) { printf(__VA_ARGS__); exit(-1); }

#endif /* RISCV_HELPER_H */
