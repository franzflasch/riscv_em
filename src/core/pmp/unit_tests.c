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

static void pmp_clear_cfg_x_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    CLEAR_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_X_BIT);
}

static void pmp_clear_cfg_w_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    CLEAR_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_W_BIT);
}

static void pmp_clear_cfg_r_flag(pmp_td *pmp, unsigned int cfg_reg_index)
{
    uint8_t *cfg_ptr = (uint8_t *)&pmp->cfg[0];
    CLEAR_BIT(cfg_ptr[cfg_reg_index], PMP_CFG_R_BIT);
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
    // printf("%lx\n", real_pmp.cfg[1]);
    // printf("Tmp PMP:\n");
    // pmp_dump_cfg_regs(&tmp_pmp);

    pmp_write_csr_cfg(&real_pmp, machine_mode, 1, tmp_pmp.cfg[1]);
    // printf("%lx\n", real_pmp.cfg[1]);

    uint8_t *real_cfg_ptr = (uint8_t *)&real_pmp.cfg[0];

    /* Ensure, that the register was not overwritten */
    TEST_ASSERT_EQUAL_HEX8(0x80, real_cfg_ptr[11]);

    // printf("Real PMP:\n");
    pmp_dump_cfg_regs(&real_pmp);

    /* Now try to set an address on pmpaddr11, this should also not be possible */
    pmp_write_csr_addr(&real_pmp, machine_mode, 11, 0x1234);
    TEST_ASSERT_EQUAL_HEX(0x00, real_pmp.addr[11]);

    /* Also try to set something on a addr which is not locked */
    pmp_write_csr_addr(&real_pmp, machine_mode, 10, 0x1234);
    TEST_ASSERT_EQUAL_HEX(0x1234, real_pmp.addr[10]);
}

void test_PMP_napot_memcheck(void)
{
    /*  testcase 2: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_napot);
    pmp_set_napot_addr(&tmp_pmp, 0, 0x40000000, 0x1000);
    pmp_set_cfg_r_flag(&tmp_pmp, 0);

    rv_ret mem_check_result = 0;

    /* now check mem */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000fff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40001000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_na4_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_na4);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);
    pmp_set_cfg_r_flag(&tmp_pmp, 0);

    rv_ret mem_check_result = 0;

    /* now check mem */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000003, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000004, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_tor_first_entry_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x80087000);
    pmp_set_cfg_r_flag(&tmp_pmp, 0);

    rv_ret mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x0, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80086FFF, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* test in the middle */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80080000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* Test first invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80087000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    // printf("%lx\n", tmp_pmp.cfg[0]);
}

void test_PMP_tor_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 1, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 1, 0x80000000);
    pmp_set_cfg_r_flag(&tmp_pmp, 1);

    rv_ret mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_tor_higher_cfg_memcheck(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 5, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 6, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 5, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 6, 0x80000000);
    pmp_set_cfg_r_flag(&tmp_pmp, 6);

    rv_ret mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_tor_memcheck_r_flag(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 1, pmp_a_tor);
    pmp_set_cfg_r_flag(&tmp_pmp, 1);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 1, 0x80000000);

    rv_ret mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* first valid but try write access */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* first valid but try instruction access */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_tor_memcheck_rwx_flags(void)
{
    /*  testcase 3: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 1, pmp_a_tor);
    pmp_set_cfg_w_flag(&tmp_pmp, 1);
    pmp_set_na4_tor_addr(&tmp_pmp, 0, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 1, 0x80000000);

    rv_ret mem_check_result = 0;

    /* first valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* first valid but try read access */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* first valid but try instruction access */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 1, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* test invalid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x80000000, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* now set various rwx combinations */
    pmp_set_cfg_r_flag(&tmp_pmp, 1);
    pmp_clear_cfg_w_flag(&tmp_pmp, 1);
    pmp_set_cfg_x_flag(&tmp_pmp, 1);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    pmp_clear_cfg_r_flag(&tmp_pmp, 1);
    pmp_set_cfg_w_flag(&tmp_pmp, 1);
    pmp_set_cfg_x_flag(&tmp_pmp, 1);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    pmp_set_cfg_r_flag(&tmp_pmp, 1);
    pmp_set_cfg_w_flag(&tmp_pmp, 1);
    pmp_clear_cfg_x_flag(&tmp_pmp, 1);

    /* last valid */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_write_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x7fffffff, 1, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_napot_outofbounds_memcheck(void)
{
    /*  testcase 2: 
    
    */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_napot);
    pmp_set_napot_addr(&tmp_pmp, 0, 0x40000000, 0x1000);
    pmp_set_cfg_r_flag(&tmp_pmp, 0);
    pmp_set_cfg_w_flag(&tmp_pmp, 0);
    pmp_set_cfg_x_flag(&tmp_pmp, 0);

    rv_ret mem_check_result = 0;

    /* now check mem */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000000, 4, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* This here should fail, as we want to read 2 bytes but the valid area ends at 0x40000fff */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000fff, 2, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* this shouls work again */
    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000ffe, 2, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40000ffc, 4, bus_read_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x3fffffff, 4, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, supervisor_mode, 0x40001000, 1, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);
}

