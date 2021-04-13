#include <stdio.h>
#include <string.h>

#include <mmu.h>
#include <riscv_helper.h>

// #define MMU_DEBUG_ENABLE
#ifdef MMU_DEBUG_ENABLE
#define MMU_DEBUG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define MMU_DEBUG(...) do{ } while ( 0 )
#endif

/* PTE Flags are arranged as follows, the XWR (bus_access_type) bits are shifted by on in contrast to the PMP, so we simply shift by 1 so we can reuse it */
/* 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 */
/* D | A | G | U | X | W | R | V */
/* simple macro to tranlsate this to the mmu arrangement */
#define ACCESS_TYPE_TO_MMU(access_type) ((1 << access_type) << 1)

rv_ret mmu_read_csr(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    (void) reg_index;

    mmu_td *mmu = priv;
    *out_val = mmu->satp_reg;
    // printf("m read! %d %x\n", reg_index, *out_val);
    return rv_ok;
}

rv_ret mmu_write_csr(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val)
{
    (void) curr_priv;
    (void) reg_index;

    mmu_td *mmu = priv;

    /* we only have satp to write */
    mmu->satp_reg = csr_val;

    return rv_ok;
}

uint64_t mmu_virt_to_phys(mmu_td *mmu, 
                          privilege_level curr_priv, 
                          rv_uint_xlen virt_addr, 
                          bus_access_type access_type, 
                          uint8_t mxr, 
                          uint8_t sum, 
                          mmu_ret *ret_val)
{
    int i,j = 0;
    rv_uint_xlen pte_addr = 0;
    uint8_t pte_flags = 0;
    uint8_t user_page = 0;
    *ret_val = mmu_ok;
    uint8_t mode = extractxlen(mmu->satp_reg, MMU_SATP_MODE_BIT, MMU_SATP_MODE_NR_BITS);

    /* in machine mode we don't have address translation */
    if( (curr_priv == machine_mode) || !mode )
        return virt_addr;

    rv_uint_xlen pte = 0;

    rv_uint_xlen vpn[SV32_LEVELS] = 
    {
        (virt_addr >> 12) & 0x3ff,
        (virt_addr >> 22) & 0xfff
    };
    MMU_DEBUG("vpn[1] "PRINTF_FMT"\n", vpn[1]);
    MMU_DEBUG("vpn[0] "PRINTF_FMT"\n", vpn[0]);

    /*
     * 1. Let a be satp.ppn × PAGESIZE, and let i = LEVELS − 1. (For Sv32, PAGESIZE=2^12 and LEVELS=2.) 
     */
    rv_uint_xlen root_pg_table_addr = mmu->satp_reg * SV32_PAGE_SIZE;
    MMU_DEBUG("root pg addr: %x\n", root_pg_table_addr);

    for(i=(SV32_LEVELS-1),j=0;i>=0;i--,j++)
    {
        /*
        * 2. Let pte be the value of the PTE at address a+va.vpn[i]×PTESIZE. (For Sv32, PTESIZE=4.)
        * If accessing pte violates a PMA or PMP check, raise an access exception.
        */
        pte_addr = root_pg_table_addr + (SV32_PAGE_SIZE*j) + (vpn[i] << SV32_PTESHIFT);

        /* Here we should raise an exception if PMP violation occurs, will be done automatically
         * if read_mem is set to the "checked_read_mem()" function.
         */
        mmu->bus_access(mmu->priv, curr_priv, bus_read_access, pte_addr, &pte, sizeof(rv_uint_xlen));
        // pte = mmu->read_mem(NULL, pte_addr, sizeof(rv_uint_xlen), &err);
        MMU_DEBUG("pte[%d] "PRINTF_FMT"\n", i, pte);
        pte_flags = pte;

        /* 
         * 3. If pte.v = 0, or if pte.r = 0 and pte.w = 1, stop and raise a page-fault exception.
         */
        if( (!(pte_flags & MMU_PAGE_VALID)) || ((!(pte_flags & MMU_PAGE_READ)) && (pte_flags & MMU_PAGE_WRITE)) )
        {
            MMU_DEBUG("page fault: pte.v = 0, or if pte.r = 0 and pte.w = 1\n");
            goto exit_page_fault;
        }

        /*
         * 4. Otherwise, the PTE is valid. If pte.r = 1 or pte.x = 1, go to step 5. Otherwise, this PTE is a
         * pointer to the next level of the page table. Let i = i − 1. If i < 0, stop and raise a page-fault
         * exception. Otherwise, let a = pte.ppn × PAGESIZE and go to step 2.
         */

        /* check if any RWX flag is set */
        if(pte_flags & 0xE)
        {
            MMU_DEBUG("Leaf pte %d\n", i);
            break;
        }
    }

    if(i<0)
    {
        MMU_DEBUG("page fault: i < 0\n");
        goto exit_page_fault;
    }

    /*
     * 5. A leaf PTE has been found. Determine if the requested memory access is allowed by the
     * pte.r, pte.w, pte.x, and pte.u bits, given the current privilege mode and the value of the SUM
     * and MXR fields of the mstatus register. If not, stop and raise a page-fault exception.
     */
    user_page = pte_flags & MMU_PAGE_USER;

    /* User has only access to user pages */
    if ( (curr_priv == user_mode) && !user_page)
    {
        MMU_DEBUG("page fault: user access to higher priv page!\n");
        goto exit_page_fault;
    }

    /* Supervisor only has access to user pages if SUM = 1 */
    if( (curr_priv == supervisor_mode) && user_page && !sum )
    {
        MMU_DEBUG("page fault: supervisor access to user page!\n");
        goto exit_page_fault;
    }

    /* Check if MXR */
    if((access_type == bus_read_access) && (ACCESS_TYPE_TO_MMU(bus_instr_access) & pte_flags ) && mxr )
        pte_flags |= MMU_PAGE_READ;

    if(!(ACCESS_TYPE_TO_MMU(access_type) & pte_flags ))
    {
        MMU_DEBUG("page fault: invalid RWX flags!\n");
        goto exit_page_fault;
    }

    MMU_DEBUG("virt addr: %x\n", virt_addr);
    MMU_DEBUG("pte: "PRINTF_FMT"\n", pte);
    MMU_DEBUG("level: %x\n", i);

    pte = pte << SV32_PTESHIFT;
    /* physical addresses are 34 Bit wide!!! even on RV32 systems */
    rv_uint_xlen ppn[SV32_LEVELS] = 
    {
        (pte >> 12) & 0x3ff,
        (pte >> 22) & 0xfff
    };

    MMU_DEBUG("ppn[1]: %x\n", ppn[1]);
    MMU_DEBUG("ppn[0]: %x\n", ppn[0]);

    /* physical addresses are at least 34 Bits wide, so we need uint64_t here */
    uint64_t phys_addr_translation[SV32_LEVELS] = 
    {
        (ppn[1] << 22) | (ppn[0] << 12) | (virt_addr & 0xfff),
        (ppn[1] << 22) | (virt_addr & 0x3fffff)
    };

    /*
     * 6. If i > 0 and pa.ppn[i − 1 : 0] != 0, this is a misaligned superpage; stop and raise a page-fault exception.
     */
    if(i > 0 && ppn[i-1] != 0)
    {
        MMU_DEBUG("misaligned superpage!\n");
        goto exit_page_fault;
    }

    /* This check here is disabled for now, as qemu also don't seem to care about this */
    #if 0
    /*
     * 7. If pte.a = 0, or if the memory access is a store and pte.d = 0, either raise a page-fault exception or:
     *  - Set pte.a to 1 and, if the memory access is a store, also set pte.d to 1.
     *  - If this access violates a PMA or PMP check, raise an access exception.
     *  - This update and the loading of pte in step 2 must be atomic; in particular, no intervening store to the PTE may be perceived to have occurred in-between.
     */
    if( (!(pte_flags & MMU_PAGE_ACCESSED)) || ((access_type == bus_write_access) && !(pte_flags & MMU_PAGE_DIRTY)) )
    {
        MMU_DEBUG("pta.a or pte.d page fault!\n");
        goto exit_page_fault;
    }
    #endif

    /*
     * 8. The translation is successful. The translated physical address is given as follows:
     * - pa.pgoff = va.pgoff.
     * - If i > 0, then this is a superpage translation and pa.ppn[i − 1 : 0] = va.vpn[i − 1 : 0].
     * - pa.ppn[LEVELS − 1 : i] = pte.ppn[LEVELS − 1 : i].
     */
    MMU_DEBUG("translated addr: virt: %x phys: %lx\n", virt_addr, phys_addr_translation[i]);
    return phys_addr_translation[i];

    exit_page_fault:
        *ret_val = mmu_page_fault;
        return 0;
}

void mmu_dump(mmu_td *mmu)
{
    printf("satp_reg: " PRINTF_FMT"\n", mmu->satp_reg);
}

void mmu_init(mmu_td *mmu, bus_access_func bus_access, void *priv)
{
    memset(mmu, 0, sizeof(mmu_td));

    mmu->bus_access = bus_access;
    mmu->priv = priv;
}
