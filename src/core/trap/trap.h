/* Trap setup and trap handling registers seem to be
 * shared across privilege levels but with different
 * access permissions. So we try to unify those registers here.
 */
#ifndef RISCV_TRAP_H
#define RISCV_TRAP_H

#include <stdint.h>
#include <riscv_types.h>

#define TRAP_REG_STATUS 0
#define TRAP_REG_ISA 1
#define TRAP_REG_EDELEG 2
#define TRAP_REG_IDELEG 3
#define TRAP_REG_IE 4
#define TRAP_REG_TVEC 5
#define TRAP_COUNTEREN 6

#define TRAP_REG_SCRATCH     0x40
#define TRAP_REG_EPC         0x41
#define TRAP_REG_CAUSE       0x42
#define TRAP_REG_TVAL        0x43
#define TRAP_REG_IP          0x44

// /* XSTATUS BITS */
#define TRAP_XSTATUS_MIE_BIT 3
// #define TRAP_XSTATUS_MIE_MASK 0x1
#define TRAP_XSTATUS_MPIE_BIT 7
#define TRAP_XSTATUS_UPIE_BIT 4
#define TRAP_XSTATUS_SPP_BIT 8
#define TRAP_XSTATUS_MPP_BIT 11 /* and 12 */
// #define TRAP_XSTATUS_MPP_MASK 0x3
#define TRAP_XSTATUS_MPRV_BIT 17
#define TRAP_XSTATUS_SPIE_BIT 5
#define TRAP_XSTATUS_SIE_BIT 1
// #define TRAP_XSTATUS_SIE_MASK 0x1

#define GET_GLOBAL_IRQ_BIT(priv_level) (1<<priv_level)
#define GET_LOCAL_IRQ_BIT(priv_level, trap_type) ( (1<<(priv_level_max*trap_type)) << priv_level )
#define GET_EXCEPTION_BIT(trap_cause_exception) ( 1 << trap_cause_exception )

typedef enum
{
    trap_cause_user_swi = 0,
    trap_cause_super_swi,
    trap_cause_rsvd_0,
    trap_cause_machine_swi,

    trap_cause_user_ti,
    trap_cause_super_ti,
    trap_cause_rsvd_1,
    trap_cause_machine_ti,

    trap_cause_user_exti,
    trap_cause_super_exti,
    trap_cause_rsvd_2,
    trap_cause_machine_exti

} trap_cause_interrupt;

typedef enum
{
    trap_cause_instr_misalign = 0,
    trap_cause_access_fault,
    trap_cause_illegal_instr,
    trap_cause_break,
    trap_cause_load_misalign,
    trap_cause_load_fault,
    trap_cause_store_amo_addr_fault,
    trap_cause_store_amo_access_fault,
    trap_cause_user_ecall,
    trap_cause_super_ecall,
    trap_cause_rsvd_3,
    trap_cause_machine_ecall,
    trap_cause_instr_page_fault,
    trap_cause_rsvd_4,
    trap_cause_store_amo_page_fault

} trap_cause_exception;

typedef enum
{
    trap_reg_status = 0,
    trap_reg_isa,
    trap_reg_edeleg,
    trap_reg_ideleg,
    trap_reg_ie,
    trap_reg_tvec,
    trap_reg_counteren,
    trap_reg_scratch,
    trap_reg_epc,
    trap_reg_cause,
    trap_reg_tval,
    trap_reg_ip,

    trap_reg_max

} trap_internal_regs;

typedef enum
{
    trap_type_unknown = -1,
    trap_type_swi = 0,
    trap_type_ti,
    trap_type_exti,

} trap_irq_type;

typedef enum
{
    trap_ret_none = 0,
    trap_ret_irq_pending,
    trap_ret_irq_delegated,

} trap_ret;

typedef struct trap_shared_struct
{
    rv_uint_xlen status;
    rv_uint_xlen ideleg;
    rv_uint_xlen ie;
    rv_uint_xlen ip;

} trap_shared_td;

typedef struct trap_setup_struct
{
    rv_uint_xlen isa;
    rv_uint_xlen edeleg; 
    rv_uint_xlen tvec;
    rv_uint_xlen counteren;

} trap_setup_td;

typedef struct trap_handling_struct
{
    rv_uint_xlen scratch;
    rv_uint_xlen epc;
    rv_uint_xlen cause;
    rv_uint_xlen tval;

} trap_handling_td;

typedef struct trap_regs_struct
{
    /* shared registers, with restricted views depending on the priv level */
    trap_shared_td shared;

    /* machine regs */
    trap_setup_td m_setup;
    trap_handling_td m_handling;

    /* supervisor regs */
    trap_setup_td s_setup;
    trap_handling_td s_handling;

    /* user regs */
    trap_setup_td u_setup;
    trap_handling_td u_handling;

} trap_regs_td;

typedef struct trap_regs_p_struct
{
    rv_uint_xlen *regs[trap_reg_max];

} trap_regs_p_td;

typedef struct trap_struct
{
    trap_regs_td regs_data;
    trap_regs_p_td m;
    trap_regs_p_td s;
    trap_regs_p_td u;

} trap_td;

void trap_init(trap_td *trap);

int trap_m_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask);
int trap_m_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val);

int trap_s_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask);
int trap_s_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val);

int trap_u_write(void *priv, privilege_level curr_priv, uint16_t reg_index, rv_uint_xlen csr_val, rv_uint_xlen mask);
int trap_u_read(void *priv, privilege_level curr_priv_mode, uint16_t reg_index, rv_uint_xlen *out_val);

void trap_set_pending_bits(trap_td *trap, privilege_level priv_level, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int);
void trap_set_pending_bits_all_levels(trap_td *trap, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int);
void trap_clear_pending_bits(trap_td *trap, privilege_level priv_level, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int);
void trap_clear_pending_bits_all_levels(trap_td *trap, uint8_t ext_int, uint8_t sw_int, uint8_t tim_int);

trap_ret trap_check_interrupt_pending(trap_td *trap, privilege_level curr_priv_mode, privilege_level target_priv_mode, trap_irq_type type);
trap_ret trap_check_interrupt_level(trap_td *trap, privilege_level curr_priv_mode, trap_irq_type type, privilege_level *ret_priv_mode);
privilege_level trap_check_exception_delegation(trap_td *trap, privilege_level curr_priv_mode, trap_cause_exception cause);
void trap_serve_interrupt(trap_td *trap, 
                          privilege_level serving_priv_mode, 
                          privilege_level previous_priv_mode, 
                          rv_uint_xlen is_interrupt,
                          rv_uint_xlen cause,
                          rv_uint_xlen *pc);

privilege_level trap_restore_irq_settings(trap_td *trap, privilege_level serving_priv_mode);

#endif /* RISCV_TRAP_H */
