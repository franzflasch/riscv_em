#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_config.h>
#include <riscv_helper.h>
#include <riscv_soc.h>

rv_uint_xlen rv_soc_read_mem(void *priv, rv_uint_xlen address)
{
    uint8_t align_offset = address & 0x3;
    rv_uint_xlen return_val = 0;
    rv_soc_td *rv_soc = priv;

    rv_uint_xlen read_val = 0;

    if((address >= RAM_BASE_ADDR) && (address < (RAM_BASE_ADDR+RAM_SIZE_BYTES)))
    {
        read_val = rv_soc->ram[(address-RAM_BASE_ADDR) >> 2];
        #ifdef RV64
            read_val |= ((uint64_t)rv_soc->ram[((address-RAM_BASE_ADDR) >> 2) + 1] << 32);
        #endif
    }
    else if((address >= CLINT_BASE_ADDR) && (address <= CLINT_BASE_ADDR_END))
    {
        if(read_clint_reg(&rv_soc->rv_core0.clint, address, &read_val))
        {
            die_msg("Error reading clint reg "PRINTF_FMT"\n", address);
        }
    }
    else
    {
        DEBUG_PRINT("Invalid Adress, read not executed!: "PRINTF_FMT"\n", address);
        return 0;
    }

    switch(align_offset)
    {
        case 1:
            return_val = read_val >> 8;
            break;
        case 2:
            return_val = read_val >> 16;
            break;
        case 3:
            return_val = read_val >> 24;
            break;
        default:
            return_val = read_val;
            break;
    }

    return return_val;
}

void rv_soc_write_mem(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes)
{
    uint8_t align_offset = address & 0x3;
    rv_uint_xlen address_for_write = 0;
    uint8_t *ptr_address = NULL;

    rv_soc_td *rv_soc = priv;

    DEBUG_PRINT("writing value "PRINTF_FMT " to address "PRINTF_FMT"\n", value, address);
    if((address >= RAM_BASE_ADDR) && (address < (RAM_BASE_ADDR+RAM_SIZE_BYTES)))
    {
        address_for_write = (address-RAM_BASE_ADDR) >> 2;
        ptr_address = (uint8_t *)&rv_soc->ram[address_for_write];
        memcpy(ptr_address+align_offset, &value, nr_bytes);
    }
    else if(address == UART_TX_REG_ADDR)
    {
        DEBUG_PRINT("%c", (char) value);
        return;
    }
    else if((address >= CLINT_BASE_ADDR) && (address <= CLINT_BASE_ADDR_END))
    {
        if(write_clint_reg(&rv_soc->rv_core0.clint, address, value))
        {
            die_msg("Error writing clint reg "PRINTF_FMT"\n", address);
        }
    }
    else
    {
        DEBUG_PRINT("Invalid address! "PRINTF_FMT"\n", address);
        return;
    }

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

    if(lsize > sizeof(rv_soc->ram))
    {
        printf("Not able to load fw file of size %lu, ram space is %lu\n", lsize, sizeof(rv_soc->ram));
        exit(-2);
    }

    memset(rv_soc, 0, sizeof(rv_soc_td));

    /* initialize one core with a csr table */
    #ifdef CSR_SUPPORT
        RV_CORE_INSTANTIATE_CSR_REGS_FOR_CORE(csr_regs_core0);
        rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem, &csr_regs_core0_desc);
    #else
        rv_core_init(&rv_soc->rv_core0, rv_soc, rv_soc_read_mem, rv_soc_write_mem, NULL);
    #endif
    

    /* set some registers initial value to match qemu's */
    rv_soc->rv_core0.x[11] = 0x00001020;

    result = fread(&rv_soc->ram, sizeof(char), lsize, p_fw_file);
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
