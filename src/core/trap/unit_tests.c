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

void test_TRAP_set_pending_bit(void)
{
    /* test if pending bits are set correctly */
    trap_set_pending_bits(&trap, machine_mode, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0, *trap.m.regs[trap_reg_ip]);

    /* enable MTIP */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);
    trap_set_pending_bits(&trap, machine_mode, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0x80, *trap.m.regs[trap_reg_ip]);

    /* also enable STIP */
    *trap.s.regs[trap_reg_ie] |= (1<<5);
    trap_set_pending_bits(&trap, supervisor_mode, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0xA0, *trap.s.regs[trap_reg_ip]);
}

void test_TRAP_set_and_clear_pending_bit(void)
{
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);
    trap_set_pending_bits(&trap, machine_mode, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0x80, *trap.m.regs[trap_reg_ip]);

    trap_clear_pending_bits(&trap, machine_mode, 0, 1, 0);
    TEST_ASSERT_EQUAL_HEX(0x00, *trap.m.regs[trap_reg_ip]);

    *trap.s.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);
    trap_set_pending_bits(&trap, supervisor_mode, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0x20, *trap.s.regs[trap_reg_ip]);

    trap_clear_pending_bits(&trap, supervisor_mode, 0, 1, 0);
    TEST_ASSERT_EQUAL_HEX(0x00, *trap.s.regs[trap_reg_ip]);

    /* now enable different kind of interrupts */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_swi) | GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);
    trap_set_pending_bits(&trap, machine_mode, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0x88, *trap.m.regs[trap_reg_ip]);

    /* clear swi bits, the supervisor ti bits should still be active afterwards */
    trap_clear_pending_bits(&trap, machine_mode, 0, 0, 1);
    TEST_ASSERT_EQUAL_HEX(0x80, *trap.m.regs[trap_reg_ip]);

    /* now clear remaining ti bits */
    trap_clear_pending_bits(&trap, machine_mode, 0, 1, 0);
    TEST_ASSERT_EQUAL_HEX(0x00, *trap.m.regs[trap_reg_ip]);
}

void test_TRAP_check_irq_pending(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti);

    /* enable interrupts globally for machine mode */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode);

    /* enable interrupts globally for machine mode */
    *trap.m.regs[trap_reg_status] |= GET_GLOBAL_IRQ_BIT(supervisor_mode);

    // printf("%x\n", *trap.m.regs[trap_reg_ie]);
    // printf("%x\n", *trap.m.regs[trap_reg_status]);

    /* set irq pending */
    trap_set_pending_bits(&trap, machine_mode, 1, 0, 0);
    trap_set_pending_bits(&trap, supervisor_mode, 1, 0, 0);

    // printf("%x\n", *trap.m.regs[trap_reg_ip]);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, machine_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, supervisor_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, user_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
}

void test_TRAP_check_irq_pending_priv_level(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti);

    /* enable SEI */
    *trap.s.regs[trap_reg_ie] |= GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_exti);

    /* enable interrupts globally */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode);
    *trap.s.regs[trap_reg_status] |= GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 1, 0, 0);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, machine_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, supervisor_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, user_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
}

void test_TRAP_delegation(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI and SEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);
    *trap.s.regs[trap_reg_ie] |= GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor */
    *trap.m.regs[trap_reg_ideleg] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode);
    *trap.m.regs[trap_reg_status] |= GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 0, 1, 0);

    /* check if a machine mode irq is pending */
    /* this should return pending, as delegation will only happen if the current mode is lower than the target mode */
    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, machine_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    /* another test to check if an machine mode irq is delegated, our mode is supervisor */
    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, machine_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_delegated, trap_ret_val);

    /* the above should give 'irq_delegated' now check if is delegated further or if supervisor can handle the irq */
    trap_ret_val = trap_check_interrupt_pending(&trap, supervisor_mode, supervisor_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
}

void test_TRAP_delegation_till_user(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI, SEI and UEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                                GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                                GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor and further to user */
    *trap.m.regs[trap_reg_ideleg] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                                    GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 0, 1, 0);

    /* another test to check if an machine mode irq is delegated, our mode is user */
    trap_ret_val = trap_check_interrupt_pending(&trap, user_mode, machine_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_delegated, trap_ret_val);

    /* the above should give 'irq_delegated' now check if is delegated further or if supervisor can handle the irq */
    trap_ret_val = trap_check_interrupt_pending(&trap, user_mode, supervisor_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_delegated, trap_ret_val);

    /* the above should give 'irq_delegated' now check if is delegated further or if user can handle the irq */
    trap_ret_val = trap_check_interrupt_pending(&trap, user_mode, user_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
}

