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

rv_ret clint_bus_access(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen address, void *value, uint8_t len);
void clint_update(clint_td *clint, uint8_t *msi, uint8_t *mti);

#endif /* RISCV_CLINT_H */
