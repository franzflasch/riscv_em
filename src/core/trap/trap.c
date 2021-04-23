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
    trap->m.regs[trap_reg_ideleg] = &trap->regs_data.m_setup.ideleg;
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
    trap->s.regs[trap_reg_ideleg] = &trap->regs_data.s_setup.ideleg;
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
    trap->u.regs[trap_reg_ideleg] = &trap->regs_data.u_setup.ideleg;
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

rv_ret trap_m_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val)
{
    (void)curr_priv;
    trap_td *trap = priv;
    *trap->m.regs[reg_index] = csr_val;
    // printf("val written %d "PRINTF_FMT"\n", reg_index, *trap->m.regs[reg_index]);
    return rv_ok;
}

rv_ret trap_m_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    trap_td *trap = priv;
    *out_val = *trap->m.regs[reg_index];

    // if(reg_index==trap_reg_scratch)
    // {
    //     printf("scratch!!! %x\n", *out_val );
    // }

    // printf("m read! %d %x\n", reg_index, *out_val);
    return rv_ok;
}

rv_ret trap_s_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val)
{
    (void)curr_priv;
    trap_td *trap = priv;
    *trap->s.regs[reg_index] = csr_val;
    // printf("val written %x\n", trap->regs[internal_reg]);
    return rv_ok;
}

rv_ret trap_s_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    trap_td *trap = priv;
    *out_val = *trap->s.regs[reg_index];
    return rv_ok;
}

rv_ret trap_u_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val)
{
    (void)curr_priv;
    trap_td *trap = priv;
    *trap->u.regs[reg_index] = csr_val;
    // printf("val written %x\n", trap->regs[internal_reg]);
    return rv_ok;
}

rv_ret trap_u_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val)
{
    (void)curr_priv_mode;
    trap_td *trap = priv;
    *out_val = *trap->u.regs[reg_index];
    return rv_ok;
}

void trap_set_pending_bits(trap_td *trap, uint8_t ext_int, uint8_t tim_int, uint8_t sw_int)
{
    /* IRQ coming from the clint are only assigned to machine mode bits, all
       other bits are actually pure SW interrupts, set for e.g. by m-mode context
     */
    trap_regs_p_td *x = get_priv_regs(trap, machine_mode);

    /* "Additionally, the platformlevel interrupt controller may generate supervisor-level external interrupts. The logical-OR of the
    software-writeable bit and the signal from the external interrupt controller is used to generate
    external interrupts to the supervisor." */

    // /* Get seip, if it is possibly set to 1 by SW */
    // uint8_t seip = CHECK_BIT(*x->regs[trap_reg_ip], trap_cause_super_exti) ? 1 : 0;
    // /* now OR it with ext_int */
    // seip |= ext_int;

    if(CHECK_BIT(*x->regs[trap_reg_ie], trap_cause_machine_exti))
        assign_xlen_bit(x->regs[trap_reg_ip], trap_cause_machine_exti, ext_int);

    /* Special seip handling */
    if(CHECK_BIT(*x->regs[trap_reg_ie], trap_cause_super_exti))
        assign_xlen_bit(x->regs[trap_reg_ip], trap_cause_super_exti, ext_int);

    if(CHECK_BIT(*x->regs[trap_reg_ie], trap_cause_machine_ti))
        assign_xlen_bit(x->regs[trap_reg_ip], trap_cause_machine_ti, tim_int);

    if(CHECK_BIT(*x->regs[trap_reg_ie], trap_cause_machine_swi))
        assign_xlen_bit(x->regs[trap_reg_ip], trap_cause_machine_swi, sw_int);
}

trap_ret trap_check_interrupt_pending(trap_td *trap, privilege_level curr_priv_mode, trap_cause_interrupt irq, privilege_level *serving_priv_level )
{
    rv_uint_xlen interrupt_bit = (1 << irq);
    privilege_level delegation_level = 0;
    trap_regs_p_td *x = NULL;
    trap_regs_p_td *deleg_register_set = NULL;
    *serving_priv_level = machine_mode;

    x = get_priv_regs(trap, curr_priv_mode);

    /* check if an interrupt is pending and if it is even enabled. 
     * we only need to do this once for all priv levels, as
     * these registers are shared accross all levels
     */
    if( !(*x->regs[trap_reg_ip] & interrupt_bit ) || 
        !(*x->regs[trap_reg_ie] & interrupt_bit ) )
        return trap_ret_none;

    /* If we come here than there is an interrupt pending, now check if it is delegated to lower priv levels */
    for(delegation_level=machine_mode;delegation_level>=curr_priv_mode;delegation_level--)
    {
        if(delegation_level == reserved_mode)
            continue;

        deleg_register_set = get_priv_regs(trap, delegation_level);

        /* check which level is intended to serve the interrupt */
        if( !( *deleg_register_set->regs[trap_reg_ideleg] & interrupt_bit) )
            break;
    }

    // printf("delegation level %d\n", delegation_level);

    /* if we are on a lower priv level and
     * the lower xIE is globally enabled, we need to handle the IRQ
     * on a higher level. (except if they are delegated)
     */
    if( /* First we check if we need to handle this irq. It depends on the xIE bits and the current privilege level
         * we do this by shifting the mask 0xF0 towards the xIE bits */ 
        ( (0xF0 >> (priv_level_max - curr_priv_mode)) & (*x->regs[trap_reg_status] & 0xF) ) & 
        /* The first check might result in a true but it could still be possible that it is delegated
         * and not handled by the curr_priv, in this case we do not serve any interrupt */
        ( 0xF >> (machine_mode - delegation_level) ) 
      )
    {
        *serving_priv_level = delegation_level;
        return trap_ret_irq_pending;   
    }

    return trap_ret_none;
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

        (void) curr_priv_mode;
        // if( curr_priv_mode >= priv_index )
        //     break;
    }

    return priv_index;
}

rv_uint_xlen trap_serve_interrupt(trap_td *trap, 
                                  privilege_level serving_priv_mode, 
                                  privilege_level previous_priv_mode, 
                                  rv_uint_xlen is_interrupt,
                                  rv_uint_xlen cause,
                                  rv_uint_xlen curr_pc,
                                  rv_uint_xlen tval)
{
    rv_uint_xlen ie = 0;
    trap_regs_p_td *x = get_priv_regs(trap, serving_priv_mode);

    // trap_regs_p_td *tmp = get_priv_regs(trap, machine_mode);
    // printf("serve interrupt status: %lx\n", *tmp->regs[trap_reg_status]);

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

    /* Also set tval */
    *x->regs[trap_reg_tval] = tval;

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
