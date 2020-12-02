#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#include <riscv_types.h>

#define CSR_ACCESS_OK 0
#define CSR_ACCESS_ERR 1

#define CSR_ACCESS_READ (1<<0)
#define CSR_ACCESS_WRITE (1<<1)
#define CSR_ACCESS_RO(priv_level) (CSR_ACCESS_READ << (priv_level*2))
#define CSR_ACCESS_WO(priv_level) (CSR_ACCESS_WRITE << (priv_level*2))
#define CSR_ACCESS_RW(priv_level) ((CSR_ACCESS_WRITE | CSR_ACCESS_READ) << (priv_level*2))
#define CSR_ACCESS_READ_GRANTED(priv_level, csr_access_flags) (CSR_ACCESS_RO(priv_level) & csr_access_flags)
#define CSR_ACCESS_WRITE_GRANTED(priv_level, csr_access_flags) (CSR_ACCESS_WO(priv_level) & csr_access_flags)

#define CSR_ADDR_MVENDORID 0xF11
#define CSR_ADDR_MARCHID   0xF12
#define CSR_ADDR_MIMPID    0xF13
#define CSR_ADDR_MHARTID   0xF14

#define CSR_ADDR_MSTATUS      0x300
#define CSR_ADDR_MISA         0x301
#define CSR_ADDR_MEDELEG      0x302
#define CSR_ADDR_MIDELEG      0x303
#define CSR_ADDR_MIE          0x304
#define CSR_ADDR_MTVEC        0x305
#define CSR_ADDR_MCOUNTEREN   0x305

#define CSR_ADDR_MSCRATCH     0x340
#define CSR_ADDR_MEPC         0x341
#define CSR_ADDR_MCAUSE       0x342
#define CSR_ADDR_MTVAL        0x343
#define CSR_ADDR_MIP          0x344

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

int read_csr_reg(csr_reg_desc_td *reg_table, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val);
int write_csr_reg(csr_reg_desc_td *reg_table, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val);

#endif /* RISCV_CSR_H */
