#include <stdio.h>
#include <string.h>

#include <pmp.h>
#include <riscv_helper.h>

// #define PMP_DEBUG
#ifdef PMP_DEBUG
#define PMP_DEBUG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define PMP_DEBUG(...) do{ } while ( 0 )
#endif

int pmp_write_csr(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val)
{
    uint8_t i = 0;
    pmp_td *pmp = priv;
    uint8_t *cfg_ptr = (uint8_t *)&pmp->regs[reg_index];

    /* All PMP cfgs can only be changed in machine mode */
    if(curr_priv != machine_mode)
        return RV_MEM_ACCESS_ERR;

    /* assume all writable at first */
    rv_uint_xlen access_mask = -1;
    uint8_t *access_mask_ptr = (uint8_t *)&access_mask;

    for(i=0;i<sizeof(rv_uint_xlen);i++)
    {
        /* check if it is locked, this can only be cleared by a reset */
        if(CHECK_BIT(cfg_ptr[i], PMP_CFG_L_BIT))
            access_mask_ptr[i] = 0;
    }

    PMP_DEBUG("Access mask: " PRINTF_FMT "\n", access_mask);

    /* OK now we can savely update the config */
    pmp->regs[reg_index] |= csr_val & access_mask;

    return RV_MEM_ACCESS_OK;
}

static inline rv_uint_xlen get_pmp_napot_size_from_pmpaddr(rv_uint_xlen addr)
{
    /* +2 because it was shifted by 2 with encoding */
    return (1 << (FIND_FIRST_BIT_SET(~addr) + 2) );
}

static inline rv_uint_xlen get_pmp_napot_addr_from_pmpaddr(rv_uint_xlen addr)
{
    rv_uint_xlen size = get_pmp_napot_size_from_pmpaddr(addr);
    rv_uint_xlen mask = ((size/2) - 1) >> 2;
    return (addr - mask) << 2;
}

int pmp_mem_check(pmp_td *pmp, privilege_level curr_priv, rv_uint_xlen addr)
{
    /* check if the address matches any enabled config */
    /* lower cfgs have precedence over higher ones */
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t *cfg_ptr = NULL;
    pmp_addr_matching addr_mode = pmp_a_off;
    rv_uint_xlen addr_start = 0;
    rv_uint_xlen addr_size = 0;

    /* We don't have to do any check if we are in machine mode */
    if(curr_priv == machine_mode)
        return RV_MEM_ACCESS_OK;

    for(i=0;i<PMP_NR_CFG_REGS;i++)
    {
        cfg_ptr = (uint8_t *)&pmp->cfg[i];
        for(j=0;j<sizeof(pmp->cfg[0]);j++)
        {
            addr_mode = extract8(cfg_ptr[j], PMP_CFG_A_BIT_OFFS, 2);
            if(!addr_mode)
                continue;

            switch(addr_mode)
            {
                case pmp_a_tor:
                    if(j==0)
                    {
                        addr_start = 0;
                        addr_size = (pmp->addr[j] << 2);
                    }
                    else
                    {
                        addr_start = (pmp->addr[j-1] << 2);
                        addr_size = (pmp->addr[j-1] << 2);
                    }
                break;
                case pmp_a_napot:
                    addr_size = get_pmp_napot_size_from_pmpaddr(pmp->addr[j]);
                    addr_start = get_pmp_napot_addr_from_pmpaddr(pmp->addr[j]);
                break;
                case pmp_a_na4:
                    addr_start = (pmp->addr[j] << 2);
                    addr_size = 4;
                break;
                default:
                break;
            }

            PMP_DEBUG("addr: " PRINTF_FMT "\n", addr_start);
            PMP_DEBUG("size: " PRINTF_FMT "\n", addr_size);

            if(ADDR_WITHIN(addr, addr_start, addr_size))
            {
                PMP_DEBUG("Address OK!\n");
                return RV_MEM_ACCESS_OK;
            }
        }
    }

    PMP_DEBUG("No PMP match found!\n");
    return RV_MEM_ACCESS_ERR;
}

void pmp_dump_cfg_regs(pmp_td *pmp)
{
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t cnt = 0;
    uint8_t *cfg_ptr = NULL;
    (void) cfg_ptr;

    PMP_DEBUG("===== CFG REG DUMP =====\n");

    for(i=0;i<PMP_NR_CFG_REGS;i++)
    {
        PMP_DEBUG("REG %d:\n", i);
        cfg_ptr = (uint8_t *)&pmp->cfg[i];
        for(j=0;j<sizeof(pmp->cfg[0]);j++)
        {
            PMP_DEBUG("pmpcfg %d: %02x\n", cnt, cfg_ptr[j]);
            cnt++;
        }
        PMP_DEBUG("\n");
    }

    PMP_DEBUG("\n");
}
