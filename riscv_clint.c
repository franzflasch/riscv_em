#include <riscv_clint.h>

#define CLINT_MSIP_OFFS     0x0000
#define CLINT_MTIMECMP_OFFS 0x4000
#define CLINT_MTIME_OFFS    0xBFF8

int read_clint_reg(clint_td *clint, rv_uint_xlen address, rv_uint_xlen *outval)
{
    int ret_val = CLINT_ACCESS_ERR;
    rv_uint_xlen tmp_addr = (address & 0xFFFF);

    switch(tmp_addr)
    {
        case(CLINT_MSIP_OFFS):
            *outval = clint->regs[clint_msip];
            ret_val = CLINT_ACCESS_OK;
            break;
        case(CLINT_MTIMECMP_OFFS):
            *outval = clint->regs[clint_mtimecmp];
            ret_val = CLINT_ACCESS_OK;
            break;
        default:
            break;
    }

    return ret_val;
}

int write_clint_reg(clint_td *clint, rv_uint_xlen address, rv_uint_xlen val)
{
    int ret_val = CLINT_ACCESS_ERR;
    rv_uint_xlen tmp_addr = (address & 0xFFFF);

    switch(tmp_addr)
    {
        case(CLINT_MSIP_OFFS):
            clint->regs[clint_msip] = val;
            ret_val = CLINT_ACCESS_OK;
            break;
        case(CLINT_MTIMECMP_OFFS):
            clint->regs[clint_mtimecmp] = val;
            ret_val = CLINT_ACCESS_OK;
            break;
        default:
            break;
    }

    return ret_val;
}
