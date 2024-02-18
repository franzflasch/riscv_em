#include <stdio.h>
#include <string.h>

#include <mmu.h>
#include <riscv_helper.h>

#define MMU_DEBUG_ENABLE
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

#if 0
// #define ENABLE_DIRTY
uint64_t mmu_virt_to_phys(mmu_td *mmu, 
                          privilege_level curr_priv, 
                          rv_uint_xlen virt_addr, 
                          bus_access_type access_type, 
                          uint8_t mxr, 
                          uint8_t sum, 
                          mmu_ret *ret_val,
                          rv_core_td *rv_core,
                          rv_uint_xlen value)
{

    #define PTE_ADDRESS_BITS 22
    #define PAGE_SHIFT	(12)
    #define PAGE_SIZE	((1UL) << PAGE_SHIFT)
    #define PAGE_MASK	(~(PAGE_SIZE - 1))
    #define PTE_V_MASK (1 << 0)
    #define PTE_W 2
    #define PTE_XW 6
    #define _PAGE_PRESENT   (1 << 0)
    #define _PAGE_READ      (1 << 1)    /* Readable */
    #define _PAGE_WRITE     (1 << 2)    /* Writable */
    #define _PAGE_EXEC      (1 << 3)    /* Executable */
    #define _PAGE_USER      (1 << 4)    /* User */
    #define _PAGE_GLOBAL    (1 << 5)    /* Global */
    #define _PAGE_ACCESSED  (1 << 6)    /* Set by hardware on any access */
    #define _PAGE_DIRTY     (1 << 7)    /* Set by hardware on any write */
    #define _PAGE_SOFT      (1 << 8)    /* Reserved for software */


    (void) mxr;
    (void) sum;
    (void) access_type;
    (void) ret_val;
    (void) rv_core;
    (void) value;


// int translate_address(State * state, word virtual_address, enum access_type access_type, word * physical_address) {
	//in machine mode no translation is happening
    /* in machine mode we don't have address translation */
    uint8_t mode = extractxlen(mmu->satp_reg, MMU_SATP_MODE_BIT, MMU_SATP_MODE_NR_BITS);
    if( (curr_priv == machine_mode) || !mode )
    {
        *ret_val = mmu_ok;
        return virt_addr;
    }

	//supervisor mode
		//TODO it would be a good idea to decode satp into a structure
		//An Sv32 virtual address  is  partitioned  into  a virtual  page  number  (VPN)  and  page  offset,
		//bits 0..11=offset, 12..21=vpn[0], 22..31=vpn[1]

		// rv_uint_xlen pte_addr = (mmu->satp_reg & 0x3FFFFF) * SV32_PAGE_SIZE;
        int pte_addr = (mmu->satp_reg & (((uint32_t)1 << PTE_ADDRESS_BITS) - 1)) << PAGE_SHIFT;

		int pte_bits = 12 - 2;
		int pte_mask = (1 << pte_bits) - 1;

		int levels = 2; //for sv32	
		//Let pte be the value of the PTE at address a+va.vpn[i]×PTESIZE.
		uint64_t pte;
		uint64_t paddr;

		for (int i = 0; i < levels; i++) {
			int vaddr_shift = PAGE_SHIFT + pte_bits * (levels - 1 - i);
			int pte_idx = (virt_addr >> vaddr_shift) & pte_mask;
			pte_addr += pte_idx << 2;
			// pte = read_word_physical(state, pte_addr);
            if(mmu->bus_access(mmu->priv, curr_priv, bus_read_access, pte_addr, &pte, sizeof(rv_uint_xlen)) != rv_ok)
                printf("mmu bus access err!\n");

			//the V bit indicates whether the PTE is valid
			//If pte.v= 0, or if pte.r= 0 and pte.w= 1, stop and raise a page-fault exception.
			if (!(pte & PTE_V_MASK))
            {
                // printf("page fault PTE_V_MASK! %x %x\n", pte_addr, virt_addr);
                *ret_val = mmu_page_fault;
				return 0; /* invalid PTE */
            }
			//get the physical address bit
			paddr = (pte >> 10) << PAGE_SHIFT;
			rv_uint_xlen xwr = (pte >> 1) & 7;
			if (xwr != 0) {
				//writable pages must also be marked readable (no W without R)
				if (xwr == PTE_W || xwr == PTE_XW)
                {
                    *ret_val = mmu_page_fault;
                    return 0; /* invalid PTE */
                }
				//TODO privilege check against PMP

				//when a virtual page is accessed and the A bit is clear, or is written and the D bit is clear, a page-fault exception is raised
				// rv_uint_xlen accessed = pte & _PAGE_ACCESSED;
				// rv_uint_xlen dirty = (access_type == bus_write_access) * _PAGE_DIRTY;

				//update accessed flag on all accesses and dirty flag on store
				rv_uint_xlen accessed_or_dirty_flags = _PAGE_ACCESSED | (access_type == bus_write_access? _PAGE_DIRTY : 0);
				if ((pte & accessed_or_dirty_flags) != accessed_or_dirty_flags) {
#ifdef ENABLE_DIRTY
					pte |= accessed_or_dirty_flags;
					//update pte 
					// write_word_physical(state, pte_addr, pte);
                    if(mmu->bus_access(mmu->priv, curr_priv, bus_write_access, pte_addr, &pte, sizeof(rv_uint_xlen)) != rv_ok)
                        printf("mmu bus access err!\n");

#else
                    *ret_val = mmu_page_fault;
					return 0;
#endif
				}


				rv_uint_xlen vaddr_mask = ((rv_uint_xlen)1 << vaddr_shift) - 1;
				//add the virtual address offset
				rv_uint_xlen result = (virt_addr & vaddr_mask) | (paddr & ~vaddr_mask);
				*ret_val = mmu_ok;
				return result;
			}
			else { //R is 0 or X is 0, this is a pointer to the next level
				pte_addr = paddr; //go one level down
			}
		}
		//
		//TODO refactor around this to allow for more "direct" read, or another "overload" on read_common with forced mode
		//MemoryTarget bare = get_memory_target_bare(state, pte_addr);
		//word value = read_common_ram(state, bare.ptr, SIZE_WORD);

		// When Sv32 virtual memory mode is selected in the MODE field of the satp register,
		//supervisor virtual addresses are translated into supervisor physical addresses via a two-level page table.   
		//The  20-bit  VPN  is  translated  into  a  22-bit  physical  page  number  (PPN),  
		//while  the  12-bit page offset is untranslated.  
		//The resulting supervisor-level physical addresses are then checkedusing any physical memory 
		//protection structures (Sections 3.6), before being directly converted to machine-level physical addresses.
		//printf("pte value: 0x%x", pte_value);
		//physical_address = -1; //raise page fault

    *ret_val = mmu_ok;
	return 0;
}
#endif

