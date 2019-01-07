#ifndef _ENV_PHYSICAL_SINGLE_CORE_H
#define _ENV_PHYSICAL_SINGLE_CORE_H

#include "../encoding.h"

#define TESTNUM gp

#define RVTEST_RV32U                   \
  .macro init;                         \
  .endm

#define RVTEST_CODE_BEGIN \
	.globl _start;          \
_start:                   \
  call tests_begin_here;  \
                          \
tests_begin_here:         \
  nop;

#define RVTEST_FAIL \
  call _start;

#define RVTEST_PASS       \
  nop;                    \

#define RVTEST_CODE_END   \
  nop

//-----------------------------------------------------------------------
// Data Section Macro
//-----------------------------------------------------------------------

#define EXTRA_DATA

#define RVTEST_DATA_BEGIN                                               \
        EXTRA_DATA                                                      \
        .pushsection .tohost,"aw",@progbits;                            \
        .align 6; .global tohost; tohost: .dword 0;                     \
        .align 6; .global fromhost; fromhost: .dword 0;                 \
        .popsection;                                                    \
        .align 4; .global begin_signature; begin_signature:

#define RVTEST_DATA_END .align 4; .global end_signature; end_signature:

#endif