void test_PMP_napot_m_mode_locked_memcheck(void)
{
    /*
     * If the locked bit is set, mem accesses in a configured PMP
     * region are not permitted even in machine mode.
     */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_napot);
    pmp_set_napot_addr(&tmp_pmp, 0, 0x40000000, 0x1000);

    /* memchecks in machine mode are only performed for regions where the lock bit is set */
    pmp_set_cfg_l_flag(&tmp_pmp, 0);

    /* Only allow write and execute */
    pmp_set_cfg_w_flag(&tmp_pmp, 0);
    pmp_set_cfg_x_flag(&tmp_pmp, 0);

    rv_ret mem_check_result = 0;

    /* reads should now fail */
    mem_check_result = pmp_mem_check(&tmp_pmp, machine_mode, 0x40000000, 4, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    // FIXME: this edge case is currently not supported (includes overlapping mem-region access, which actually should not succeed, but currently does):
    mem_check_result = pmp_mem_check(&tmp_pmp, machine_mode, 0x40000fff, 2, bus_read_access);
    TEST_ASSERT_EQUAL(rv_err, mem_check_result);

    /* writes should succeed */
    mem_check_result = pmp_mem_check(&tmp_pmp, machine_mode, 0x40000000, 4, bus_write_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, machine_mode, 0x40000fff, 2, bus_write_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    /* executes should also succeed */
    mem_check_result = pmp_mem_check(&tmp_pmp, machine_mode, 0x40000000, 4, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);

    mem_check_result = pmp_mem_check(&tmp_pmp, machine_mode, 0x40000fff, 2, bus_instr_access);
    TEST_ASSERT_EQUAL(rv_ok, mem_check_result);
}

void test_PMP_tor_check_locked_csr_write(void)
{
    /*
     * From the spec: "Additionally, if pmpicfg.A is set to TOR, writes to pmpaddri-1 are ignored."
     */
    pmp_set_cfg_a_mode(&tmp_pmp, 0, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 1, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 1, 0x4000);
    pmp_set_cfg_l_flag(&tmp_pmp, 1);

    uint8_t *cfg_ptr = (uint8_t *)&tmp_pmp.cfg[0];
    TEST_ASSERT_EQUAL_HEX8(0x88, cfg_ptr[1]);

    /* Should still be 0x88 after this */
    pmp_write_csr_cfg(&tmp_pmp, machine_mode, 0, (0xFF << 8));
    TEST_ASSERT_EQUAL_HEX8(0x88, cfg_ptr[1]);

    /* Also writes to addr[1] should be not possible */
    pmp_write_csr_addr(&tmp_pmp, machine_mode, 1, 0x1234);
    TEST_ASSERT_EQUAL_HEX(0x1000, tmp_pmp.addr[1]);

    /* If set to tor writes to addr[0] should also be prohibited */
    pmp_write_csr_addr(&tmp_pmp, machine_mode, 0, 0x4321);
    TEST_ASSERT_EQUAL_HEX(0x00, tmp_pmp.addr[0]);
}

void test_PMP_tor_check_locked_csr_write_edge_case(void)
{
    /*
     * From the spec: "Additionally, if pmpicfg.A is set to TOR, writes to pmpaddri-1 are ignored."
     */
    pmp_set_cfg_a_mode(&tmp_pmp, 7, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 8, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 8, 0x4000);
    pmp_set_cfg_l_flag(&tmp_pmp, 8);

    uint8_t *cfg_ptr = (uint8_t *)&tmp_pmp.cfg[0];
    TEST_ASSERT_EQUAL_HEX8(0x88, cfg_ptr[8]);

    /* Should still be 0x88 after this */
    pmp_write_csr_cfg(&tmp_pmp, machine_mode, 1, 0xFF);
    TEST_ASSERT_EQUAL_HEX8(0x88, cfg_ptr[8]);

    /* Also writes to addr[1] should be not possible */
    pmp_write_csr_addr(&tmp_pmp, machine_mode, 8, 0x1234);
    TEST_ASSERT_EQUAL_HEX(0x1000, tmp_pmp.addr[8]);

    /* If set to tor writes to addr[0] should also be prohibited */
    pmp_write_csr_addr(&tmp_pmp, machine_mode, 7, 0x4321);
    TEST_ASSERT_EQUAL_HEX(0x00, tmp_pmp.addr[7]);
}


/* 
 *
 * As there are some RV32/RV64 specifics, we also need to adapt some tests here 
 * 
 * */
void test_PMP_read_write_csr_RV64(void)
{
    pmp_set_cfg_a_mode(&tmp_pmp, 5, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 6, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 5, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 6, 0x80000000);

    pmp_write_csr_cfg(&real_pmp, machine_mode, 0, tmp_pmp.cfg[0]);
    pmp_write_csr_addr(&real_pmp, machine_mode, 5, tmp_pmp.addr[5]);
    pmp_write_csr_addr(&real_pmp, machine_mode, 6, tmp_pmp.addr[6]);

    uint8_t *real_cfg_ptr = (uint8_t *)&real_pmp.cfg[0];

    // printf("%lx\n", real_pmp.cfg[0]);
    pmp_dump_cfg_regs(&real_pmp);

    /* Ensure, that the register was not overwritten */
    TEST_ASSERT_EQUAL_HEX8(0x08, real_cfg_ptr[6]);

    rv_uint_xlen read_val = 0;
    pmp_read_csr_cfg(&real_pmp, machine_mode, 0, &read_val);
    TEST_ASSERT_EQUAL_HEX64(0x0008000000000000, read_val);

    pmp_read_csr_addr(&real_pmp, machine_mode, 5, &read_val);
    TEST_ASSERT_EQUAL_HEX64(0x0000000010000000, read_val);

    pmp_read_csr_addr(&real_pmp, machine_mode, 6, &read_val);
    TEST_ASSERT_EQUAL_HEX64(0x0000000020000000, read_val);
}

void test_PMP_read_write_csr_RV32(void)
{
    pmp_set_cfg_a_mode(&tmp_pmp, 5, pmp_a_off);
    pmp_set_cfg_a_mode(&tmp_pmp, 6, pmp_a_tor);
    pmp_set_na4_tor_addr(&tmp_pmp, 5, 0x40000000);
    pmp_set_na4_tor_addr(&tmp_pmp, 6, 0x80000000);

    pmp_write_csr_cfg(&real_pmp, machine_mode, 1, tmp_pmp.cfg[1]);
    pmp_write_csr_addr(&real_pmp, machine_mode, 5, tmp_pmp.addr[5]);
    pmp_write_csr_addr(&real_pmp, machine_mode, 6, tmp_pmp.addr[6]);

    pmp_dump_cfg_regs(&tmp_pmp);

    uint8_t *real_cfg_ptr = (uint8_t *)&real_pmp.cfg[0];

    // printf("%lx\n", real_pmp.cfg[0]);
    // printf("%lx\n", real_cfg_ptr[6]);
    pmp_dump_cfg_regs(&real_pmp);

    /* Ensure, that the register was not overwritten */
    TEST_ASSERT_EQUAL_HEX8(0x08, real_cfg_ptr[6]);

    rv_uint_xlen read_val = 0;
    pmp_read_csr_cfg(&real_pmp, machine_mode, 1, &read_val);
    TEST_ASSERT_EQUAL_HEX64(0x0000000000080000, read_val);

    pmp_read_csr_addr(&real_pmp, machine_mode, 5, &read_val);
    TEST_ASSERT_EQUAL_HEX64(0x0000000010000000, read_val);

    pmp_read_csr_addr(&real_pmp, machine_mode, 6, &read_val);
    TEST_ASSERT_EQUAL_HEX64(0x0000000020000000, read_val);
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
    RUN_TEST(test_PMP_napot_m_mode_locked_memcheck, __LINE__);
    RUN_TEST(test_PMP_tor_check_locked_csr_write, __LINE__);
    RUN_TEST(test_PMP_tor_check_locked_csr_write_edge_case, __LINE__);

    RUN_TEST(test_PMP_tor_memcheck_r_flag, __LINE__);
    RUN_TEST(test_PMP_tor_memcheck_rwx_flags, __LINE__);
    RUN_TEST(test_PMP_napot_outofbounds_memcheck, __LINE__);

    #ifdef RV64
        RUN_TEST(test_PMP_read_write_csr_RV64, __LINE__);
    #else
        RUN_TEST(test_PMP_read_write_csr_RV32, __LINE__);
    #endif

    return (UnityEnd());
}