#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_config.h>
#include <riscv_helper.h>
#include <riscv_example_soc.h>

#include <uart.h>

#define INIT_MEM_ACCESS_STRUCT(_ref_rv_soc, _entry, _read_func, _write_func, _priv, _addr_start, _mem_size) \
{ \
    size_t _tmp_count = _entry; \
    if(_tmp_count >= (sizeof(_ref_rv_soc->mem_access_cbs)/sizeof(_ref_rv_soc->mem_access_cbs[0]))) \
        die_msg("No mem access pointer available for entry nr %d, please increase mem_access_cbs!\n", _entry); \
    _ref_rv_soc->mem_access_cbs[_tmp_count].read = _read_func; \
    _ref_rv_soc->mem_access_cbs[_tmp_count].write = _write_func; \
    _ref_rv_soc->mem_access_cbs[_tmp_count].priv = _priv; \
    _ref_rv_soc->mem_access_cbs[_tmp_count].addr_start = _addr_start; \
    _ref_rv_soc->mem_access_cbs[_tmp_count].mem_size = _mem_size; \
}

static int read_memory(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval)
{
    uint8_t *mem_ptr = priv;
    rv_uint_xlen *xlen_ptr = (rv_uint_xlen *)&mem_ptr[address_internal];
    *outval = *xlen_ptr;
    return RV_MEM_ACCESS_OK;
}

static int write_memory(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    uint8_t *mem_ptr = priv;
    memcpy(&mem_ptr[address_internal], &val, nr_bytes);
    return RV_MEM_ACCESS_OK;
}

static void rv_soc_init_mem_acces_cbs(rv_soc_td *rv_soc)
{
    int count = 0;
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, read_memory, write_memory, rv_soc->ram, RAM_BASE_ADDR, RAM_SIZE_BYTES);
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, read_clint_reg, write_clint_reg, &rv_soc->rv_core0.clint, CLINT_BASE_ADDR, CLINT_SIZE_BYTES);
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, uart_read, uart_write, NULL, UART_TX_REG_ADDR, 20);
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, read_memory, write_memory, rv_soc->mrom, MROM_BASE_ADDR, MROM_SIZE_BYTES);
}

static rv_uint_xlen rv_soc_read_mem(void *priv, rv_uint_xlen address, int *err)
{
    rv_soc_td *rv_soc = priv;
    rv_uint_xlen tmp_addr = 0;
    rv_uint_xlen read_val = 0;
    size_t i = 0;

    *err = RV_CORE_E_ERR;

    for(i=0;i<(sizeof(rv_soc->mem_access_cbs)/sizeof(rv_soc->mem_access_cbs[0]));i++)
    {
        if(rv_soc->mem_access_cbs[i].read != NULL)
        {
            if(ADDR_WITHIN(address, rv_soc->mem_access_cbs[i].addr_start, rv_soc->mem_access_cbs[i].mem_size))
            {
                tmp_addr = address - rv_soc->mem_access_cbs[i].addr_start;
                rv_soc->mem_access_cbs[i].read(rv_soc->mem_access_cbs[i].priv, tmp_addr, &read_val);
                *err = RV_CORE_E_OK;
                return read_val;
            }
        }
    }

    die_msg("Invalid Address, or no valid read pointer found, read not executed!: "PRINTF_FMT" %ld "PRINTF_FMT"\n", address, rv_soc->rv_core0.curr_cycle, rv_soc->rv_core0.pc);
    return 0;
}

static void rv_soc_write_mem(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes)
{
    rv_soc_td *rv_soc = priv;
    rv_uint_xlen tmp_addr = 0;
    size_t i = 0;

    for(i=0;i<(sizeof(rv_soc->mem_access_cbs)/sizeof(rv_soc->mem_access_cbs[0]));i++)
    {
        if(rv_soc->mem_access_cbs[i].write != NULL)
        {
            if(ADDR_WITHIN(address, rv_soc->mem_access_cbs[i].addr_start, rv_soc->mem_access_cbs[i].mem_size))
            {
                tmp_addr = address - rv_soc->mem_access_cbs[i].addr_start;
                rv_soc->mem_access_cbs[i].write(rv_soc->mem_access_cbs[i].priv, tmp_addr, value, nr_bytes);
                return;
            }
        }
    }

    die_msg("Invalid Address, or no valid write pointer found, write not executed!: "PRINTF_FMT" %ld "PRINTF_FMT"\n", address, rv_soc->rv_core0.curr_cycle, rv_soc->rv_core0.pc);
    return;
}

void rv_soc_dump_mem(rv_soc_td *rv_soc)
{
    uint32_t i = 0;
    printf("rv RAM contents\n");
    for(i=0;i<RAM_SIZE_BYTES/(sizeof(rv_uint_xlen));i++)
    {
        printf("%x\n", rv_soc->ram[i]);
    }
}

void rv_soc_init(rv_soc_td *rv_soc, char *fw_file_name)
{
    static uint8_t __attribute__((aligned (4))) soc_mrom[MROM_SIZE_BYTES] = { 0 };
    static uint8_t __attribute__((aligned (4))) soc_ram[RAM_SIZE_BYTES] = { 0 };

    memset(rv_soc, 0, sizeof(rv_soc_td));
    rv_soc->mrom = soc_mrom;
    rv_soc->ram = soc_ram;

    FILE * p_fw_file = NULL;
    unsigned long lsize = 0;
    size_t result = 0;

    p_fw_file = fopen(fw_file_name, "rb");
    if(p_fw_file == NULL)
    {
        printf("Could not open fw file!\n");
        exit(-1);
    }

    fseek(p_fw_file, 0, SEEK_END);
    lsize = ftell(p_fw_file);
    rewind(p_fw_file);

    if(lsize > sizeof(soc_ram))
    {
        printf("Not able to load fw file of size %lu, ram space is %lu\n", lsize, sizeof(soc_ram));
        exit(-2);
    }

    /* initialize one core with a csr table */
    #ifdef CSR_SUPPORT
        RV_CORE_INSTANTIATE_CSR_REGS_FOR_CORE(csr_regs_core0);
        rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem, &csr_regs_core0_desc);
    #else
        rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem, NULL);
    #endif
    

    /* set some registers initial value to match qemu's */
    rv_soc->rv_core0.x[11] = 0x00001020;

    /* initialize ram and peripheral read write access pointers */
    rv_soc_init_mem_acces_cbs(rv_soc);

    result = fread(rv_soc->ram, sizeof(char), lsize, p_fw_file);
    if(result != lsize)
    {
        printf("Error while reading file!\n");
        exit(-3);
    }

    fclose(p_fw_file);

    // rv_soc_dump_mem(rv_soc);
    // while(1);

    DEBUG_PRINT("rv SOC initialized!\n");
}
