#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_config.h>
#include <riscv_helper.h>
#include <riscv_example_soc.h>

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

static int memory_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval)
{
    uint8_t *mem_ptr = priv;
    rv_uint_xlen *xlen_ptr = (rv_uint_xlen *)&mem_ptr[address_internal];
    *outval = *xlen_ptr;
    return RV_MEM_ACCESS_OK;
}

static int memory_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    uint8_t *mem_ptr = priv;
    memcpy(&mem_ptr[address_internal], &val, nr_bytes);
    return RV_MEM_ACCESS_OK;
}

static void rv_soc_init_mem_acces_cbs(rv_soc_td *rv_soc)
{
    int count = 0;
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, memory_read, memory_write, rv_soc->ram, RAM_BASE_ADDR, RAM_SIZE_BYTES);
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, clint_read_reg, clint_write_reg, &rv_soc->clint, CLINT_BASE_ADDR, CLINT_SIZE_BYTES);
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, plic_read_reg, plic_write_reg, &rv_soc->plic, PLIC_BASE_ADDR, PLIC_SIZE_BYTES);
    #ifdef USE_SIMPLE_UART
        INIT_MEM_ACCESS_STRUCT(rv_soc, count++, simple_uart_read, simple_uart_write, &rv_soc->uart, SIMPLE_UART_TX_REG_ADDR, SIMPLE_UART_SIZE_BYTES);
    #else
        INIT_MEM_ACCESS_STRUCT(rv_soc, count++, uart_read, uart_write, &rv_soc->uart8250, UART8250_TX_REG_ADDR, UART_NS8250_NR_REGS);
    #endif
    INIT_MEM_ACCESS_STRUCT(rv_soc, count++, memory_read, memory_write, rv_soc->mrom, MROM_BASE_ADDR, MROM_SIZE_BYTES);
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

    die_msg("Invalid Address, or no valid read pointer found, read not executed!: Addr: "PRINTF_FMT"  Cycle: %ld  PC: "PRINTF_FMT"\n", address, rv_soc->rv_core0.curr_cycle, rv_soc->rv_core0.pc);
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

    die_msg("Invalid Address, or no valid write pointer found, write not executed!: Addr: "PRINTF_FMT"  Cycle: %ld  PC: "PRINTF_FMT"\n", address, rv_soc->rv_core0.curr_cycle, rv_soc->rv_core0.pc);
    return;
}

static void write_mem_from_file(char *file_name, uint8_t *memory, rv_uint_xlen mem_size)
{
    FILE * p_fw_file = NULL;
    unsigned long lsize = 0;
    size_t result = 0;

    p_fw_file = fopen(file_name, "rb");
    if(p_fw_file == NULL)
    {
        printf("Could not open fw file!\n");
        exit(-1);
    }

    fseek(p_fw_file, 0, SEEK_END);
    lsize = ftell(p_fw_file);
    rewind(p_fw_file);

    if(lsize > mem_size)
    {
        printf("Not able to load fw file of size %lu, mem space is " PRINTF_FMTU "\n", lsize, mem_size);
        exit(-2);
    }

    result = fread(memory, sizeof(uint8_t), lsize, p_fw_file);
    if(result != lsize)
    {
        printf("Error while reading file!\n");
        exit(-3);
    }

    fclose(p_fw_file);
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

void rv_soc_init(rv_soc_td *rv_soc, char *fw_file_name, char *dtb_file_name)
{
    uint64_t i;
    uint64_t start_addr = RAM_BASE_ADDR;

    static uint8_t __attribute__((aligned (4))) soc_mrom[MROM_SIZE_BYTES] = { 0 };
    static uint8_t __attribute__((aligned (4))) soc_ram[RAM_SIZE_BYTES] = { 0 };

    /* this is the reset vector, taken from qemu v4.2 */
    uint32_t reset_vec[8] = {
        0x00000297,                  /* 1:  auipc  t0, %pcrel_hi(dtb) */
        0x02028593,                  /*     addi   a1, t0, %pcrel_lo(1b) */
        0xf1402573,                  /*     csrr   a0, mhartid  */

        #ifdef RV64
                0x0182b283,          /*     ld     t0, 24(t0) */
        #else
                0x0182a283,          /*     lw     t0, 24(t0) */
        #endif

        0x00028067,                  /*     jr     t0 */
        0x00000000,
        start_addr,                  /* start: .dword */
        0x00000000,
                                     /* dtb: */
    };

    uint32_t *tmp_ptr = (uint32_t *)soc_mrom;
    for(i=0;i<(sizeof(reset_vec)/sizeof(reset_vec[0]));i++)
    {
        tmp_ptr[i] = reset_vec[i];
    }

    memset(rv_soc, 0, sizeof(rv_soc_td));
    rv_soc->mrom = soc_mrom;
    rv_soc->ram = soc_ram;

    if(dtb_file_name != NULL)
        write_mem_from_file(dtb_file_name, &soc_mrom[8*sizeof(uint32_t)], sizeof(soc_mrom)-(8*sizeof(uint32_t)));

    write_mem_from_file(fw_file_name, soc_ram, sizeof(soc_ram));

    /* initialize one core with a csr table */
    #ifdef CSR_SUPPORT
        RV_CORE_INSTANTIATE_CSR_REGS_FOR_CORE(csr_regs_core0);
        rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem, &csr_regs_core0_desc);
    #else
        rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem, NULL);
    #endif

    #ifdef USE_SIMPLE_UART
        simple_uart_init(&rv_soc->uart);
    #else
        uart_init(&rv_soc->uart8250);
    #endif

    /* initialize ram and peripheral read write access pointers */
    rv_soc_init_mem_acces_cbs(rv_soc);

    DEBUG_PRINT("rv SOC initialized!\n");
}

void rv_soc_run(rv_soc_td *rv_soc, rv_uint_xlen success_pc, uint64_t num_cycles)
{
    uint8_t mei = 0, msi = 0, mti = 0;
    uint8_t uart_irq_pending = 0;

    rv_core_reg_dump(&rv_soc->rv_core0);

    while(1)
    {
        rv_core_run(&rv_soc->rv_core0);

        /* update peripherals */
        #ifdef USE_SIMPLE_UART
            uart_irq_pending = simple_uart_update(&rv_soc->uart);
        #else
            uart_irq_pending = uart_update(&rv_soc->uart8250);
        #endif

        // printf("Uart pending: %d\n", uart_irq_pending);

        /* update interrupt controllers */
        plic_update_pending(&rv_soc->plic, 10, uart_irq_pending);
        mei = plic_update(&rv_soc->plic);
        clint_update(&rv_soc->clint, &msi, &mti);

        /* update CSRs for actual interrupt processing */
        rv_core_process_interrupts(&rv_soc->rv_core0, mei, msi, mti);

        rv_core_reg_dump(&rv_soc->rv_core0);
        // rv_core_reg_internal_after_exec(&rv_soc.rv_core);

        if((rv_soc->rv_core0.pc == (success_pc)))
            break;

        if((num_cycles != 0) && (rv_soc->rv_core0.curr_cycle >= num_cycles))
            break;
    }
}
