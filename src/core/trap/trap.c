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

static inline trap_regs_p_td *get_priv_regs(trap_td *trap, privilege_level priv_level)
{
    switch(priv_level)
    {
        case machine_mode:
            return &trap->m;
            break;
        case supervisor_mode:
            return &trap->s;
            break;
        case user_mode:
            return &trap->u;
            break;
        default:
            return NULL;
    }
}

void trap_init(trap_td *trap)
{
    /* machine setup regs */
    trap->m.regs[trap_reg_status] = &trap->regs_data.shared.status;
    trap->m.regs[trap_reg_isa] = &trap->regs_data.m_setup.isa;
    trap->m.regs[trap_reg_edeleg] = &trap->regs_data.m_setup.edeleg;
    trap->m.regs[trap_reg_ideleg] = &trap->regs_data.shared.ideleg;
    trap->m.regs[trap_reg_ie] = &trap->regs_data.shared.ie;
    trap->m.regs[trap_reg_tvec] = &trap->regs_data.m_setup.tvec;
    trap->m.regs[trap_reg_counteren] = &trap->regs_data.m_setup.counteren;

    /* machine handling regs */
    trap->m.regs[trap_reg_scratch] = &trap->regs_data.m_handling.scratch;
    trap->m.regs[trap_reg_epc] = &trap->regs_data.m_handling.epc;
    trap->m.regs[trap_reg_cause] = &trap->regs_data.m_handling.cause;
    trap->m.regs[trap_reg_tval] = &trap->regs_data.m_handling.tval;
    trap->m.regs[trap_reg_ip] = &trap->regs_data.shared.ip;

    /* supervisor setup regs */
    trap->s.regs[trap_reg_status] = &trap->regs_data.shared.status;
    trap->s.regs[trap_reg_isa] = &trap->regs_data.s_setup.isa;
    trap->s.regs[trap_reg_edeleg] = &trap->regs_data.s_setup.edeleg;
    trap->s.regs[trap_reg_ideleg] = &trap->regs_data.shared.ideleg;
    trap->s.regs[trap_reg_ie] = &trap->regs_data.shared.ie;
    trap->s.regs[trap_reg_tvec] = &trap->regs_data.s_setup.tvec;
    trap->s.regs[trap_reg_counteren] = &trap->regs_data.s_setup.counteren;

    /* supervisor handling regs */
    trap->s.regs[trap_reg_scratch] = &trap->regs_data.s_handling.scratch;
    trap->s.regs[trap_reg_epc] = &trap->regs_data.s_handling.epc;
    trap->s.regs[trap_reg_cause] = &trap->regs_data.s_handling.cause;
    trap->s.regs[trap_reg_tval] = &trap->regs_data.s_handling.tval;
    trap->s.regs[trap_reg_ip] = &trap->regs_data.shared.ip;

    /* user setup regs */
    trap->u.regs[trap_reg_status] = &trap->regs_data.shared.status;
    trap->u.regs[trap_reg_isa] = &trap->regs_data.u_setup.isa;
    trap->u.regs[trap_reg_edeleg] = &trap->regs_data.u_setup.edeleg;
    trap->u.regs[trap_reg_ideleg] = &trap->regs_data.shared.ideleg;
    trap->u.regs[trap_reg_ie] = &trap->regs_data.shared.ie;
    trap->u.regs[trap_reg_tvec] = &trap->regs_data.u_setup.tvec;
    trap->u.regs[trap_reg_counteren] = &trap->regs_data.u_setup.counteren;

    /* user handling regs */
    trap->u.regs[trap_reg_scratch] = &trap->regs_data.u_handling.scratch;
    trap->u.regs[trap_reg_epc] = &trap->regs_data.u_handling.epc;
    trap->u.regs[trap_reg_cause] = &trap->regs_data.u_handling.cause;
    trap->u.regs[trap_reg_tval] = &trap->regs_data.u_handling.tval;
    trap->u.regs[trap_reg_ip] = &trap->regs_data.shared.ip;
}

int trap_m_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask)
{
    (void)curr_priv;
    trap_td *trap = priv;
    *trap->m.regs[reg_index] = csr_val & mask;
    // printf("val written %d "PRINTF_FMT"\n", reg_index, *trap->m.regs[reg_index]);
    return RV_ACCESS_OK;
}

