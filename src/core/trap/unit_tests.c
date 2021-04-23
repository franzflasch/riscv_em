#include <stdio.h>
#include <string.h>

#include <trap.h>
#include <riscv_helper.h>

#include <unity.h>

trap_td trap;

void setUp(void)
{
    memset(&trap, 0, sizeof(trap));
    trap_init(&trap);
}

void tearDown(void)
{
}

void test_TRAP_simple_irq(void)
{
    privilege_level serving_priv_level = 0;
    trap_ret trap_ret_val = 0;
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode);
    *trap.m.regs[trap_reg_ie] = (1<< trap_cause_machine_swi);

    trap_set_pending_bits(&trap, 0, 0, 1);

    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, trap_cause_machine_swi, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
    TEST_ASSERT_EQUAL(machine_mode, serving_priv_level);
}

void test_TRAP_delegate_to_supervisor_with_sie_disabled(void)
{
    privilege_level serving_priv_level = 0;
    trap_ret trap_ret_val = 0;

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ideleg] = (1<< trap_cause_super_ti);

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ie] = (1<< trap_cause_super_ti);

    /* set irq pending */
    *trap.m.regs[trap_reg_ip] = (1<< trap_cause_super_ti);

    /* enable interrupts globally for supervisor */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode);

    /* when in supervisor mode no IRQ should be pending */
    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
}

void test_TRAP_delegate_to_supervisor_with_sie_enabled(void)
{
    privilege_level serving_priv_level = 0;
    trap_ret trap_ret_val = 0;

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ideleg] = (1<< trap_cause_super_ti);

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ie] = (1<< trap_cause_super_ti);

    /* set irq pending */
    *trap.m.regs[trap_reg_ip] = (1<< trap_cause_super_ti);

    /* enable interrupts globally for supervisor */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* with SIE enabled and calling from supervisor mode an supervisor level irq should be handled */
    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    /* same settings, but when in machine mode no IRQ should be pending */
    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
}

void test_TRAP_delegate_to_supervisor_with_mie_enabled(void)
{
    privilege_level serving_priv_level = 0;
    trap_ret trap_ret_val = 0;

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ideleg] = (1<< trap_cause_super_ti);

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ie] = (1<< trap_cause_super_ti);

    /* set irq pending */
    *trap.m.regs[trap_reg_ip] = (1<< trap_cause_super_ti);

    /* enable interrupts globally for supervisor */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode);

    /* no IRQ should be pending */
    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);

    /* also no IRQ when on supervisor mode, as SIE is disabled */
    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
}

void test_TRAP_sie_enabled_trigger_from_machine_mode(void)
{
    privilege_level serving_priv_level = 0;
    trap_ret trap_ret_val = 0;

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ie] = (1<< trap_cause_super_ti);

    /* set irq pending */
    *trap.m.regs[trap_reg_ip] = (1<< trap_cause_super_ti);

    /* enable interrupts globally for supervisor */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* check if a machine mode irq is pending */
    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
}

