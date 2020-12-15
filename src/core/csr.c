#include <stdio.h>

#include <csr.h>

rv_uint_xlen *get_reg_reference(csr_reg_desc_td *reg_table, uint16_t address)
{
    uint16_t i = 0;
    rv_uint_xlen *ret_val = NULL;

    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            ret_val = &reg_table->regs[i].value;
            break;
        }
    }

    return ret_val;
}

/* this is only used for internal emulator use to be able to override any value regardless of access flags and write mask */
int read_csr_reg_internal(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen *out_val)
{
    uint16_t i = 0;
    int ret_val = CSR_ACCESS_ERR;

    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            *out_val = reg_table->regs[i].value;
            ret_val = CSR_ACCESS_OK;
            break;
        }
    }

    return ret_val;
}

/* this is only used for internal emulator use to be able to override any value regardless of access flags and write mask */
int write_csr_reg_internal(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen val)
{
    uint16_t i = 0;
    int ret_val = CSR_ACCESS_ERR;

    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            reg_table->regs[i].value = val;
            ret_val = CSR_ACCESS_OK;
            break;
        }
    }

    return ret_val;
}

int read_csr_reg(csr_reg_desc_td *reg_table, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val)
{
    uint16_t i = 0;
    int ret_val = CSR_ACCESS_ERR;

    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            if(CSR_ACCESS_READ_GRANTED(curr_priv_mode, reg_table->regs[i].access_flags))
            {
                *out_val = reg_table->regs[i].value;
                ret_val = CSR_ACCESS_OK;
                break;
            }
        }
    }

    return ret_val;
}

int write_csr_reg(csr_reg_desc_td *reg_table, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val)
{
    uint16_t i = 0;
    int ret_val = CSR_ACCESS_ERR;

    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            if(CSR_ACCESS_WRITE_GRANTED(curr_priv_mode, reg_table->regs[i].access_flags))
            {
                reg_table->regs[i].value = val & reg_table->regs[i].write_mask;
                ret_val = CSR_ACCESS_OK;
                // printf("csr val %lx reg_val %lx mask %lx address %x\n", val, reg_table->regs[i].value, reg_table->regs[i].write_mask, address);
                break;
            }
        }
    }

    return ret_val;
}
