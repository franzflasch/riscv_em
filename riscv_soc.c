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
    rv_uint_xlen read_val = 0;
    rv_uint_xlen read_val2 = 0;
    rv_uint_xlen return_val = 0;

    rv_soc_td *rv_soc = priv;

    if((address >= RAM_BASE_ADDR) && (address < (RAM_BASE_ADDR+RAM_SIZE_BYTES)))
    {
        read_val = rv_soc->ram[(address-RAM_BASE_ADDR) >> 2];
        if(align_offset)
            read_val2 = rv_soc->ram[((address-RAM_BASE_ADDR) >> 2) + 1];
    }

    switch(align_offset)
    {
        case 1:
            return_val = (read_val2 << 24) | (read_val >> 8);
            break;
        case 2:
            return_val = (read_val2 << 16) | (read_val >> 16);
            break;
        case 3:
            return_val = (read_val2 << 8) | (read_val >> 24);
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

    DEBUG_PRINT("writing value %x to address %x\n", value, address);
    if((address >= RAM_BASE_ADDR) && (address < (RAM_BASE_ADDR+RAM_SIZE_BYTES)))
    {
        address_for_write = (address-RAM_BASE_ADDR) >> 2;
        ptr_address = (uint8_t *)&rv_soc->ram[address_for_write];
    }
    else if(address == 0x300000)
    {
        printf("%c", (char) value);
        return;
    }

    memcpy(ptr_address+align_offset, &value, nr_bytes);

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

    rv_core_init(&rv_soc->rv_core, rv_soc, rv_soc_read_mem, rv_soc_write_mem);

    /* set some registers initial value to match qemu's */
    rv_soc->rv_core.x[11] = 0x00001020;

    result = fread(&rv_soc->ram, sizeof(char), lsize, p_fw_file);
    if(result != lsize)
    {
        printf("Error while reading file!\n");
        exit(-3);
    }

    fclose(p_fw_file);

    printf("rv SOC initialized!\n");
}
