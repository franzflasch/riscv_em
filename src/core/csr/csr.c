#include <stdio.h>

#include <csr.h>

rv_uint_xlen *csr_get_reg_reference(csr_reg_td *csr_regs, uint16_t address)
{
    return &csr_regs[address].value;
}

/* this is only used for internal emulator use to be able to override any value regardless of access flags and write mask */
void csr_read_reg_internal(csr_reg_td *csr_regs, uint16_t address, rv_uint_xlen *out_val)
{
    *out_val = csr_regs[address].value;
}

/* this is only used for internal emulator use to be able to override any value regardless of access flags and write mask */
void csr_write_reg_internal(csr_reg_td *csr_regs, uint16_t address, rv_uint_xlen val)
{
    csr_regs[address].value = val;
}

int csr_read_reg(csr_reg_td *csr_regs, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val)
{
    if(address>CSR_ADDR_MAX)
        return CSR_ACCESS_ERR;

    if(CSR_ACCESS_READ_GRANTED(curr_priv_mode, csr_regs[address].access_flags))
    {
        *out_val = csr_regs[address].value;
        return CSR_ACCESS_OK;
    }

    return CSR_ACCESS_ERR;
}

int csr_write_reg(csr_reg_td *csr_regs, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val)
{
    if(address>CSR_ADDR_MAX)
        return CSR_ACCESS_ERR;

    if(CSR_ACCESS_WRITE_GRANTED(curr_priv_mode, csr_regs[address].access_flags))
    {
        csr_regs[address].value = val & csr_regs[address].write_mask;
        return CSR_ACCESS_OK;
    }

    return CSR_ACCESS_ERR;
}
