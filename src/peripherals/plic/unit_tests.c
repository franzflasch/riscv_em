#include <stdio.h>
#include <string.h>

#include <plic.h>
#include <riscv_helper.h>

#include <unity.h>

plic_td plic = {0};

void setUp(void)
{
    memset(&plic, 0, sizeof(plic_td));
}

void tearDown(void)
{
}

void test_PLIC_read_write_reg(void)
{
    rv_uint_xlen tmp_val = 0;
    rv_uint_xlen tmp_read_val = 0;

    // printf("%p %p\n", get_u8_reg_ptr(&plic, 0x1004), (void *)&plic.pending_bits[1]);

    /* There are only 7 prio levels, so ensure that when writing 0x42 there should be only 0x2 set */
    tmp_val = 42;
    plic_write_reg(&plic, 0x8, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x8, &tmp_read_val);
    TEST_ASSERT_EQUAL_HEX(0x2, tmp_read_val);
    // printf("RESULT %x "PRINTF_FMT"\n", plic.priority[2], tmp_read_val);

    /* Pending bit registers start at 0x1000 */
    tmp_val = 43;
    plic_write_reg(&plic, 0x1004, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x1004, &tmp_read_val);
    TEST_ASSERT_EQUAL(43, tmp_read_val);
    // printf("RESULT %x "PRINTF_FMT"\n", plic.pending_bits[1], tmp_read_val);

    /* Enable bits start at 0x2000 */
    tmp_val = 44;
    plic_write_reg(&plic, 0x2004, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x2004, &tmp_read_val);
    TEST_ASSERT_EQUAL(44, tmp_read_val);
    // printf("RESULT %x "PRINTF_FMT"\n", plic.enable_bits[1], tmp_read_val);

    /* Threshold register - max level 7 */
    tmp_val = 45;
    plic_write_reg(&plic, 0x200000, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x200000, &tmp_read_val);
    TEST_ASSERT_EQUAL(5, tmp_read_val);
    // printf("RESULT %x "PRINTF_FMT"\n", plic.priority_threshold, tmp_read_val);

    /* Claim registers */
    tmp_val = 46;
    plic_write_reg(&plic, 0x200004, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    TEST_ASSERT_EQUAL(46, tmp_read_val);
    // printf("RESULT %x "PRINTF_FMT"\n", plic.claim_complete, tmp_read_val);
}

void test_PLIC_test_interrupts(void)
{
    rv_uint_xlen tmp_val = 0;
    rv_uint_xlen tmp_read_val = 0;

    plic.priority[10] = 7;
    plic.enable_bits[0] = (1<<10);
    plic.priority_threshold = 0;
    TEST_ASSERT_EQUAL(0, plic_update(&plic));
    // plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");

    /* now set pending */
    plic.pending_bits[0] |= (1<<10);
    TEST_ASSERT_EQUAL(1, plic_update(&plic));
    // plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");

    /* test priorities */
    plic.priority[12] = 5;
    plic.enable_bits[0] |= (1<<12);
    plic.pending_bits[0] |= (1<<12);
    TEST_ASSERT_EQUAL_HEX(0x1400, plic.pending_bits[0]);
    TEST_ASSERT_EQUAL_HEX(0x1400, plic.enable_bits[0]);
    // printf("pending %x\n", plic.enable_bits[0]);

    plic.priority[37] = 6;
    plic.enable_bits[1] |= (1<<5);
    plic.pending_bits[1] |= (1<<5);
    TEST_ASSERT_EQUAL_HEX(0x20, plic.enable_bits[1]);
    TEST_ASSERT_EQUAL(1, plic_update(&plic));
    // printf("pending %x\n", plic.enable_bits[1]);
    // plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");

    /* now claim the interrupt and check if gets triggered again */
    TEST_ASSERT_EQUAL(1, plic_update(&plic));
    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    /* Highest prio should be claimed first */
    TEST_ASSERT_EQUAL(10, tmp_read_val);
    // plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");

    /* Next should be 37 */
    TEST_ASSERT_EQUAL(1, plic_update(&plic));
    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    TEST_ASSERT_EQUAL(37, tmp_read_val);
    // plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");

    /* Last should be 12 */
    TEST_ASSERT_EQUAL(1, plic_update(&plic));
    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    TEST_ASSERT_EQUAL(12, tmp_read_val);

    /* Now all interrupts should be claimed so no one pending anymore */
    TEST_ASSERT_EQUAL(0, plic_update(&plic));
}

int main() 
{
    UnityBegin("plic/unit_tests.c");
    RUN_TEST(test_PLIC_read_write_reg, __LINE__);
    RUN_TEST(test_PLIC_test_interrupts, __LINE__);

    return (UnityEnd());
}