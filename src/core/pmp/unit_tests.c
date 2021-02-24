#include <stdio.h>
#include <string.h>

#include <pmp.h>
#include <riscv_helper.h>

#include <unity.h>


/* these are internal functions, actually only used for testing
   the emulator will set those flags anyway using pmp_write_csr()
 */
static void pmp_set_cfg_l_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    SET_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_L_BIT);
}

static void pmp_set_cfg_a_mode(pmp_td *pmp, unsigned int cfg_reg_index, pmp_addr_matching mode)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    cfg_ptr[cfg_reg_index] |= (mode << PMP_CFG_A_BIT_OFFS);
}

static void pmp_set_cfg_x_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    SET_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_X_BIT);
}

static void pmp_set_cfg_w_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    SET_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_W_BIT);
}

static void pmp_set_cfg_r_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    SET_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_R_BIT);
}

static void pmp_set_napot_addr(pmp_td *pmp, unsigned int cfg_reg_index, rv_uint_xlen base, rv_uint_xlen size)
{
    rv_uint_xlen napot_size = ((size/2)-1);
    //napot_size = 0x7FFF
    pmp->addr[cfg_reg_index] = (base + napot_size) >> 2;
    //pmp_addr = 0x1000_01FF

    // printf("pmp addr: " PRINTF_FMT "\n", pmp->addr[cfg_reg_index]);
}

static void pmp_set_na4_tor_addr(pmp_td *pmp, unsigned int cfg_reg_index, rv_uint_xlen base)
{
    pmp->addr[cfg_reg_index] = base >> 2;
}


static pmp_td real_pmp = {0};
static pmp_td tmp_pmp = {0};

void setUp(void)
{
    memset(&real_pmp, 0, sizeof(real_pmp));
    memset(&tmp_pmp, 0, sizeof(tmp_pmp));
}

void tearDown(void)
{
}

void test_PMP_lock_bit(void)
{
    /*  testcase 1: 

        - set some bits in tmp pmpcfg11
        - also set the locked bit in real pmpcfg11
        - try to assign the tmp pmpcfg11 to the real pmpcfg11

        expexted output:
        - the real pmpcfg11 should not change at all as the lock bit does not allow that.
    */
    /* lets setup some PMP flags in the tmp_pmp regs */
    pmp_set_cfg_a_mode(&tmp_pmp, 11, pmp_a_na4);
    pmp_set_cfg_x_flag(&tmp_pmp, 11);

    uint8_t *tmp_cfg_ptr = (uint8_t *)&tmp_pmp.cfg[0];
    TEST_ASSERT_EQUAL_HEX8(0x14, tmp_cfg_ptr[11]);

    /* now we set the locked bit in the real_pmp and check if pmp_write_csr works as expected afterwards */
    pmp_set_cfg_l_flag(&real_pmp, 11);
    // printf("Tmp PMP:\n");
    // pmp_dump_cfg_regs(&tmp_pmp);

    pmp_write_csr(&real_pmp, machine_mode, 1, tmp_pmp.cfg[1]);

    uint8_t *real_cfg_ptr = (uint8_t *)&real_pmp.cfg[0];

    /* Ensure, that the register was not overwritten */
    TEST_ASSERT_EQUAL_HEX8(0x80, real_cfg_ptr[11]);

    // printf("Real PMP:\n");
    // pmp_dump_cfg_regs(&real_pmp);
}

void test_PMP_napot_memcheck(void)
{
    /*  testcase 2: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_napot);
    pmp_set_napot_addr(&tmp_pmp, 0, 0x40000000, 0x1000);

    int mem_check_result = 0;

    /* now check mem */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000fff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40001000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);
}

void test_PMP_na4_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_na4);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);

    int mem_check_result = 0;

    /* now check mem */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000003);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000004);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);
}

void test_PMP_tor_first_entry_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);

    int mem_check_result = 0;

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x0);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    /* first invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);
}

void test_PMP_tor_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 1, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 1, 0x80000000);

    int mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);
}

void test_PMP_tor_higher_cfg_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 5, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 6, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 5, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 6, 0x80000000);

    int mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_OK, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80000000);
    TEST_ASSERT_EQUAL(RV_MEM_ACCESS_ERR, mem_check_result);
}

int main() 
{
    UnityBegin("pmp/unit_tests.c");
    RUN_TEST(test_PMP_lock_bit, __LINE__);
    RUN_TEST(test_PMP_napot_memcheck, __LINE__);
    RUN_TEST(test_PMP_na4_memcheck, __LINE__);
    RUN_TEST(test_PMP_tor_first_entry_memcheck, __LINE__);
    RUN_TEST(test_PMP_tor_memcheck, __LINE__);
    RUN_TEST(test_PMP_tor_higher_cfg_memcheck, __LINE__);

    return (UnityEnd());
}