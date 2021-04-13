#include <stdio.h>
#include <string.h>

#include <mmu.h>
#include <riscv_helper.h>

#include <unity.h>

#define TEST_RAM_SIZE_BYTES 0x800000
#define TEST_RAM_ADDR_OFFS 0x2000

mmu_td mmu_test = {0};
uint8_t test_ram[TEST_RAM_SIZE_BYTES] = {0};

static rv_ret mmu_phys_bus_access(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen addr, void *value, uint8_t len)
{
    (void) priv;
    (void) priv_level;
    rv_uint_xlen tmp_addr = 0;

    if( (addr >= TEST_RAM_ADDR_OFFS) && (addr < (TEST_RAM_ADDR_OFFS+TEST_RAM_SIZE_BYTES)) )
    {
        tmp_addr = addr-TEST_RAM_ADDR_OFFS;
        if(access_type == bus_write_access)
            memcpy(&test_ram[tmp_addr], value, len);
        else 
            memcpy(value, &test_ram[tmp_addr], len);
        return rv_ok;
    }
    return rv_err;
}

void setUp(void)
{
    memset(&mmu_test, 0, sizeof(mmu_td));
    mmu_init(&mmu_test, mmu_phys_bus_access, NULL);
    mmu_write_csr(&mmu_test, supervisor_mode, 0, (MMU_SATP_MODE_SV32<<MMU_SATP_MODE_BIT) | (TEST_RAM_ADDR_OFFS >> 12) );
}

void tearDown(void)
{
}

/* this is just for testing purposes to be able to setup page tables, in reality this will of course be
 * done in SW which is emulated
 */
void mmu_map_test(mmu_td *mmu, rv_uint_xlen virt_addr, uint64_t phys_addr, uint8_t level, uint8_t pte_flags)
{
    int i,j = 0;
    rv_uint_xlen pte_addr = 0;
    rv_uint_xlen pte = 0;

    /* get vpn */
    rv_uint_xlen vpn[SV32_LEVELS] = 
    {
        /* 10 Bit */
        (virt_addr >> 12) & 0x3ff,
        /* 10 Bit */
        (virt_addr >> 22) & 0x3ff
    };
    // printf("vpn[1] %x vpn[0] %x\n", vpn[1], vpn[0]);

    /* get ppn */
    rv_uint_xlen ppn[SV32_LEVELS] = 
    {
        /* 10 Bit */
        (phys_addr >> 12) & 0x3ff,
        /* 12 Bit */
        (phys_addr >> 22) & 0xfff
    };
    // printf("phys addr: %x ppn[1] %x ppn[0] %x\n",phys_addr, ppn[1], ppn[0]);

    /* get address of root page table */
    rv_uint_xlen root_pg_table_addr = mmu->satp_reg << 12;
    // printf("root pg addr: %x\n", root_pg_table_addr);

    for(i=(SV32_LEVELS-1),j=0;i>=0;i--,j++)
    {
        /*
         * 1 level: 4MB superpage
         * 2 level: 4KB page 
         */
        if(j==level)
            break;

        /* iterate through the Pagetable */
        pte_addr = root_pg_table_addr + (SV32_PAGE_SIZE*j) + (vpn[i] << SV32_PTESHIFT);

        /* Only set valid here in the loop */
        pte = MMU_PAGE_VALID;
        mmu_phys_bus_access(NULL, machine_mode, bus_write_access, pte_addr, &pte, sizeof(rv_uint_xlen));
        // printf("--- vpn[%d]: %x pte addr: %x pte[%d] %x\n",i, vpn[i], pte_addr, i, pte);
    }

    /* Set physical address here */
    pte = (ppn[1] << 20) | (ppn[0] << 10) | pte_flags | MMU_PAGE_VALID;
    
    // printf("pte: %x\n", pte);
    mmu_phys_bus_access(NULL, machine_mode, bus_write_access, pte_addr, &pte, sizeof(rv_uint_xlen));
    // mmu->write_mem(NULL, pte_addr, pte, sizeof(rv_uint_xlen));
}

