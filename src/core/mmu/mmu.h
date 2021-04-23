#ifndef RISCV_MMU_H
#define RISCV_MMU_H

#include <stdint.h>
#include <riscv_types.h>

#define MMU_PAGE_VALID (1<<0)
#define MMU_PAGE_READ  (1<<1)
#define MMU_PAGE_WRITE (1<<2)
#define MMU_PAGE_EXEC  (1<<3)
#define MMU_PAGE_USER  (1<<4)
#define MMU_PAGE_GLOB  (1<<5)
#define MMU_PAGE_ACCESSED  (1<<6)
#define MMU_PAGE_DIRTY  (1<<7)

#define MMU_SATP_MODE_SV32 1

#define SV32_LEVELS 2
#define SV32_PAGE_SIZE 4096
#define SV32_PAGE_TABLE_ENTRIES 1024
#define SV32_PTESIZE 4
#define SV32_PTESHIFT 2

#ifdef RV64
    #define MMU_SATP_MODE_BIT 60
    #define MMU_SATP_MODE_NR_BITS 4
#else
    #define MMU_SATP_MODE_BIT 31
    #define MMU_SATP_MODE_NR_BITS 1
#endif

typedef enum
{
    mmu_ok = 0,
    mmu_page_fault

} mmu_ret;

typedef struct mmu_struct
{
    /* satp register */
    rv_uint_xlen satp_reg;

    /* bus access callbacks */
    // bus_read_mem read_mem;
    // bus_write_mem write_mem;
    bus_access_func bus_access;

    /* priv pointer for read and write mem cb */
    void *priv;

    rv_uint_xlen last_virt_pc;
    rv_uint_xlen last_phys_pc;    

} mmu_td;

#include <core.h>

void mmu_init(mmu_td *mmu, bus_access_func bus_access, void *priv);
uint64_t mmu_virt_to_phys(mmu_td *mmu, 
                          privilege_level curr_priv, 
                          rv_uint_xlen virt_addr, 
                          bus_access_type access_type, 
                          uint8_t mxr, 
                          uint8_t sum, 
                          mmu_ret *ret_val,
                          rv_core_td *rv_core,
                          rv_uint_xlen value);
mmu_ret mmu_mem_read(mmu_td *mmu, 
                     privilege_level curr_priv, 
                     rv_uint_xlen virt_addr, 
                     uint8_t len, 
                     bus_access_type access_type, 
                     uint8_t mxr, 
                     uint8_t sum);
mmu_ret mmu_mem_write(mmu_td *mmu, 
                      privilege_level curr_priv, 
                      rv_uint_xlen virt_addr, 
                      rv_uint_xlen val, 
                      uint8_t len, 
                      bus_access_type access_type, 
                      uint8_t mxr, 
                      uint8_t sum);
void mmu_dump(mmu_td *mmu);
rv_ret mmu_read_csr(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val);
rv_ret mmu_write_csr(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val);

#endif /* RISCV_MMU_H */