#if 1
#define MAX_LEVELS 4
uint64_t mmu_virt_to_phys(mmu_td *mmu, 
                          privilege_level curr_priv, 
                          rv_uint_xlen virt_addr, 
                          bus_access_type access_type, 
                          uint8_t mxr, 
                          uint8_t sum, 
                          mmu_ret *ret_val,
                          rv_core_td *rv_core,
                          rv_uint_xlen value)
{
    /* We only have these here for debug purposes */
    (void) value;
    (void) rv_core;

    int i,j = 0;
    rv_uint_xlen a = 0;
    rv_uint_xlen pte = 0;
    rv_uint_xlen pte_addr = 0;
    uint8_t pte_flags = 0;
    uint8_t user_page = 0;
    *ret_val = mmu_ok;
    uint8_t mode = extractxlen(mmu->satp_reg, MMU_SATP_MODE_BIT, MMU_SATP_MODE_NR_BITS);

    MMU_DEBUG("priv=%i, mode=%i: %lx\n", curr_priv, mode, virt_addr);
    
    /* in machine mode we don't have address translation */
    if( (curr_priv == machine_mode) || !mode )
    {
        MMU_DEBUG("=> machine mode\n");
        return virt_addr;
    }
    
    rv_uint_xlen vpn[MAX_LEVELS] = {0};
    uint8_t levels = 0;
    uint16_t page_size = 0;
    uint16_t pte_size = 0;
    uint8_t bits_per_level = 0;
    if (mode == supervisor_mode) {
        vpn[0] = (virt_addr >> 12) & 0x3ff;
        vpn[1] = (virt_addr >> 22) & 0x3ff;
        levels = SV32_LEVELS;
        page_size = SV32_PAGE_SIZE;
        pte_size = SV32_PTESIZE;
        bits_per_level = 10;
    } else if (mode == supervisor_39_mode) {
        vpn[0] = (virt_addr >> 12) & 0x1ff;
        vpn[1] = (virt_addr >> 21) & 0x1ff;
        vpn[2] = (virt_addr >> 30) & 0x1ff;
        levels = SV39_LEVELS; 
        page_size = SV39_PAGE_SIZE;
        pte_size = SV39_PTESIZE;
        bits_per_level = 9;
    }

    MMU_DEBUG("vpn[2] "PRINTF_FMT"\n", vpn[2]);
    MMU_DEBUG("vpn[1] "PRINTF_FMT"\n", vpn[1]);
    MMU_DEBUG("vpn[0] "PRINTF_FMT"\n", vpn[0]);

    /*
     * 1. Let a be satp.ppn × PAGESIZE, and let i = LEVELS − 1. (For Sv32, PAGESIZE=2^12 and LEVELS=2.) 
     */
    a = (mmu->satp_reg & 0x3FFFFF) * page_size;
    MMU_DEBUG("satp: %lx\n", mmu->satp_reg);

    for(i=(levels-1),j=0;i>=0;i--,j++)
    {
        /*
        * 2. Let pte be the value of the PTE at address a+va.vpn[i]×PTESIZE. (For Sv32, PTESIZE=4.)
        * If accessing pte violates a PMA or PMP check, raise an access exception.
        */
        pte_addr = a + (vpn[i] * pte_size);
        MMU_DEBUG("address a: " PRINTF_FMT " pte_addr: "PRINTF_FMT"\n", a, pte_addr);

        /* Here we should raise an exception if PMP violation occurs, will be done automatically
         * if read_mem is set to the "checked_read_mem()" function.
         */
        mmu->bus_access(mmu->priv, curr_priv, bus_read_access, pte_addr, &pte, sizeof(rv_uint_xlen));
        MMU_DEBUG("pte[%d] "PRINTF_FMT"\n", i, pte);
        pte_flags = pte;

        /* 
         * 3. If pte.v = 0, or if pte.r = 0 and pte.w = 1, stop and raise a page-fault exception.
         */
        if( (!(pte_flags & MMU_PAGE_VALID)) || ((!(pte_flags & MMU_PAGE_READ)) && (pte_flags & MMU_PAGE_WRITE)) )
        {
            MMU_DEBUG("page fault: pte.v = 0, or if pte.r = 0 and pte.w = 1 access_type: %d a: "PRINTF_FMT" virt_addr: "PRINTF_FMT" pte: "PRINTF_FMT" pte_addr: "PRINTF_FMT" flags: %x curr_priv: %d level: %d pc: "PRINTF_FMT" value: "PRINTF_FMT"\n", access_type, a, virt_addr, pte, pte_addr, pte_flags, curr_priv, i, rv_core->pc, value);
            goto exit_page_fault;
        }

        /*
         * 4. Otherwise, the PTE is valid. If pte.r = 1 or pte.x = 1, go to step 5. Otherwise, this PTE is a
         * pointer to the next level of the page table. Let i = i − 1. If i < 0, stop and raise a page-fault
         * exception. Otherwise, let a = pte.ppn × PAGESIZE and go to step 2.
         */
        /* check if any RWX flag is set */
        if(pte_flags & 0xA)
        {
            MMU_DEBUG("Leaf pte %d\n", i);
            break;
        }

        a = (pte >> bits_per_level) * page_size;
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
        printf("page fault: user access to higher priv page!\n");
        goto exit_page_fault;
    }

    /* Supervisor only has access to user pages if SUM = 1 */
    if(   (   curr_priv == supervisor_mode
           || curr_priv == supervisor_39_mode)
       && user_page
       && !sum )
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

    /* physical addresses are 34 Bit wide!!! even on RV32 systems */
    rv_uint_xlen ppn[MAX_LEVELS] = {0};
    if (mode == supervisor_mode) {
       pte = pte << SV32_PTESHIFT;
       ppn[0] = (pte >> 12) & 0x3ff; // 10 bits
       ppn[1] = (pte >> 22) & 0xfff; // What's going on here? Should be 10 bits here too, to make up 20 bits total (10 to 30).
    } else if (mode == supervisor_39_mode) {
        pte = pte << SV39_PTESHIFT;
        ppn[0] = (pte >> 12) & 0x1ff; // 9 bits
        ppn[1] = (pte >> 21) & 0x1ff; // 9 bits
        ppn[2] = (pte >> 30) & 0x1ff; // 9 bits
    };
    MMU_DEBUG("ppn[2]: %lx\n", ppn[2]);
    MMU_DEBUG("ppn[1]: %lx\n", ppn[1]);
    MMU_DEBUG("ppn[0]: %lx\n", ppn[0]);

    /* physical addresses are at least 34 Bits wide, so we need uint64_t here */
    uint64_t phys_addr_translation[MAX_LEVELS];
    if (mode == supervisor_mode) {
        phys_addr_translation[0] = (ppn[1] << 22) | (ppn[0] << 12) | (virt_addr & 0xfff),
        phys_addr_translation[1] = (ppn[1] << 22) | (virt_addr & 0x3fffff);
    } else if (mode == supervisor_39_mode) {
        phys_addr_translation[0] = (ppn[2] << 30) | (ppn[1] << 21) | (ppn[0] << 12) | (virt_addr & 0xfff);
        phys_addr_translation[1] = (ppn[2] << 30) | (ppn[1] << 21) | (virt_addr & 0x1fffff);
        phys_addr_translation[1] = (ppn[2] << 30) | (virt_addr & 0x3fffffff);
    }
    
    /*
     * 6. If i > 0 and pa.ppn[i − 1 : 0] != 0, this is a misaligned superpage; stop and raise a page-fault exception.
     */
    if(i > 0 && ppn[i-1] != 0)
    {
        MMU_DEBUG("misaligned superpage!\n");
        goto exit_page_fault;
    }

    #if 1
        /*
        * 7. If pte.a = 0, or if the memory access is a store and pte.d = 0, either raise a page-fault exception or:
        *  - Set pte.a to 1 and, if the memory access is a store, also set pte.d to 1.
        *  - If this access violates a PMA or PMP check, raise an access exception.
        *  - This update and the loading of pte in step 2 must be atomic; in particular, no intervening store to the PTE may be perceived to have occurred in-between.
        */
        if( (!(pte_flags & MMU_PAGE_ACCESSED)) || ((access_type == bus_write_access) && !(pte_flags & MMU_PAGE_DIRTY)) )
        {
            // printf("pta.a or pte.d page fault!\n");
            goto exit_page_fault;
        }
    #endif

    /*
     * 8. The translation is successful. The translated physical address is given as follows:
     * - pa.pgoff = va.pgoff.
     * - If i > 0, then this is a superpage translation and pa.ppn[i − 1 : 0] = va.vpn[i − 1 : 0].
     * - pa.ppn[LEVELS − 1 : i] = pte.ppn[LEVELS − 1 : i].
     */
    if(access_type == bus_instr_access)
    {
        mmu->last_phys_pc = phys_addr_translation[i];
        mmu->last_virt_pc = virt_addr;
    }

    return phys_addr_translation[i];

    exit_page_fault:
        // printf("page fault!!!\n");
        *ret_val = mmu_page_fault;
        return 0;
}
#endif

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
