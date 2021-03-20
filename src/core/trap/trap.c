#include <stdio.h>
#include <string.h>

#include <trap.h>
#include <riscv_helper.h>

// #define TRAP_DEBUG_ENABLE
#ifdef TRAP_DEBUG_ENABLE
#define TRAP_DEBUG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define TRAP_DEBUG(...) do{ } while ( 0 )
#endif

/* Used to mask away the privilege of the CSR reg, like 0x342 (mcause), 0x142(scause) */
#define TRAP_REG_PRIV_LEVEL_MASK 0xFF

#define TRAP_IE_IP_MASK 0xB

int trap_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask)
{
    (void)curr_priv;

    trap_td *trap = priv;
    uint16_t internal_reg = (reg_index & TRAP_REG_PRIV_LEVEL_MASK);

    trap->regs[internal_reg] = csr_val & mask;

    // printf("val written %x\n", trap->regs[internal_reg]);

    return RV_ACCESS_OK;
}

int trap_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    (void)reg_index;

    trap_td *trap = priv;
    uint16_t internal_reg = (reg_index & TRAP_REG_PRIV_LEVEL_MASK);
    *out_val = trap->regs[internal_reg];

    return RV_ACCESS_OK;
}

void trap_set_pending_bits(trap_td *trap, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int)
{
    rv_uint_xlen interrupts_pending = (( (ext_int&1) * TRAP_IE_IP_MASK) << 8) | 
                                      (( (sw_int&1) * TRAP_IE_IP_MASK) << 4) | 
                                      ( (tim_int&1) * TRAP_IE_IP_MASK);
    trap->trap_handling.ip = interrupts_pending & trap->trap_setup.ie;
}

void trap_clear_pending_bits(trap_td *trap, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int)
{
    rv_uint_xlen tmp = (( (ext_int&1) * TRAP_IE_IP_MASK) << 8) | 
                       (( (sw_int&1) * TRAP_IE_IP_MASK) << 4) | 
                       (  (tim_int&1) * TRAP_IE_IP_MASK);;
    trap->trap_handling.ip &= ~tmp;
}

trap_ret trap_check_interrupt_pending(trap_td *trap, privilege_level curr_priv_mode, privilege_level target_priv_mode, trap_irq_type type )
{
    rv_uint_xlen interrupt_bit = GET_LOCAL_IRQ_BIT(target_priv_mode, type);

    /* delegation will only be done if the currenc priv level is lower than the target priv level */
    if( (trap->trap_setup.ideleg & interrupt_bit) && 
        (curr_priv_mode < target_priv_mode) )
        return trap_ret_irq_delegated;

    /* check if an interrupt is pending */
    if( !(trap->trap_handling.ip & interrupt_bit ) )
        return trap_ret_irq_none;

    /* check if interrupts are even enabled */
    if( !(trap->trap_setup.status & GET_GLOBAL_IRQ_BIT(target_priv_mode) ) ||
        !(trap->trap_setup.ie & interrupt_bit ) )
        return trap_ret_irq_none;

    return trap_ret_irq_pending;    
}

trap_ret trap_check_interrupt_level(trap_td *trap, privilege_level curr_priv_mode, trap_irq_type type, privilege_level *serving_priv_level)
{
    trap_ret ret_val = trap_ret_irq_none;
    privilege_level priv_index = 0;

    /* First get the target privilege level of the interrupt */
    for(priv_index=machine_mode;priv_index>=user_mode;priv_index--)
    {
        if(priv_index == reserved_mode)
            continue;

        ret_val = trap_check_interrupt_pending(trap, curr_priv_mode, priv_index, type);

        if((ret_val == trap_ret_irq_pending) || (ret_val == trap_ret_irq_none))
            break;
    }

    /* set target priv mode to the privilege level which is supposed to serve the IRQ */
    *serving_priv_level = (ret_val == trap_ret_irq_pending) ? priv_index : priv_level_unknown;
    return ret_val;
}

// void pmp_dump_cfg_regs(pmp_td *pmp)
// {
//     uint8_t i = 0;
//     uint8_t j = 0;
//     uint8_t cnt = 0;
//     uint8_t *cfg_ptr = NULL;
//     (void) cfg_ptr;

//     TRAP_DEBUG("===== CFG REG DUMP =====\n");

//     for(i=0;i<PMP_NR_CFG_REGS;i++)
//     {
//         TRAP_DEBUG("REG %d:\n", i);
//         cfg_ptr = (uint8_t *)&pmp->cfg[i];
//         for(j=0;j<sizeof(pmp->cfg[0]);j++)
//         {
//             TRAP_DEBUG("pmpcfg %d: %02x\n", cnt, cfg_ptr[j]);
//             cnt++;
//         }
//         TRAP_DEBUG("\n");
//     }

//     TRAP_DEBUG("\n");
// }