void test_TRAP_delegation_till_user_with_user_deleg(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI, SEI and UEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor and further to user, also set delegation at user level and see what happens */
    *trap.m.regs[trap_reg_ideleg] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 0, 1, 0);

    /* another test to check if an machine mode irq is delegated, our mode is user */
    trap_ret_val = trap_check_interrupt_pending(&trap, user_mode, machine_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_delegated, trap_ret_val);

    /* the above should give 'irq_delegated' now check if is delegated further or if supervisor can handle the irq */
    trap_ret_val = trap_check_interrupt_pending(&trap, user_mode, supervisor_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_delegated, trap_ret_val);

    /* the above should give 'irq_delegated' now check if is delegated further or if user can handle the irq */
    trap_ret_val = trap_check_interrupt_pending(&trap, user_mode, user_mode, trap_type_ti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
}

void test_TRAP_get_pending_irq_level(void)
{
    /* test the convinience function 'trap_check_interrupt_level' */
    trap_ret trap_ret_val = 0;
    privilege_level serving_priv_level = 0;

    /* enable MEI, SEI and UEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor and further to user */
    *trap.m.regs[trap_reg_ideleg] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 0, 1, 0);

    /* we should have a pending irq and the serving level should be supervisor */
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    /* no irq pending on exti */
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_exti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
    /* if not pending priv_level_unknown should be returned */
    TEST_ASSERT_EQUAL(priv_level_unknown, serving_priv_level);

    /* clear irq pending */
    trap_clear_pending_bits_all_levels(&trap, 0, 1, 0);
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
    TEST_ASSERT_EQUAL(priv_level_unknown, serving_priv_level);

    /* Now also delegate to user */
    *trap.m.regs[trap_reg_ideleg] |= GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);
    trap_set_pending_bits_all_levels(&trap, 0, 0, 1);
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_none, trap_ret_val);
    TEST_ASSERT_EQUAL(priv_level_unknown, serving_priv_level);

    /* set ti pending again */
    trap_set_pending_bits_all_levels(&trap, 0, 1, 1);
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    /* as the current level is supervisor it should not go down to user */
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    /* if current mode is user the serving mode should also be user, as we delegated it down to user */
    trap_ret_val = trap_check_interrupt_level(&trap, user_mode, trap_type_ti, &serving_priv_level);
    /* as the current level is supervisor it should not go down to user */
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
    TEST_ASSERT_EQUAL(user_mode, serving_priv_level);
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
    serving_priv_level = trap_check_exception_delegation(&trap, supervisor_mode, trap_cause_load_fault);
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

void test_TRAP_interrupt_enable_priv_level(void)
{
    privilege_level restored_priv_level = 0;

    /* enable MEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti);

    /* enable interrupts globally */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 1, 0, 0);

    // printf("status initial: %x\n", *trap.m.regs[trap_reg_status]);

    TEST_ASSERT_EQUAL_HEX32(0xb, *trap.m.regs[trap_reg_status]);
    trap_serve_interrupt(&trap, machine_mode, user_mode);
    TEST_ASSERT_EQUAL_HEX32(0x83, *trap.m.regs[trap_reg_status]);
    // printf("stored status: %x\n", *trap.m.regs[trap_reg_status]);

    restored_priv_level = trap_restore_irq_settings(&trap, machine_mode);
    TEST_ASSERT_EQUAL_HEX32(0xb, *trap.m.regs[trap_reg_status]);
    TEST_ASSERT_EQUAL(user_mode, restored_priv_level);
    // printf("restored status: %x\n", *trap.m.regs[trap_reg_status]);

    // TEST_ASSERT_EQUAL_HEX32(0xa, trap.trap_setup.status);
    trap_serve_interrupt(&trap, machine_mode, supervisor_mode);
    TEST_ASSERT_EQUAL_HEX32(0x883, *trap.m.regs[trap_reg_status]);
    // printf("stored status: %x\n", *trap.m.regs[trap_reg_status]);

    restored_priv_level = trap_restore_irq_settings(&trap, machine_mode);
    TEST_ASSERT_EQUAL_HEX32(0x80b, *trap.m.regs[trap_reg_status]);
    TEST_ASSERT_EQUAL(supervisor_mode, restored_priv_level);
    // printf("restored status: %x\n", *trap.m.regs[trap_reg_status]);

    // TEST_ASSERT_EQUAL_HEX32(0xa, trap.trap_setup.status);
    trap_serve_interrupt(&trap, machine_mode, machine_mode);
    TEST_ASSERT_EQUAL_HEX32(0x1883, *trap.m.regs[trap_reg_status]);
    // printf("stored status: %x\n", *trap.m.regs[trap_reg_status]);

    restored_priv_level = trap_restore_irq_settings(&trap, machine_mode);
    TEST_ASSERT_EQUAL_HEX32(0x180b, *trap.m.regs[trap_reg_status]);
    TEST_ASSERT_EQUAL(machine_mode, restored_priv_level);
    // printf("restored status: %x\n", *trap.m.regs[trap_reg_status]);
}

