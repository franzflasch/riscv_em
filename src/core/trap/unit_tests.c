#include <stdio.h>
#include <string.h>

#include <trap.h>
#include <riscv_helper.h>

#include <unity.h>

trap_td trap;

void setUp(void)
{
    memset(&trap, 0, sizeof(trap));
}

void tearDown(void)
{
}

void test_TRAP_set_pending_bit(void)
{
    /* test if pending bits are set correctly */
    trap_set_pending_bits(&trap, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0, trap.regs[trap_reg_ip]);

    /* enable MTIP */
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);
    trap_set_pending_bits(&trap, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0x80, trap.regs[trap_reg_ip]);

    /* also enable STIP */
    trap.trap_setup.ie |= (1<<5);
    trap_set_pending_bits(&trap, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0xA0, trap.regs[trap_reg_ip]);
}

void test_TRAP_set_and_clear_pending_bit(void)
{
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);
    trap_set_pending_bits(&trap, 1, 1, 1);
    /* Only machine and supervisor ti bits should be set */
    TEST_ASSERT_EQUAL_HEX(0xA0, trap.regs[trap_reg_ip]);

    /* clear pending ti bits */
    trap_clear_pending_bits(&trap, 0, 1, 0);
    TEST_ASSERT_EQUAL_HEX(0x00, trap.regs[trap_reg_ip]);

    /* now enable different kind of interrupts */
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_swi) | GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);
    trap_set_pending_bits(&trap, 1, 1, 1);
    TEST_ASSERT_EQUAL_HEX(0x28, trap.regs[trap_reg_ip]);

    /* clear swi bits, the supervisor ti bits should still be active afterwards */
    trap_clear_pending_bits(&trap, 0, 0, 1);
    TEST_ASSERT_EQUAL_HEX(0x20, trap.regs[trap_reg_ip]);

    /* now clear remaining ti bits */
    trap_clear_pending_bits(&trap, 0, 1, 0);
    TEST_ASSERT_EQUAL_HEX(0x00, trap.regs[trap_reg_ip]);
}

void test_TRAP_check_irq_pending(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI */
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti);

    /* enable interrupts globally */
    trap.trap_setup.status = GET_GLOBAL_IRQ_BIT(machine_mode);

    /* set irq pending */
    trap_set_pending_bits(&trap, 1, 0, 0);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, machine_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, supervisor_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_none, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, user_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_none, trap_ret_val);
}

void test_TRAP_check_irq_pending_priv_level(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI */
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_exti) | GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_exti);

    /* enable interrupts globally */
    trap.trap_setup.status = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* set irq pending */
    trap_set_pending_bits(&trap, 1, 0, 0);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, machine_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, supervisor_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);

    trap_ret_val = trap_check_interrupt_pending(&trap, machine_mode, user_mode, trap_type_exti);
    TEST_ASSERT_EQUAL(trap_ret_irq_none, trap_ret_val);
}

void test_TRAP_delegation(void)
{
    trap_ret trap_ret_val = 0;

    /* enable MEI and SEI */
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor */
    trap.trap_setup.ideleg = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    trap.trap_setup.status = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode);

    /* set irq pending */
    trap_set_pending_bits(&trap, 0, 1, 0);

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
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor and further to user */
    trap.trap_setup.ideleg = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    trap.trap_setup.status = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits(&trap, 0, 1, 0);

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
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor and further to user, also set delegation at user level and see what happens */
    trap.trap_setup.ideleg = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    trap.trap_setup.status = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits(&trap, 0, 1, 0);

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
    trap.trap_setup.ie = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti) | 
                         GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);

    /* delegate timer interrupt to supervisor and further to user */
    trap.trap_setup.ideleg = GET_LOCAL_IRQ_BIT(machine_mode, trap_type_ti) | 
                             GET_LOCAL_IRQ_BIT(supervisor_mode, trap_type_ti);

    /* enable interrupts globally for machine and supervisor mode */
    trap.trap_setup.status = GET_GLOBAL_IRQ_BIT(machine_mode) | GET_GLOBAL_IRQ_BIT(supervisor_mode) | GET_GLOBAL_IRQ_BIT(user_mode);

    /* set irq pending */
    trap_set_pending_bits(&trap, 0, 1, 0);

    /* we should have a pending irq and the serving level should be supervisor */
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_pending, trap_ret_val);
    TEST_ASSERT_EQUAL(supervisor_mode, serving_priv_level);

    /* no irq pending on exti */
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_exti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_none, trap_ret_val);
    /* if not pending priv_level_unknown should be returned */
    TEST_ASSERT_EQUAL(priv_level_unknown, serving_priv_level);

    /* clear irq pending */
    trap_clear_pending_bits(&trap, 0, 1, 0);
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_none, trap_ret_val);
    TEST_ASSERT_EQUAL(priv_level_unknown, serving_priv_level);

    /* Now also delegate to user */
    trap.trap_setup.ideleg |= GET_LOCAL_IRQ_BIT(user_mode, trap_type_ti);
    trap_set_pending_bits(&trap, 0, 0, 1);
    trap_ret_val = trap_check_interrupt_level(&trap, supervisor_mode, trap_type_ti, &serving_priv_level);
    TEST_ASSERT_EQUAL(trap_ret_irq_none, trap_ret_val);
    TEST_ASSERT_EQUAL(priv_level_unknown, serving_priv_level);

    /* set ti pending again */
    trap_set_pending_bits(&trap, 0, 1, 1);
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

int main() 
{
    UnityBegin("trap/unit_tests.c");
    RUN_TEST(test_TRAP_set_pending_bit, __LINE__);
    RUN_TEST(test_TRAP_check_irq_pending, __LINE__);
    RUN_TEST(test_TRAP_check_irq_pending_priv_level, __LINE__);
    RUN_TEST(test_TRAP_delegation, __LINE__);
    RUN_TEST(test_TRAP_delegation_till_user, __LINE__);
    RUN_TEST(test_TRAP_delegation_till_user_with_user_deleg, __LINE__);
    RUN_TEST(test_TRAP_set_and_clear_pending_bit, __LINE__);
    RUN_TEST(test_TRAP_get_pending_irq_level, __LINE__);

    return (UnityEnd());
}