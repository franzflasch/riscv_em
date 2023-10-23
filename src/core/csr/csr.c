#include <stdio.h>

#include <csr.h>

#include <riscv_helper.h>

rv_ret csr_read_reg(csr_reg_td *csr_regs, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen *out_val)
{
    if(address>CSR_ADDR_MAX)
        return rv_err;

    rv_ret ret_val = rv_err;
    rv_uint_xlen tmp_out_val = 0;

    if(CSR_ACCESS_READ_GRANTED(curr_priv_mode, csr_regs[address].access_flags))
    {
        if(csr_regs[address].read_cb)
        {
            ret_val = csr_regs[address].read_cb(csr_regs[address].priv, curr_priv_mode, csr_regs[address].internal_reg, &tmp_out_val);
        }
        else
        {
            tmp_out_val = csr_regs[address].value;
            ret_val = rv_ok;
        }

        /* always or'ed with WARL always enabled bits */
        *out_val = (tmp_out_val | csr_regs[address].warl_always_enabled);
        return ret_val;
    }

    return rv_err;
}

rv_ret csr_write_reg(csr_reg_td *csr_regs, privilege_level curr_priv_mode, uint16_t address, rv_uint_xlen val)
{
    // printf("csr write %x\n", address);
    if(address>CSR_ADDR_MAX)
        return rv_err;

    if(CSR_ACCESS_WRITE_GRANTED(curr_priv_mode, csr_regs[address].access_flags))
    {
        if(csr_regs[address].write_cb)
        {
            // printf("write cb! %x "PRINTF_FMT"\n", address, val);
            return csr_regs[address].write_cb(csr_regs[address].priv, curr_priv_mode, csr_regs[address].internal_reg, val);
        }
        
        // printf("csr addr: %x %x\n", address, csr_regs[address].value);
        csr_regs[address].value = val & csr_regs[address].mask;
        return rv_ok;
    }

    return rv_err;
}
