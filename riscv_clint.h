#ifndef RISCV_CLINT_H
#define RISCV_CLINT_H

#include <riscv_types.h>

#define CLINT_ACCESS_OK 0
#define CLINT_ACCESS_ERR 1

typedef enum 
{
    clint_msip = 0,
    clint_mtimecmp,
    clint_mtime,

    clint_reg_max

} clint_regs;

typedef struct clint_struct
{
    uint64_t regs[clint_reg_max];

} clint_td;

int read_clint_reg(clint_td *clint, rv_uint_xlen address, rv_uint_xlen *outval);
int write_clint_reg(clint_td *clint, rv_uint_xlen address, rv_uint_xlen val);

#endif /* RISCV_CLINT_H */
