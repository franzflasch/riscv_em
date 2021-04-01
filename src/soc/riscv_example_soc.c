#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_helper.h>
#include <riscv_example_soc.h>

#include <file_helper.h>

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
    return RV_ACCESS_OK;
}

static int memory_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    uint8_t *mem_ptr = priv;
    memcpy(&mem_ptr[address_internal], &val, nr_bytes);
    return RV_ACCESS_OK;
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

static rv_uint_xlen rv_soc_read_mem(void *priv, rv_uint_xlen address, uint8_t len, int *err)
{
    rv_soc_td *rv_soc = priv;
    rv_uint_xlen tmp_addr = 0;
    rv_uint_xlen read_val = 0;
    size_t i = 0;

    *err = RV_ACCESS_ERR;

    for(i=0;i<(sizeof(rv_soc->mem_access_cbs)/sizeof(rv_soc->mem_access_cbs[0]));i++)
    {
        if(ADDR_WITHIN_LEN(address, len, rv_soc->mem_access_cbs[i].addr_start, rv_soc->mem_access_cbs[i].mem_size))
        {
            tmp_addr = address - rv_soc->mem_access_cbs[i].addr_start;
            rv_soc->mem_access_cbs[i].read(rv_soc->mem_access_cbs[i].priv, tmp_addr, &read_val);
            *err = RV_ACCESS_OK;
            return read_val;
        }
    }

    die_msg("Invalid Address, or no valid read pointer found, read not executed!: Addr: "PRINTF_FMT" Len: %d Cycle: %ld  PC: "PRINTF_FMT"\n", address, len, rv_soc->rv_core0.curr_cycle, rv_soc->rv_core0.pc);
    return 0;
}

static void rv_soc_write_mem(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t len)
{
    rv_soc_td *rv_soc = priv;
    rv_uint_xlen tmp_addr = 0;
    size_t i = 0;

    for(i=0;i<(sizeof(rv_soc->mem_access_cbs)/sizeof(rv_soc->mem_access_cbs[0]));i++)
    {
        if(ADDR_WITHIN_LEN(address, len, rv_soc->mem_access_cbs[i].addr_start, rv_soc->mem_access_cbs[i].mem_size))
        {
            tmp_addr = address - rv_soc->mem_access_cbs[i].addr_start;
            rv_soc->mem_access_cbs[i].write(rv_soc->mem_access_cbs[i].priv, tmp_addr, value, len);
            return;
        }
    }

    die_msg("Invalid Address, or no valid write pointer found, write not executed!: Addr: "PRINTF_FMT" Len: %d Cycle: %ld  PC: "PRINTF_FMT"\n", address, len, rv_soc->rv_core0.curr_cycle, rv_soc->rv_core0.pc);
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

void rv_soc_init(rv_soc_td *rv_soc, char *fw_file_name, char *dtb_file_name)
{
    #define RESET_VEC_SIZE 10
    #define MiB 0x100000

    uint64_t i;
    uint64_t start_addr = RAM_BASE_ADDR;
    uint64_t ram_addr_end = (RAM_BASE_ADDR + RAM_SIZE_BYTES);
    uint64_t fdt_addr = 0;
    uint64_t fdt_size = 0;
    uint64_t tmp = 0;

    static uint8_t __attribute__((aligned (4))) soc_mrom[MROM_SIZE_BYTES] = { 0 };
    static uint8_t __attribute__((aligned (4))) soc_ram[RAM_SIZE_BYTES] = { 0 };

    /* Init everything to zero */
    memset(rv_soc, 0, sizeof(rv_soc_td));
    rv_soc->mrom = soc_mrom;
    rv_soc->ram = soc_ram;

    /* Copy dtb and firmware */
    if(dtb_file_name != NULL)
    {
        fdt_size = get_file_size(dtb_file_name);

        /*
         * This is a little annoying: qemu keeps changing this stuff 
         * from time to time and I need to do it the same, otherwise the 
         * tests would fail as they won't match with qemu's results anymore
         * Be Aware: 2 * MiB was taken from qemu 5.2 but it seems
         * they already changed this again in later versions to 16 * MiB
         */
        fdt_addr = ADDR_ALIGN_DOWN(ram_addr_end - fdt_size, 2 * MiB);
        tmp = fdt_addr - RAM_BASE_ADDR;
        write_mem_from_file(dtb_file_name, &soc_ram[tmp], RAM_SIZE_BYTES-tmp);
    }

    write_mem_from_file(fw_file_name, soc_ram, sizeof(soc_ram));

    /* this is the reset vector, taken from qemu v5.2 */
    uint32_t reset_vec[RESET_VEC_SIZE] = {
        0x00000297,                  /* 1:  auipc  t0, %pcrel_hi(fw_dyn) */
        0x02828613,                  /*     addi   a2, t0, %pcrel_lo(1b) */
        0xf1402573,                  /*     csrr   a0, mhartid  */
        #ifdef RV64
            0x0202b583,              /*     ld     a1, 32(t0) */
            0x0182b283,              /*     ld     t0, 24(t0) */
        #else
            0x0202a583,              /*     lw     a1, 32(t0) */
            0x0182a283,              /*     lw     t0, 24(t0) */
        #endif
        0x00028067,                  /*     jr     t0 */
        start_addr,                  /* start: .dword */
        0x00000000,
        fdt_addr,                    /* fdt_laddr: .dword */
        0x00000000,
                                     /* fw_dyn: */
    };

    uint32_t *tmp_ptr = (uint32_t *)soc_mrom;
    for(i=0;i<(sizeof(reset_vec)/sizeof(reset_vec[0]));i++)
    {
        tmp_ptr[i] = reset_vec[i];
    }

    /* initialize one core with a csr table */
    rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem);

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

        // if(msi)
        //     printf("mei %d, msi %d, mti %d\n", mei, msi, mti);

        /* update CSRs for actual interrupt processing */
        rv_core_process_interrupts(&rv_soc->rv_core0, mei, mti, msi);

        rv_core_reg_dump(&rv_soc->rv_core0);

        if(rv_soc->rv_core0.pc == success_pc)
            break;

        if((num_cycles != 0) && (rv_soc->rv_core0.curr_cycle >= num_cycles))
            break;
    }
}
