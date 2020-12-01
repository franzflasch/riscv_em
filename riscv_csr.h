#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#include <riscv_types.h>

#define CSR_ACCESS_OK 0
#define CSR_ACCESS_ERR 1

#define CSR_ACCESS_MREAD (1<<0)
#define CSR_ACCESS_MWRITE (1<<1)

#define CSR_ADDR_MVENDORID 0xF11
#define CSR_ADDR_MARCHID   0xF12
#define CSR_ADDR_MIMPID    0xF13
#define CSR_ADDR_MHARTID   0xF14

#define CSR_ADDR_MSTATUS 0x300
#define CSR_ADDR_MISA    0x301
#define CSR_ADDR_MEDELEG 0x302
#define CSR_ADDR_MIDELEG 0x303
#define CSR_ADDR_MIE     0x304
#define CSR_ADDR_MTVEC   0x305

typedef struct csr_reg_struct {
    uint16_t address;
    uint16_t access_flags;
    rv_uint_xlen value;

} csr_reg_td;

typedef struct csr_reg_desc_struct {
    unsigned int reg_size;
    csr_reg_td *regs;

} csr_reg_desc_td;
#define INIT_CSR_REG_DESC(csr_reg_table) \
    static csr_reg_desc_td  csr_reg_table##_desc = \
    { sizeof(csr_reg_table)/sizeof(csr_reg_table[0]), csr_reg_table }

int read_csr_reg(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen *out_val);
int write_csr_reg(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen val);

#endif /* RISCV_CSR_H */