int trap_m_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    trap_td *trap = priv;
    *out_val = *trap->m.regs[reg_index];
    // printf("m read! %d %x\n", reg_index, *out_val);
    return RV_ACCESS_OK;
}

int trap_s_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask)
{
    (void)curr_priv;
    trap_td *trap = priv;
    *trap->s.regs[reg_index] = csr_val & mask;
    // printf("val written %x\n", trap->regs[internal_reg]);
    return RV_ACCESS_OK;
}

int trap_s_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    trap_td *trap = priv;
    *out_val = *trap->s.regs[reg_index];
    return RV_ACCESS_OK;
}

int trap_u_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask)
{
    (void)curr_priv;
    trap_td *trap = priv;
    *trap->u.regs[reg_index] = csr_val & mask;
    // printf("val written %x\n", trap->regs[internal_reg]);
    return RV_ACCESS_OK;
}

int trap_u_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    trap_td *trap = priv;
    *out_val = *trap->u.regs[reg_index];
    return RV_ACCESS_OK;
}

void trap_set_pending_bits(trap_td *trap, privilege_level priv_level, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int)
{
    trap_regs_p_td *x = get_priv_regs(trap, priv_level);
    rv_uint_xlen irq_bit = 0;

    irq_bit = (trap_type_exti*priv_level_max) + priv_level;
    if(CHECK_BIT(*x->regs[trap_reg_ie], irq_bit))
        assign_xlen_bit(x->regs[trap_reg_ip], irq_bit, ext_int);

    irq_bit = (trap_type_ti*priv_level_max) + priv_level;
    if(CHECK_BIT(*x->regs[trap_reg_ie], irq_bit))
        assign_xlen_bit(x->regs[trap_reg_ip], irq_bit, tim_int);

    irq_bit = (trap_type_swi*priv_level_max) + priv_level;
    if(CHECK_BIT(*x->regs[trap_reg_ie], irq_bit))
        assign_xlen_bit(x->regs[trap_reg_ip], irq_bit, sw_int);
}

void trap_set_pending_bits_all_levels(trap_td *trap, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int)
{
    trap_set_pending_bits(trap, machine_mode, ext_int, sw_int, tim_int);
    trap_set_pending_bits(trap, supervisor_mode, ext_int, sw_int, tim_int);
    trap_set_pending_bits(trap, user_mode, ext_int, sw_int, tim_int);
}

void trap_clear_pending_bits(trap_td *trap, privilege_level priv_level, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int)
{
    rv_uint_xlen tmp = ( ((ext_int&1) << (8 + priv_level)) | 
                         ((tim_int&1) << (4 + priv_level)) | 
                         ((sw_int&1) << priv_level) );

    trap_regs_p_td *x = get_priv_regs(trap, priv_level);
    *x->regs[trap_reg_ip] &= ~tmp;
}

void trap_clear_pending_bits_all_levels(trap_td *trap, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int)
{
    trap_clear_pending_bits(trap, machine_mode, ext_int, sw_int, tim_int);
    trap_clear_pending_bits(trap, supervisor_mode, ext_int, sw_int, tim_int);
    trap_clear_pending_bits(trap, user_mode, ext_int, sw_int, tim_int);
}

trap_ret trap_check_interrupt_pending(trap_td *trap, privilege_level curr_priv_mode, privilege_level target_priv_mode, trap_irq_type type )
{
    rv_uint_xlen interrupt_bit = GET_LOCAL_IRQ_BIT(target_priv_mode, type);

    trap_regs_p_td *x = get_priv_regs(trap, curr_priv_mode);

    /* delegation will only be done if the currenc priv level is lower than the target priv level */
    if( (*x->regs[trap_reg_ideleg] & interrupt_bit) && 
        (curr_priv_mode < target_priv_mode) )
        return trap_ret_irq_delegated;

    /* check if an interrupt is pending */
    if( !(*x->regs[trap_reg_ip] & interrupt_bit ) )
        return trap_ret_none;

    /* check if interrupts are even enabled */
    if( !(*x->regs[trap_reg_status] & GET_GLOBAL_IRQ_BIT(target_priv_mode) ) ||
        !(*x->regs[trap_reg_ie] & interrupt_bit ) )
        return trap_ret_none;

    return trap_ret_irq_pending;
}

