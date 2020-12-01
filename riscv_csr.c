#include <riscv_csr.h>

int read_csr_reg(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen *out_val)
{
    uint16_t i = 0;
    int ret_val = CSR_ACCESS_ERR;
    
    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            if(reg_table->regs[i].access_flags & CSR_ACCESS_MREAD)
            {
                *out_val = reg_table->regs[i].value;
                ret_val = CSR_ACCESS_OK;
                break;
            }
        }
    }

    return ret_val;
}

int write_csr_reg(csr_reg_desc_td *reg_table, uint16_t address, rv_uint_xlen val)
{
    uint16_t i = 0;
    int ret_val = CSR_ACCESS_ERR;
    
    for(i=0;i<reg_table->reg_size;i++)
    {
        if(reg_table->regs[i].address == address)
        {
            if(reg_table->regs[i].access_flags & CSR_ACCESS_MWRITE)
            {
                reg_table->regs[i].value = val;
                ret_val = CSR_ACCESS_OK;
                break;
            }
        }
    }

    return ret_val;
}