void test_TRAP_interrupt_nesting_priv_level(void)
{
    privilege_level restored_priv_level = 0;

    /* enable MEI */
    *trap.m.regs[trap_reg_ie] = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti);

    /* enable interrupts globally */
    *trap.m.regs[trap_reg_status] = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits_all_levels(&trap, 1, 0, 0);

    // printf("status initial: %x\n", *trap.m.regs[trap_reg_status]);

    TEST_ASSERT_EQUAL_HEX32(0xb, *trap.m.regs[trap_reg_status]);

    /* assume we are currently in supervisor mode and an irq comes up which shall be served in supervisor mode */
    trap_serve_interrupt(&trap, supervisor_mode, supervisor_mode);
    TEST_ASSERT_EQUAL_HEX32(0x129, *trap.m.regs[trap_reg_status]);
    // printf("supervisor_mode stored status: %x\n", *trap.m.regs[trap_reg_status]);

    /* ther running irq in supervisor_mode will then be interrupted by an IRQ in machine_mode */
    trap_serve_interrupt(&trap, machine_mode, supervisor_mode);
    TEST_ASSERT_EQUAL_HEX32(0x9a1, *trap.m.regs[trap_reg_status]);
    // printf("machine_mode stored status: %x\n", *trap.m.regs[trap_reg_status]);

    /* restore the previous supervisor_mode IRQ context here */
    restored_priv_level = trap_restore_irq_settings(&trap, machine_mode);
    TEST_ASSERT_EQUAL_HEX32(0x929, *trap.m.regs[trap_reg_status]);
    TEST_ASSERT_EQUAL(supervisor_mode, restored_priv_level);
    // printf("machine restored status: %x\n", *trap.m.regs[trap_reg_status]);

    /* finish the supervisor IRQ which was interrupted before */
    restored_priv_level = trap_restore_irq_settings(&trap, supervisor_mode);
    TEST_ASSERT_EQUAL_HEX32(0x90b, *trap.m.regs[trap_reg_status]);
    TEST_ASSERT_EQUAL(supervisor_mode, restored_priv_level);
    // printf("supervisor_mode restored status: %x\n", *trap.m.regs[trap_reg_status]);


    /* now test another context: currently running in user_mode and IRQ is served by supervisor */
    trap_serve_interrupt(&trap, supervisor_mode, user_mode);
    TEST_ASSERT_EQUAL_HEX32(0x829, *trap.m.regs[trap_reg_status]);

    /* restore user mode from supervisor context */
    restored_priv_level = trap_restore_irq_settings(&trap, supervisor_mode);
    TEST_ASSERT_EQUAL_HEX32(0x80b, *trap.m.regs[trap_reg_status]);
    TEST_ASSERT_EQUAL(user_mode, restored_priv_level);
}

int main() 
{
    UnityBegin("trap/unit_tests.c");
    RUN_TEST(test_TRAP_set_pending_bit, __LINE__);
    RUN_TEST(test_TRAP_set_and_clear_pending_bit, __LINE__);
    RUN_TEST(test_TRAP_check_irq_pending, __LINE__);
    RUN_TEST(test_TRAP_check_irq_pending_priv_level, __LINE__);
    RUN_TEST(test_TRAP_delegation, __LINE__);
    RUN_TEST(test_TRAP_delegation_till_user, __LINE__);
    RUN_TEST(test_TRAP_delegation_till_user_with_user_deleg, __LINE__);
    RUN_TEST(test_TRAP_get_pending_irq_level, __LINE__);
    RUN_TEST(test_TRAP_get_exception_level, __LINE__);
    RUN_TEST(test_TRAP_interrupt_enable_priv_level, __LINE__);
    RUN_TEST(test_TRAP_interrupt_nesting_priv_level, __LINE__);

    return (UnityEnd());
}