trap_ret trap_check_interrupt_level(trap_td *trap, privilege_level curr_priv_mode, trap_irq_type type, privilege_level *serving_priv_level)
{
    trap_ret ret_val = trap_ret_none;
    privilege_level priv_index = 0;

    /* First get the target privilege level of the interrupt */
    for(priv_index=machine_mode;priv_index>=user_mode;priv_index--)
    {
        if(priv_index == reserved_mode)
            continue;

        ret_val = trap_check_interrupt_pending(trap, curr_priv_mode, priv_index, type);

        if((ret_val == trap_ret_irq_pending) || (ret_val == trap_ret_none))
            break;
    }

    /* set target priv mode to the privilege level which is supposed to serve the IRQ */
    *serving_priv_level = (ret_val == trap_ret_irq_pending) ? priv_index : priv_level_unknown;
    return ret_val;
}

privilege_level trap_check_exception_delegation(trap_td *trap, privilege_level curr_priv_mode, trap_cause_exception cause)
{
    privilege_level priv_index = 0;
    rv_uint_xlen exception_bit = 0;

    trap_regs_p_td *x = NULL;

    for(priv_index=machine_mode;priv_index>=user_mode;priv_index--)
    {
        if(priv_index == reserved_mode)
            continue;

        x = get_priv_regs(trap, priv_index);

        exception_bit = (1 << cause);

        if( !(*x->regs[trap_reg_edeleg] & exception_bit) )
            break;
        
        if( curr_priv_mode >= priv_index )
            break;
    }

    return priv_index;
}

rv_uint_xlen trap_serve_interrupt(trap_td *trap, 
                                  privilege_level serving_priv_mode, 
                                  privilege_level previous_priv_mode, 
                                  rv_uint_xlen is_interrupt,
                                  rv_uint_xlen cause,
                                  rv_uint_xlen curr_pc)
{
    rv_uint_xlen ie = 0;
    trap_regs_p_td *x = get_priv_regs(trap, serving_priv_mode);

    *x->regs[trap_reg_epc] = is_interrupt ? curr_pc : (curr_pc - 4);
    *x->regs[trap_reg_cause] = ( (is_interrupt<<(XLEN-1)) | cause );

    /* "When a trap is taken from privilege mode y into privilege mode x, xPIE is set to the value of x IE; x IE is set to 0; and xPP is set to y."*/
    /* Save MPP and MIE */
    if(serving_priv_mode == machine_mode)
    {
        assign_xlen_value_within_reg(x->regs[trap_reg_status], TRAP_XSTATUS_MPP_BIT, previous_priv_mode, 0x3);
    }
    else if(serving_priv_mode == supervisor_mode)
    {
        assign_xlen_value_within_reg(x->regs[trap_reg_status], TRAP_XSTATUS_SPP_BIT, previous_priv_mode, 0x1);
    }

    ie = (*x->regs[trap_reg_status] >> serving_priv_mode) & 0x1;
    assign_xlen_bit(x->regs[trap_reg_status], TRAP_XSTATUS_UPIE_BIT + serving_priv_mode, ie);

    CLEAR_BIT(*x->regs[trap_reg_status], serving_priv_mode);

    return *x->regs[trap_reg_tvec];
}

privilege_level trap_restore_irq_settings(trap_td *trap, privilege_level serving_priv_mode)
{
    privilege_level previous_priv_level = 0;
    rv_uint_xlen pie = 0;

    trap_regs_p_td *x = get_priv_regs(trap, serving_priv_mode);

    /* Restore MPP and MIE */
    if(serving_priv_mode == machine_mode)
    {
        previous_priv_level = extractxlen(*x->regs[trap_reg_status], TRAP_XSTATUS_MPP_BIT, 2);
    }
    else if(serving_priv_mode == supervisor_mode)
    {
        previous_priv_level = extractxlen(*x->regs[trap_reg_status], TRAP_XSTATUS_SPP_BIT, 1);
    }

    pie = (*x->regs[trap_reg_status] >> (TRAP_XSTATUS_UPIE_BIT + serving_priv_mode)) & 0x1;
    assign_xlen_bit(x->regs[trap_reg_status], serving_priv_mode, pie);
    CLEAR_BIT(*x->regs[trap_reg_status], (TRAP_XSTATUS_UPIE_BIT + serving_priv_mode));

    return previous_priv_level;
}
