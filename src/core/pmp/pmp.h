#ifndef RISCV_PMP_H
#define RISCV_PMP_H

#include <stdint.h>
#include <riscv_types.h>

#ifdef RV64
    #define PMP_NR_CFG_REGS 2
#else
    #define PMP_NR_CFG_REGS 4
#endif

#define PMP_NR_ADDR_REGS 16

/* Lock bit */
#define PMP_CFG_L_BIT 7
/* Adress matching bit offset */
#define PMP_CFG_A_BIT_OFFS 3
/* Executable bit */
#define PMP_CFG_X_BIT 2
/* Writable bit */
#define PMP_CFG_W_BIT 1
/* Readable bit */
#define PMP_CFG_R_BIT 0

typedef enum
{
    pmp_a_off = 0,
    pmp_a_tor,
    pmp_a_na4,
    pmp_a_napot

} pmp_addr_matching;

typedef struct pmp_struct
{
    union {
        struct {
            rv_uint_xlen cfg[PMP_NR_CFG_REGS];
            rv_uint_xlen addr[PMP_NR_ADDR_REGS];
        };
        rv_uint_xlen regs[PMP_NR_CFG_REGS+PMP_NR_ADDR_REGS];
    };

} pmp_td;

int pmp_write_csr(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val);
int pmp_mem_check(pmp_td *pmp, privilege_level curr_priv, rv_uint_xlen addr);
void pmp_dump_cfg_regs(pmp_td *pmp);

#endif /* RISCV_PMP_H */