void print_page_table(mmu_td *mmu)
{
    rv_uint_xlen root_pg_table_addr = mmu->satp_reg << 12;
    rv_uint_xlen pte = 0;
    int i = 0;

    printf("\nPAGE TABLE:\n");

    for(i=0;i<(SV32_PAGE_SIZE);i+=SV32_PTESIZE)
    {
        mmu_phys_bus_access(NULL, machine_mode, bus_read_access, root_pg_table_addr+i, &pte, sizeof(rv_uint_xlen));
        // pte = mmu->read_mem(NULL, root_pg_table_addr+i, sizeof(rv_uint_xlen), &err);
        if(pte != 0)
            printf("PTE1[%d]: %x\n", i, pte);
    }

    for(i=0;i<(SV32_PAGE_SIZE);i+=SV32_PTESIZE)
    {
        mmu_phys_bus_access(NULL, machine_mode, bus_read_access, root_pg_table_addr+(SV32_PAGE_TABLE_ENTRIES*SV32_PTESIZE)+i, &pte, sizeof(rv_uint_xlen));
        // pte = mmu->read_mem(NULL, root_pg_table_addr+(SV32_PAGE_TABLE_ENTRIES*SV32_PTESIZE)+i, sizeof(rv_uint_xlen), &err);
        if(pte != 0)
            printf("PTE0[%d]: %x\n", i, pte);
    } 
    printf("\n");
}

void test_MMU_simple(void)
{
    mmu_ret mmu_retval = mmu_ok;
    uint64_t translated_phys_addr = 0;

    // mmu_dump(&mmu_test);

    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_WRITE | MMU_PAGE_READ | MMU_PAGE_EXEC | MMU_PAGE_ACCESSED);
    mmu_map_test(&mmu_test, 0x8000000, 0x400000, 1, MMU_PAGE_WRITE | MMU_PAGE_READ | MMU_PAGE_EXEC | MMU_PAGE_ACCESSED);

    print_page_table(&mmu_test);

    /* 4K page */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x4080, translated_phys_addr);

    /* Superpage */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x8106090, bus_read_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x506090, translated_phys_addr);
}

void test_MMU_access_flags(void)
{
    mmu_ret mmu_retval = mmu_ok;
    uint64_t translated_phys_addr = 0;

    // mmu_dump(&mmu_test);

    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_WRITE | MMU_PAGE_READ | MMU_PAGE_EXEC | MMU_PAGE_ACCESSED);
    mmu_map_test(&mmu_test, 0x8000000, 0x400000, 1, MMU_PAGE_WRITE | MMU_PAGE_ACCESSED);
    mmu_map_test(&mmu_test, 0x16000, 0x1000, SV32_LEVELS, MMU_PAGE_EXEC | MMU_PAGE_ACCESSED);
    mmu_map_test(&mmu_test, 0x17000, 0x2000, SV32_LEVELS, MMU_PAGE_WRITE | MMU_PAGE_DIRTY | MMU_PAGE_ACCESSED);

    print_page_table(&mmu_test);

    /* 4K page */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x4080, translated_phys_addr);

    /* Superpage */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x8106090, bus_read_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x0, translated_phys_addr);

    /* Only instruction access should be allowed */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x16080, bus_instr_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x1080, translated_phys_addr);

    /* Write access is not allowed */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x16080, bus_write_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x0, translated_phys_addr);

    /* Read access is also not allowed */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x16080, bus_read_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x0, translated_phys_addr);

    /* Write only flag (with ACCESSED and DIRTY) seems to be not allowed so without read flag also write is not permitted  */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x17080, bus_write_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* Also set read flag, still not permitted without 'DIRTY' bit set */
    mmu_map_test(&mmu_test, 0x17000, 0x2000, SV32_LEVELS, MMU_PAGE_WRITE | MMU_PAGE_READ);
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x17080, bus_write_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* Also set dirty bit, this should still not be possible without accessed bit set */
    mmu_map_test(&mmu_test, 0x17000, 0x2000, SV32_LEVELS, MMU_PAGE_WRITE | MMU_PAGE_READ | MMU_PAGE_DIRTY);
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x17080, bus_write_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* Also set accessed bit, and finally write access should be possible */
    mmu_map_test(&mmu_test, 0x17000, 0x2000, SV32_LEVELS, MMU_PAGE_WRITE | MMU_PAGE_READ | MMU_PAGE_DIRTY | MMU_PAGE_ACCESSED);
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x17080, bus_write_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x2080, translated_phys_addr);
}

