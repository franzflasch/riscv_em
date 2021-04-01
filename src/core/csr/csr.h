#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#include <riscv_types.h>

#define CSR_ACCESS_READ (1<<0)
#define CSR_ACCESS_WRITE (1<<1)
#define CSR_ACCESS_RO(_priv_level) (CSR_ACCESS_READ << (_priv_level*2))
#define CSR_ACCESS_WO(_priv_level) (CSR_ACCESS_WRITE << (_priv_level*2))
#define CSR_ACCESS_RW(_priv_level) ((CSR_ACCESS_WRITE | CSR_ACCESS_READ) << (_priv_level*2))
#define CSR_ACCESS_READ_GRANTED(_priv_level, _csr_access_flags) (CSR_ACCESS_RO(_priv_level) & _csr_access_flags)
#define CSR_ACCESS_WRITE_GRANTED(_priv_level, _csr_access_flags) (CSR_ACCESS_WO(_priv_level) & _csr_access_flags)

#define STATUS_REG 0x00
#define EDELEG_REG 0x02
#define IDELEG_REG 0x03
#define TVEC_REG   0x05
#define EPC_REG    0x41
#define CAUSE_REG  0x42

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

#define CSR_PMPCFG0           0x3A0
#define CSR_PMPCFG1           0x3A1
#define CSR_PMPCFG2           0x3A2
#define CSR_PMPCFG3           0x3A3
#define CSR_PMPADDR0          0x3B0
#define CSR_PMPADDR1          0x3B1
#define CSR_PMPADDR2          0x3B2
#define CSR_PMPADDR3          0x3B3
#define CSR_PMPADDR4          0x3B4
#define CSR_PMPADDR5          0x3B5
#define CSR_PMPADDR6          0x3B6
#define CSR_PMPADDR7          0x3B7
#define CSR_PMPADDR8          0x3B8
#define CSR_PMPADDR9          0x3B9
#define CSR_PMPADDR10         0x3BA
#define CSR_PMPADDR11         0x3BB
#define CSR_PMPADDR12         0x3BC
#define CSR_PMPADDR13         0x3BD
#define CSR_PMPADDR14         0x3BE
#define CSR_PMPADDR15         0x3BF

/* Supervisor CSRs */
#define CSR_ADDR_SSTATUS      0x100
#define CSR_ADDR_SEDELEG      0x102
#define CSR_ADDR_SIDELEG      0x103
#define CSR_ADDR_SIE          0x104
#define CSR_ADDR_STVEC        0x105
#define CSR_ADDR_SCOUNTEREN   0x106

#define CSR_ADDR_SSCRATCH     0x140
#define CSR_ADDR_SEPC         0x141
#define CSR_ADDR_SCAUSE       0x142
#define CSR_ADDR_STVAL        0x143
#define CSR_ADDR_SIP          0x144

#define CSR_ADDR_MAX          0xFFF

/* CSR WRITE MASKS */
#ifdef RV64
    #define CSR_MASK_WR_ALL 0xFFFFFFFFFFFFFFFF
    #define CSR_MSTATUS_MASK 0x8000000F007FF9BB
    #define CSR_MTVEC_MASK 0xFFFFFFFFFFFFFFFC

    #define CSR_SSTATUS_MASK 0x80000003000DE133
#else
    #define CSR_MASK_WR_ALL 0xFFFFFFFF
    #define CSR_MSTATUS_MASK 0x807FF9BB
    #define CSR_MTVEC_MASK 0xFFFFFFFC

    #define CSR_SSTATUS_MASK 0x800DE133
#endif
#define CSR_MASK_ZERO 0
#define CSR_MIP_MIE_MASK 0xBBB
#define CSR_MIDELEG_MASK CSR_MIP_MIE_MASK
/* In particular, medeleg[11] are hardwired to zero. */
#define CSR_MEDELEG_MASK 0xF3FF

#define CSR_STVEC_MASK CSR_MTVEC_MASK
#define CSR_SIP_SIE_MASK 0x333
#define CSR_SIDELEG_MASK CSR_SIP_SIE_MASK
/* In particular, sedeleg[11:9] are all hardwired to zero. */
#define CSR_SEDELEG_MASK 0xF1FF

#define INIT_CSR_REG_DEFAULT(_csr, _index, _access_flags, _init_val, _MASK) \
    _csr[_index].access_flags = _access_flags; \
    _csr[_index].value = _init_val; \
    _csr[_index].mask = _MASK; \
    _csr[_index].priv = NULL; \
    _csr[_index].read_cb = NULL; \
    _csr[_index].write_cb = NULL; \
    _csr[_index].internal_reg = 0;

#define INIT_CSR_REG_SPECIAL(_csr, _index, _access_flags, _init_val, _MASK, _priv, _read_cb, _write_cb, _internal_reg) \
    _csr[_index].access_flags = _access_flags; \
    _csr[_index].value = _init_val; \
    _csr[_index].mask = _MASK; \
    _csr[_index].priv = _priv; \
    _csr[_index].read_cb = _read_cb; \
    _csr[_index].write_cb = _write_cb; \
    _csr[_index].internal_reg = _internal_reg;

typedef int (*csr_read_cb)(void *priv, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val);
typedef int (*csr_write_cb)(void *priv, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val);

typedef struct csr_reg_struct {
    uint16_t access_flags;
    rv_uint_xlen value;
    rv_uint_xlen mask;

    /* used if special handling is needed for e.g. pmp */
    void *priv;
    csr_read_cb read_cb;
    csr_write_cb write_cb;
    int internal_reg;

} csr_reg_td;

static inline rv_uint_xlen csr_get_mask(csr_reg_td *csr_regs, uint16_t address)
{
    return csr_regs[address].mask;
}

void csr_read_reg_internal(csr_reg_td *csr_regs, uint16_t address, rv_uint_xlen *out_val);
void csr_write_reg_internal(csr_reg_td *csr_regs, uint16_t address, rv_uint_xlen val);

int csr_read_reg(csr_reg_td *csr_regs, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val);
int csr_write_reg(csr_reg_td *csr_regs, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val);

#endif /* RISCV_CSR_H */