void test_TRAP_sie_enabled_trigger_from_supervisor_mode(void)
{
    privilege_level serving_priv_level = 0;
    trap_ret trap_ret_val = 0;

    /* enable mti interrupt */
    *trap.m.regs[trap_reg_ie] = (1<< trap_cause_super_ti);

    /* set irq pending */
    *trap.m.regs[trap_reg_ip] = (1<< trap_cause_super_ti);

    /* enable interrupts globally for supervisor */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* There should be an IRQ when called from supervisor context */
    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, trap_cause_super_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(machine_mode, serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
}

void test_TRAP_get_exception_level(void)
{
    /* test the convinience function 'trap_check_interrupt_level' */
    privilege_level serving_priv_level = 0;

    /* delegate an exception to supervisor */
    *trap.m.regs[trap_reg_edeleg] = GET_EXCEPTION_BIT(trap_cause_illegal_instr);

    /* serving level should be supervisor */
    serving_priv_level = trap_check_exception_delegation(&trap, supervisor_mode, trap_cause_illegal_instr);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    /* any other exception should be served by machine mode */
    serving_priv_level = trap_check_exception_delegation(&trap, supervisor_mode, trap_cause_load_access_fault);
    TEST_ASSERT_EQUAL(machine_mode, serving_priv_level);

    /* serving level should be supervisor */
    serving_priv_level = trap_check_exception_delegation(&trap, user_mode, trap_cause_illegal_instr);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    *trap.m.regs[trap_reg_edeleg] = GET_EXCEPTION_BIT(trap_cause_store_amo_addr_fault);
    *trap.s.regs[trap_reg_edeleg] = GET_EXCEPTION_BIT(trap_cause_store_amo_addr_fault);

    /* serving level should be user */
    serving_priv_level = trap_check_exception_delegation(&trap, user_mode, trap_cause_store_amo_addr_fault);
    TEST_ASSERT_EQUAL(user_mode, serving_priv_level);

    /* serving level should be supervisor */
    serving_priv_level = trap_check_exception_delegation(&trap, supervisor_mode, trap_cause_store_amo_addr_fault);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    /* serving level should be machine */
    serving_priv_level = trap_check_exception_delegation(&trap, machine_mode, trap_cause_store_amo_addr_fault);
    TEST_ASSERT_EQUAL(machine_mode, serving_priv_level);
}

// void test_TRAP_interrupt_nesting_priv_level(void)
// {
//     privilege_level restored_priv_level = 0;

//     /* enable MEI */
//     *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti);

//     /* enable interrupts globally */
//     *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

//     /* set irq pending */
//     trap_set_pending_bits_all_levels(&trap, 1, 0, 0);

//     printf("status initial: %x\n", *trap.m.regs[trap_reg_status]);

//     TEST_ASSERT_EQUAL_HEX32(0xb, *trap.m.regs[trap_reg_status]);

//     /* assume we are currently in supervisor mode and an irq comes up which shall be served in supervisor mode */
//     trap_serve_interrupt(&trap, supervisor_mode, supervisor_mode, 0, 0, 0);
//     TEST_ASSERT_EQUAL_HEX32(0x129, *trap.m.regs[trap_reg_status]);
//     // printf("supervisor_mode stored status: %x\n", *trap.m.regs[trap_reg_status]);

//     /* ther running irq in supervisor_mode will then be interrupted by an IRQ in machine_mode */
//     trap_serve_interrupt(&trap, machine_mode, supervisor_mode, 0, 0, 0);
//     TEST_ASSERT_EQUAL_HEX32(0x9a1, *trap.m.regs[trap_reg_status]);
//     // printf("machine_mode stored status: %x\n", *trap.m.regs[trap_reg_status]);

//     /* restore the previous supervisor_mode IRQ context here */
//     restored_priv_level = trap_restore_irq_settings(&trap, machine_mode);
//     TEST_ASSERT_EQUAL_HEX32(0x929, *trap.m.regs[trap_reg_status]);
//     TEST_ASSERT_EQUAL(supervisor_mode, restored_priv_level);
//     // printf("machine restored status: %x\n", *trap.m.regs[trap_reg_status]);

//     /* finish the supervisor IRQ which was interrupted before */
//     restored_priv_level = trap_restore_irq_settings(&trap, supervisor_mode);
//     TEST_ASSERT_EQUAL_HEX32(0x90b, *trap.m.regs[trap_reg_status]);
//     TEST_ASSERT_EQUAL(supervisor_mode, restored_priv_level);
//     // printf("supervisor_mode restored status: %x\n", *trap.m.regs[trap_reg_status]);


//     /* now test another context: currently running in user_mode and IRQ is served by supervisor */
//     trap_serve_interrupt(&trap, supervisor_mode, user_mode, 0, 0, 0);
//     TEST_ASSERT_EQUAL_HEX32(0x829, *trap.m.regs[trap_reg_status]);

//     /* restore user mode from supervisor context */
//     restored_priv_level = trap_restore_irq_settings(&trap, supervisor_mode);
//     TEST_ASSERT_EQUAL_HEX32(0x80b, *trap.m.regs[trap_reg_status]);
//     TEST_ASSERT_EQUAL(user_mode, restored_priv_level);
// }

int main() 
{
    UnityBegin("trap/unit_tests.c");
    RUN_TEST(test_TRAP_simple_irq, __LINE__);
    RUN_TEST(test_TRAP_delegate_to_supervisor_with_sie_disabled, __LINE__);
    RUN_TEST(test_TRAP_delegate_to_supervisor_with_sie_enabled, __LINE__);
    RUN_TEST(test_TRAP_delegate_to_supervisor_with_mie_enabled, __LINE__);
    RUN_TEST(test_TRAP_sie_enabled_trigger_from_machine_mode, __LINE__);
    RUN_TEST(test_TRAP_sie_enabled_trigger_from_supervisor_mode, __LINE__);
    RUN_TEST(test_TRAP_get_exception_level, __LINE__);
    // RUN_TEST(test_TRAP_interrupt_enable_priv_level, __LINE__);
    // RUN_TEST(test_TRAP_interrupt_nesting_priv_level, __LINE__);

    return (UnityEnd());
}