void test_MMU_machine_mode(void)
{
    mmu_ret mmu_retval = mmu_ok;
    uint64_t translated_phys_addr = 0;

    // mmu_dump(&mmu_test);

    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_EXEC | MMU_PAGE_ACCESSED);

    print_page_table(&mmu_test);

    /* in machinemode the translation should be virt_addr == phys_addr */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, machine_mode, 0x12080, bus_read_access, 0, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x12080, translated_phys_addr);
}

void test_MMU_mxr(void)
{
    mmu_ret mmu_retval = mmu_ok;
    uint64_t translated_phys_addr = 0;
    uint8_t mxr = 0;

    // mmu_dump(&mmu_test);

    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_EXEC | MMU_PAGE_ACCESSED);

    print_page_table(&mmu_test);

    /* read access for instruction pages is not allowed, if mxr = 0 */
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, mxr, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* But it should be allowed if mxr = 1 */
    mxr = 1;
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, mxr, 0, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x4080, translated_phys_addr);
}

void test_MMU_sum(void)
{
    mmu_ret mmu_retval = mmu_ok;
    uint64_t translated_phys_addr = 0;
    uint8_t sum = 0;

    // mmu_dump(&mmu_test);

    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_EXEC | MMU_PAGE_READ | MMU_PAGE_WRITE | MMU_PAGE_ACCESSED);

    print_page_table(&mmu_test);

    /* user is not allowed to access non user pages */
    sum = 0;
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, user_mode, 0x12080, bus_read_access, 0, sum, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* Now set SUM to 1, this has no effect in user_mode */
    sum = 1;
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, user_mode, 0x12080, bus_read_access, 0, sum, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* everything should be fine in supervisor_mode */
    sum = 1;
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, 0, sum, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x4080, translated_phys_addr);

    /* Now map the page as user page, access should fail if SUM = 0 */
    sum = 0;
    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_EXEC | MMU_PAGE_READ | MMU_PAGE_WRITE | MMU_PAGE_ACCESSED | MMU_PAGE_USER);
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, 0, sum, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_page_fault, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x00, translated_phys_addr);

    /* Access should be granted if SUM = 1 */
    sum = 1;
    mmu_map_test(&mmu_test, 0x12000, 0x4000, SV32_LEVELS, MMU_PAGE_EXEC | MMU_PAGE_READ | MMU_PAGE_WRITE | MMU_PAGE_ACCESSED | MMU_PAGE_USER);
    translated_phys_addr = mmu_virt_to_phys(&mmu_test, supervisor_mode, 0x12080, bus_read_access, 0, sum, &mmu_retval);
    TEST_ASSERT_EQUAL(mmu_ok, mmu_retval);
    TEST_ASSERT_EQUAL_HEX64(0x4080, translated_phys_addr);
}

int main() 
{
    UnityBegin("mmu/unit_tests.c");
    RUN_TEST(test_MMU_simple, __LINE__);
    RUN_TEST(test_MMU_access_flags, __LINE__);
    RUN_TEST(test_MMU_machine_mode, __LINE__);
    RUN_TEST(test_MMU_mxr, __LINE__);
    RUN_TEST(test_MMU_sum, __LINE__);

    return (UnityEnd());
}
