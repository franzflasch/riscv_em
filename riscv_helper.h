#ifndef RISCV_HELPER_H
#define RISCV_HELPER_H

#include <riscv_config.h>

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


#endif /* RISCV_HELPER_H */
