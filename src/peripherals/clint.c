#include <stdio.h>
#include <string.h>

#include <riscv_helper.h>

#include <clint.h>

#define CLINT_MSIP_OFFS       0x0000
#define CLINT_MTIMECMP_OFFS   0x4000
#define CLINT_MTIME_OFFS      0xBFF8
#define CLINT_REG_SIZE_BYTES  8

static rv_int_xlen get_u8_arr_index_offs(rv_uint_xlen address)
{
    if(address < (CLINT_MSIP_OFFS + CLINT_REG_SIZE_BYTES))
    {
        return (0*CLINT_REG_SIZE_BYTES);
    }
    else if(ADDR_WITHIN(address, CLINT_MTIMECMP_OFFS, CLINT_REG_SIZE_BYTES))
    {
        return (1*CLINT_REG_SIZE_BYTES);
    }
    else if(ADDR_WITHIN(address, CLINT_MTIME_OFFS, CLINT_REG_SIZE_BYTES))
    {
        return (2*CLINT_REG_SIZE_BYTES);
    }

    return -1;
}

int clint_read_reg(void *priv, rv_uint_xlen address, rv_uint_xlen *outval)
{
    clint_td *clint = priv;
    int ret_val = RV_MEM_ACCESS_ERR;
    rv_uint_xlen tmp_addr = 0;
    uint8_t *tmp_u8 = (uint8_t *)clint->regs;
    rv_int_xlen arr_index_offs = -1;

    arr_index_offs = get_u8_arr_index_offs(address);

    if(arr_index_offs >= 0)
    {
        tmp_addr = (address & 0x7) + arr_index_offs;
        memcpy(outval, &tmp_u8[tmp_addr], sizeof(*outval));
        ret_val = RV_MEM_ACCESS_OK;
    }

    // printf("clint %lx %lx %lx\n", clint->regs[clint_msip], clint->regs[clint_mtime], clint->regs[clint_mtimecmp]);

    return ret_val;
}

int clint_write_reg(void *priv, rv_uint_xlen address, rv_uint_xlen val, uint8_t nr_bytes)
{
    clint_td *clint = priv;
    int ret_val = RV_MEM_ACCESS_ERR;
    rv_uint_xlen tmp_addr = 0;
    uint8_t *tmp_u8 = (uint8_t *)clint->regs;
    rv_int_xlen arr_index_offs = -1;

    arr_index_offs = get_u8_arr_index_offs(address);

    if(arr_index_offs >= 0)
    {
        tmp_addr = (address & 0x7) + arr_index_offs;
        memcpy(&tmp_u8[tmp_addr], &val, nr_bytes);
        ret_val = RV_MEM_ACCESS_OK;
    }

    // printf("mtimecmp: %ld\n", clint->regs[clint_mtimecmp]);

    return ret_val;
}

void clint_update(clint_td *clint, uint8_t *msi, uint8_t *mti)
{
    static uint8_t i = 0;
    if(i%2 == 0)
        clint->regs[clint_mtime]+=1;

    i++;

    *mti = (clint->regs[clint_mtime] >= clint->regs[clint_mtimecmp]);
    *msi = (clint->regs[clint_msip] & 0x1);
}
