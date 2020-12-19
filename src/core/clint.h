#ifndef RISCV_CLINT_H
#define RISCV_CLINT_H

#include <riscv_types.h>

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

int clint_read_reg(void *priv, rv_uint_xlen address, rv_uint_xlen *outval);
int clint_write_reg(void *priv, rv_uint_xlen address, rv_uint_xlen val, uint8_t nr_bytes);
void clint_update(clint_td *clint);

#endif /* RISCV_CLINT_H */
