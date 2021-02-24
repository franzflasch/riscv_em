#include <stdio.h>

#include <csr.h>

rv_uint_xlen *csr_get_reg_reference(csr_reg_desc_td *reg_table, uint16_t address)
{
    return &reg_table->regs[address].value;
}

/* this is only used for internal emulator use to be able to override any value regardless of access flags and write mask */
void csr_read_reg_internal(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen *out_val)
{
    *out_val = reg_table->regs[address].value;
}

/* this is only used for internal emulator use to be able to override any value regardless of access flags and write mask */
void csr_write_reg_internal(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen val)
{
    reg_table->regs[address].value = val;
}

int csr_read_reg(csr_reg_desc_td *reg_table, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val)
{
    if(address>CSR_ADDR_MAX)
        return CSR_ACCESS_ERR;

    if(CSR_ACCESS_READ_GRANTED(curr_priv_mode, reg_table->regs[address].access_flags))
    {
        *out_val = reg_table->regs[address].value;
        return CSR_ACCESS_OK;
    }

    return CSR_ACCESS_ERR;
}

int csr_write_reg(csr_reg_desc_td *reg_table, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val)
{
    if(address>CSR_ADDR_MAX)
        return CSR_ACCESS_ERR;

    if(CSR_ACCESS_WRITE_GRANTED(curr_priv_mode, reg_table->regs[address].access_flags))
    {
        reg_table->regs[address].value = val & reg_table->regs[address].write_mask;
        return CSR_ACCESS_OK;
    }

    return CSR_ACCESS_ERR;
}
