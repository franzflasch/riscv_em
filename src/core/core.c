#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_config.h>
#include <riscv_helper.h>
#include <riscv_instr.h>

#include <core.h>

/* Defines */
#define XREG_STACK_POINTER 2
#define XREG_THREAD_POINTER 5

#define STACK_POINTER_START_VAL 0x0

/* Helpers */
/* portable signextension from: https://stackoverflow.com/a/31655073 */
#ifdef RV64
    #define SIGNEX(v, sb) ((v) | (((v) & (1LL << (sb))) ? ~((1LL << (sb))-1LL) : 0))
#else
    #define SIGNEX(v, sb) ((v) | (((v) & (1 << (sb))) ? ~((1 << (sb))-1) : 0))
#endif

#define ADDR_MISALIGNED(addr) (addr & 0x3)

static inline uint32_t extract32(uint32_t value, int start, int length)
{
    return (value >> start) & (~0U >> (32 - length));
}

static inline void prepare_sync_trap(rv_core_td *rv_core, rv_uint_xlen cause)
{
    if(!rv_core->sync_trap_pending)
    {
        rv_core->sync_trap_pending = 1;
        rv_core->sync_trap_cause = cause;
    }
}

static void instr_NOP(rv_core_td *rv_core)
{
    (void) rv_core;
    return;
}

/* RISCV Instructions */
static void instr_LUI(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = (rv_core->immediate << 12);
}

static void instr_AUIPC(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = (rv_core->pc) + (rv_core->immediate << 12);
}

static void instr_JAL(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = rv_core->pc + 4;

    if(ADDR_MISALIGNED(rv_core->jump_offset))
    {
        prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
        return;
    }

    rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
}

static void instr_JALR(rv_core_td *rv_core)
{
    rv_uint_xlen curr_pc = rv_core->pc + 4;
    rv_core->jump_offset = SIGNEX(rv_core->immediate, 11);

    if(ADDR_MISALIGNED(rv_core->jump_offset))
    {
        prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
        return;
    }

    rv_core->next_pc = (rv_core->x[rv_core->rs1] + rv_core->jump_offset);
    rv_core->next_pc &= ~(1<<0);
    rv_core->x[rv_core->rd] = curr_pc;
}

static void instr_BEQ(rv_core_td *rv_core)
{
    if(rv_core->x[rv_core->rs1] == rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BNE(rv_core_td *rv_core)
{
    if(rv_core->x[rv_core->rs1] != rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BLT(rv_core_td *rv_core)
{
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
    rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs < signed_rs2)
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BGE(rv_core_td *rv_core)
{
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
    rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs >= signed_rs2)
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BLTU(rv_core_td *rv_core)
{
    if(rv_core->x[rv_core->rs1] < rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BGEU(rv_core_td *rv_core)
{
    if(rv_core->x[rv_core->rs1] >= rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_ADDI(rv_core_td *rv_core)
{
    rv_int_xlen signed_immediate = SIGNEX(rv_core->immediate, 11);
    rv_int_xlen signed_rs_val = rv_core->x[rv_core->rs1];
    rv_core->x[rv_core->rd] = (signed_immediate + signed_rs_val);
}

static void instr_SLTI(rv_core_td *rv_core)
{
    rv_int_xlen signed_immediate = SIGNEX(rv_core->immediate, 11);
    rv_int_xlen signed_rs_val = rv_core->x[rv_core->rs1];

    if(signed_rs_val < signed_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTIU(rv_core_td *rv_core)
{
    rv_uint_xlen unsigned_immediate = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen unsigned_rs_val = rv_core->x[rv_core->rs1];

    if(unsigned_rs_val < unsigned_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_XORI(rv_core_td *rv_core)
{
    rv_int_xlen signed_immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    signed_immediate = rv_core->immediate;

    if(signed_immediate == -1)
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ -1;
    else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ rv_core->immediate;
}

static void instr_ORI(rv_core_td *rv_core)
{
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | rv_core->immediate;
}

static void instr_ANDI(rv_core_td *rv_core)
{
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & rv_core->immediate;
}

static void instr_SLLI(rv_core_td *rv_core)
{
    #ifdef RV64
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x3F));
    #else
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x1F));
    #endif
}

static void instr_SRAI(rv_core_td *rv_core)
{
    rv_int_xlen rs_val = rv_core->x[rv_core->rs1];

    /* a right shift on signed ints seem to be always arithmetic */
    #ifdef RV64
        rs_val = rs_val >> (rv_core->immediate & 0x3F);
    #else
        rs_val = rs_val >> (rv_core->immediate & 0x1F);
    #endif
    rv_core->x[rv_core->rd] = rs_val;
}

static void instr_SRLI(rv_core_td *rv_core)
{
    #ifdef RV64
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] >> (rv_core->immediate & 0x3F));
    #else
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] >> (rv_core->immediate & 0x1F));
    #endif
}

static void instr_ADD(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] + rv_core->x[rv_core->rs2];
}

static void instr_SUB(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] - rv_core->x[rv_core->rs2];
}

static void instr_SLL(rv_core_td *rv_core)
{
    #ifdef RV64
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] << (rv_core->x[rv_core->rs2] & 0x3F);
    #else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] << (rv_core->x[rv_core->rs2]);
    #endif
}

static void instr_SLT(rv_core_td *rv_core)
{
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
    rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs < signed_rs2) rv_core->x[rv_core->rd] = 1;
    else rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTU(rv_core_td *rv_core)
{
    if(rv_core->rs1 == 0)
    {
        if(rv_core->x[rv_core->rs2])
            rv_core->x[rv_core->rd] = 1;
        else
            rv_core->x[rv_core->rd] = 0;
    }
    else
    {
        if(rv_core->x[rv_core->rs1] < rv_core->x[rv_core->rs2]) 
            rv_core->x[rv_core->rd] = 1;
        else
            rv_core->x[rv_core->rd] = 0;
    }
}

static void instr_XOR(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ rv_core->x[rv_core->rs2];
}

static void instr_SRL(rv_core_td *rv_core)
{
    #ifdef RV64
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] >> (rv_core->x[rv_core->rs2] & 0x3F);
    #else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] >> (rv_core->x[rv_core->rs2]);
    #endif
}

static void instr_OR(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | (rv_core->x[rv_core->rs2]);
}

static void instr_AND(rv_core_td *rv_core)
{
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & (rv_core->x[rv_core->rs2]);
}

static void instr_SRA(rv_core_td *rv_core)
{
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];

    #ifdef RV64
        rv_core->x[rv_core->rd] = signed_rs >> (rv_core->x[rv_core->rs2] & 0x3F);
    #else
        rv_core->x[rv_core->rd] = signed_rs >> (rv_core->x[rv_core->rs2]);
    #endif
}

static void instr_LB(rv_core_td *rv_core)
{
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 7);
}

static void instr_LH(rv_core_td *rv_core)
{
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 15);
}

static void instr_LW(rv_core_td *rv_core)
{
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    #ifdef RV64
        uint32_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
        rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 31);
    #else
        rv_core->x[rv_core->rd] = rv_core->read_mem(rv_core->priv, address, &err);
    #endif
}

static void instr_LBU(rv_core_td *rv_core)
{
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_LHU(rv_core_td *rv_core)
{
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_SB(rv_core_td *rv_core)
{
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t value_to_write = (uint8_t)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 1);
}

static void instr_SH(rv_core_td *rv_core)
{
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t value_to_write = (uint16_t)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 2);
}

static void instr_SW(rv_core_td *rv_core)
{
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    rv_uint_xlen value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 4);
}

#ifdef RV64
    static void instr_LWU(rv_core_td *rv_core)
    {
        int err = RV_CORE_E_ERR;
        rv_uint_xlen unsigned_offset = SIGNEX(rv_core->immediate, 11);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + unsigned_offset;
        uint32_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
        rv_core->x[rv_core->rd] = tmp_load_val;
    }

    static void instr_LD(rv_core_td *rv_core)
    {
        int err = RV_CORE_E_ERR;
        rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
        rv_core->x[rv_core->rd] = rv_core->read_mem(rv_core->priv, address, &err);
    }

    static void instr_SD(rv_core_td *rv_core)
    {
        rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
        rv_uint_xlen value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
        rv_core->write_mem(rv_core->priv, address, value_to_write, 8);
    }

    static void instr_SRAIW(rv_core_td *rv_core)
    {
        int32_t signed_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (signed_rs_val >> (rv_core->immediate & 0x1F));
    }

    static void instr_ADDIW(rv_core_td *rv_core)
    {
        int32_t signed_immediate = SIGNEX(rv_core->immediate, 11);
        int32_t signed_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (signed_rs_val + signed_immediate);
    }

    static void instr_SLLIW(rv_core_td *rv_core)
    {
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x1F)) & 0xFFFFFFFF;
        rv_core->x[rv_core->rd] = SIGNEX(rv_core->x[rv_core->rd], 31);
    }

    static void instr_SRLIW(rv_core_td *rv_core)
    {
        uint32_t unsigned_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (unsigned_rs_val >> (rv_core->immediate & 0x1F));
        rv_core->x[rv_core->rd] = SIGNEX(rv_core->x[rv_core->rd], 31);
    }

    static void instr_SRLW(rv_core_td *rv_core)
    {
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val >> rs2_val, 31);
    }

    static void instr_SRAW(rv_core_td *rv_core)
    {
        int32_t rs1_val_signed = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val_signed >> rs2_val, 31);
    }

    static void instr_SLLW(rv_core_td *rv_core)
    {
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val << rs2_val, 31);
    }

    static void instr_ADDW(rv_core_td *rv_core)
    {
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val + rs2_val, 31);
    }

    static void instr_SUBW(rv_core_td *rv_core)
    {
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val - rs2_val, 31);
    }
#endif

#ifdef CSR_SUPPORT
    static void instr_CSRRW(rv_core_td *rv_core)
    {
        rv_uint_xlen csr_val = 0;

        // printf("CSR: " PRINTF_FMT "\n", rv_core->immediate);

        if(read_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
            die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);

        if(write_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, rv_core->x[rv_core->rs1]))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRS(rv_core_td *rv_core)
    {
        rv_uint_xlen csr_val = 0;
        if(read_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
            die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);

        if(write_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val | rv_core->x[rv_core->rs1]))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRC(rv_core_td *rv_core)
    {
        rv_uint_xlen csr_val = 0;
        if(read_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
            die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);

        if(write_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val & ~rv_core->x[rv_core->rs1]))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRWI(rv_core_td *rv_core)
    {
        rv_uint_xlen csr_val = 0;
        if(read_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
            die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);

        if(write_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, rv_core->rs1))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRSI(rv_core_td *rv_core)
    {
        rv_uint_xlen csr_val = 0;
        if(read_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        if(write_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val | rv_core->rs1))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRCI(rv_core_td *rv_core)
    {
        rv_uint_xlen csr_val = 0;
        if(read_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
            die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);

        if(write_csr_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val & ~rv_core->rs1))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);
        
        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_ECALL(rv_core_td *rv_core)
    {
        prepare_sync_trap(rv_core, CSR_MCAUSE_ECALL_M);
    }

    static void instr_EBREAK(rv_core_td *rv_core)
    {
        /* not implemented */
        (void)rv_core;
    }

    static void instr_MRET(rv_core_td *rv_core)
    {        
        rv_core->next_pc = *rv_core->mepc;
        rv_core->in_irq = 0;

        if(rv_core->is_sync_trap)
        {
            rv_core->is_sync_trap = 0;
            rv_core->sync_trap_pending = 0;
        }

        /* Restore MPP and MIE */
        rv_core->curr_priv_mode = (*rv_core->mstatus & (CSR_MSTATUS_MPP_MASK << CSR_MSTATUS_MPP_BIT)) >> CSR_MSTATUS_MPP_BIT;
        assign_xlen_bit(rv_core->mstatus, CSR_MSTATUS_MIE_BIT, (1 << CSR_MSTATUS_MPIE_BIT) >> CSR_MSTATUS_MPIE_BIT);
    }

    static void instr_SRET(rv_core_td *rv_core)
    {
        /* not implemented */
        (void)rv_core;
    }

    static void instr_URET(rv_core_td *rv_core)
    {
        /* not implemented */
        (void)rv_core;
    }

    static void preparation_func12(rv_core_td *rv_core, int32_t *next_subcode)
    {
        rv_core->func12 = ((rv_core->instruction >> 20) & 0x0FFF);
        *next_subcode = rv_core->func12;
    }
#endif

static void preparation_func7(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->func7 = ((rv_core->instruction >> 25) & 0x7F);
    *next_subcode = rv_core->func7;
}

#ifdef RV64
    static void preparation_func6(rv_core_td *rv_core, int32_t *next_subcode)
    {
        rv_core->func6 = ((rv_core->instruction >> 26) & 0x3F);
        *next_subcode = rv_core->func6;
    }
#endif

static void R_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->func3 = ((rv_core->instruction >> 12) & 0x7);
    rv_core->rs1 = ((rv_core->instruction >> 15) & 0x1F);
    rv_core->rs2 = ((rv_core->instruction >> 20) & 0x1F);
    *next_subcode = rv_core->func3;
}

static void I_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->func3 = ((rv_core->instruction >> 12) & 0x7);
    rv_core->rs1 = ((rv_core->instruction >> 15) & 0x1F);
    rv_core->immediate = ((rv_core->instruction >> 20) & 0xFFF);
    *next_subcode = rv_core->func3;
}

static void S_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->func3 = ((rv_core->instruction >> 12) & 0x7);
    rv_core->rs1 = ((rv_core->instruction >> 15) & 0x1F);
    rv_core->rs2 = ((rv_core->instruction >> 20) & 0x1F);
    rv_core->immediate = (((rv_core->instruction >> 25) << 5) | ((rv_core->instruction >> 7) & 0x1F));
    *next_subcode = rv_core->func3;
}

static void B_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->func3 = ((rv_core->instruction >> 12) & 0x7);
    rv_core->rs1 = ((rv_core->instruction >> 15) & 0x1F);
    rv_core->rs2 = ((rv_core->instruction >> 20) & 0x1F);
    rv_core->jump_offset=((extract32(rv_core->instruction, 8, 4) << 1) |
                          (extract32(rv_core->instruction, 25, 6) << 5) |
                          (extract32(rv_core->instruction, 7, 1) << 11));
    rv_core->jump_offset = SIGNEX(rv_core->jump_offset, 11);
    *next_subcode = rv_core->func3;
}

static void U_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->immediate = ((rv_core->instruction >> 12) & 0xFFFFF);
    rv_core->immediate = SIGNEX(rv_core->immediate, 19);
    *next_subcode = -1;
}

static void J_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->jump_offset=((extract32(rv_core->instruction, 21, 10) << 1) |
                          (extract32(rv_core->instruction, 20, 1) << 11) |
                          (extract32(rv_core->instruction, 12, 8) << 12) );
    /* sign extend the 20 bit number */
    rv_core->jump_offset = SIGNEX(rv_core->jump_offset, 19);
    *next_subcode = -1;
}

static instruction_hook_td JALR_func3_subcode_list[] = {
    [FUNC3_INSTR_JALR] = {NULL, instr_JALR, NULL},
};
INIT_INSTRUCTION_LIST_DESC(JALR_func3_subcode_list);

static instruction_hook_td BEQ_BNE_BLT_BGE_BLTU_BGEU_func3_subcode_list[] = {
    [FUNC3_INSTR_BEQ] = {NULL, instr_BEQ, NULL},
    [FUNC3_INSTR_BNE] = {NULL, instr_BNE, NULL},
    [FUNC3_INSTR_BLT] = {NULL, instr_BLT, NULL},
    [FUNC3_INSTR_BGE] = {NULL, instr_BGE, NULL},
    [FUNC3_INSTR_BLTU] = {NULL, instr_BLTU, NULL},
    [FUNC3_INSTR_BGEU] = {NULL, instr_BGEU, NULL},
};
INIT_INSTRUCTION_LIST_DESC(BEQ_BNE_BLT_BGE_BLTU_BGEU_func3_subcode_list);

static instruction_hook_td LB_LH_LW_LBU_LHU_LWU_LD_func3_subcode_list[] = {
    [FUNC3_INSTR_LB] = {NULL, instr_LB, NULL},
    [FUNC3_INSTR_LH] = {NULL, instr_LH, NULL},
    [FUNC3_INSTR_LW] = {NULL, instr_LW, NULL},
    [FUNC3_INSTR_LBU] = {NULL, instr_LBU, NULL},
    [FUNC3_INSTR_LHU] = {NULL, instr_LHU, NULL},

    #ifdef RV64
        [FUNC3_INSTR_LWU] = {NULL, instr_LWU, NULL},
        [FUNC3_INSTR_LD] = {NULL, instr_LD, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(LB_LH_LW_LBU_LHU_LWU_LD_func3_subcode_list);

static instruction_hook_td SB_SH_SW_SD_func3_subcode_list[] = {
    [FUNC3_INSTR_SB] = {NULL, instr_SB, NULL},
    [FUNC3_INSTR_SH] = {NULL, instr_SH, NULL},
    [FUNC3_INSTR_SW] = {NULL, instr_SW, NULL},
    #ifdef RV64
        [FUNC3_INSTR_SD] = {NULL, instr_SD, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(SB_SH_SW_SD_func3_subcode_list);

#ifdef RV64
    static instruction_hook_td SRLI_SRAI_func6_subcode_list[] = {
        [FUNC6_INSTR_SRLI] = {NULL, instr_SRLI, NULL},
        [FUNC6_INSTR_SRAI] = {NULL, instr_SRAI, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRLI_SRAI_func6_subcode_list);
#else
    static instruction_hook_td SRLI_SRAI_func7_subcode_list[] = {
        [FUNC7_INSTR_SRLI] = {NULL, instr_SRLI, NULL},
        [FUNC7_INSTR_SRAI] = {NULL, instr_SRAI, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRLI_SRAI_func7_subcode_list);
#endif

static instruction_hook_td ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list[] = {
    [FUNC3_INSTR_ADDI] = {NULL, instr_ADDI, NULL},
    [FUNC3_INSTR_SLTI] = {NULL, instr_SLTI, NULL},
    [FUNC3_INSTR_SLTIU] = {NULL, instr_SLTIU, NULL},
    [FUNC3_INSTR_XORI] = {NULL, instr_XORI, NULL},
    [FUNC3_INSTR_ORI] = {NULL, instr_ORI, NULL},
    [FUNC3_INSTR_ANDI] = {NULL, instr_ANDI, NULL},
    [FUNC3_INSTR_SLLI] = {NULL, instr_SLLI, NULL},
    #ifdef RV64
        [FUNC3_INSTR_SRLI_SRAI] = {preparation_func6, NULL, &SRLI_SRAI_func6_subcode_list_desc},
    #else
        [FUNC3_INSTR_SRLI_SRAI] = {preparation_func7, NULL, &SRLI_SRAI_func7_subcode_list_desc},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list);

static instruction_hook_td ADD_SUB_func7_subcode_list[] = {
    [FUNC7_INSTR_ADD] = {NULL, instr_ADD, NULL},
    [FUNC7_INSTR_SUB] = {NULL, instr_SUB, NULL},
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_func7_subcode_list);

static instruction_hook_td SLL_func7_subcode_list[] = {
    [FUNC7_INSTR_SLL] = {NULL, instr_SLL, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SLL_func7_subcode_list);

static instruction_hook_td SLT_func7_subcode_list[] = {
    [FUNC7_INSTR_SLT] = {NULL, instr_SLT, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SLT_func7_subcode_list);

static instruction_hook_td SLTU_func7_subcode_list[] = {
    [FUNC7_INSTR_SLTU] = {NULL, instr_SLTU, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SLTU_func7_subcode_list);

static instruction_hook_td XOR_func7_subcode_list[] = {
    [FUNC7_INSTR_SLTU] = {NULL, instr_XOR, NULL},
};
INIT_INSTRUCTION_LIST_DESC(XOR_func7_subcode_list);

static instruction_hook_td SRL_SRA_func7_subcode_list[] = {
    [FUNC7_INSTR_SRL] = {NULL, instr_SRL, NULL},
    [FUNC7_INSTR_SRA] = {NULL, instr_SRA, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SRL_SRA_func7_subcode_list);

static instruction_hook_td OR_func7_subcode_list[] = {
    [FUNC7_INSTR_OR] = {NULL, instr_OR, NULL},
};
INIT_INSTRUCTION_LIST_DESC(OR_func7_subcode_list);

static instruction_hook_td AND_func7_subcode_list[] = {
    [FUNC7_INSTR_AND] = {NULL, instr_AND, NULL},
};
INIT_INSTRUCTION_LIST_DESC(AND_func7_subcode_list);

static instruction_hook_td ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list[] = {
    [FUNC3_INSTR_ADD_SUB] = {preparation_func7, NULL, &ADD_SUB_func7_subcode_list_desc},
    [FUNC3_INSTR_SLL] = {preparation_func7, NULL, &SLL_func7_subcode_list_desc},
    [FUNC3_INSTR_SLT] = {preparation_func7, NULL, &SLT_func7_subcode_list_desc},
    [FUNC3_INSTR_SLTU] = {preparation_func7, NULL, &SLTU_func7_subcode_list_desc},
    [FUNC3_INSTR_XOR] = {preparation_func7, NULL, &XOR_func7_subcode_list_desc},
    [FUNC3_INSTR_SRL_SRA] = {preparation_func7, NULL, &SRL_SRA_func7_subcode_list_desc},
    [FUNC3_INSTR_OR] = {preparation_func7, NULL, &OR_func7_subcode_list_desc},
    [FUNC3_INSTR_AND] = {preparation_func7, NULL, &AND_func7_subcode_list_desc},
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list);

#ifdef RV64
    static instruction_hook_td SLLIW_func7_subcode_list[] = {
        [FUNC7_INSTR_SLLIW] = {NULL, instr_SLLIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SLLIW_func7_subcode_list);

    static instruction_hook_td SRLIW_SRAIW_func7_subcode_list[] = {
        [FUNC7_INSTR_SRLIW] = {NULL, instr_SRLIW, NULL},
        [FUNC7_INSTR_SRAIW] = {NULL, instr_SRAIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRLIW_SRAIW_func7_subcode_list);

    static instruction_hook_td SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list[] = {
        [FUNC3_INSTR_SLLIW] = {preparation_func7, NULL, &SLLIW_func7_subcode_list_desc},
        [FUNC3_INSTR_SRLIW_SRAIW] = {preparation_func7, NULL, &SRLIW_SRAIW_func7_subcode_list_desc},
        [FUNC3_INSTR_ADDIW] = {NULL, instr_ADDIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list);

    static instruction_hook_td SRLW_SRAW_func7_subcode_list[] = {
        [FUNC7_INSTR_SRLW] = {NULL, instr_SRLW, NULL},
        [FUNC7_INSTR_SRAW] = {NULL, instr_SRAW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRLW_SRAW_func7_subcode_list);

    static instruction_hook_td SLLW_func7_subcode_list[] = {
        [FUNC7_INSTR_SLLW] = {NULL, instr_SLLW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SLLW_func7_subcode_list);

    static instruction_hook_td ADDW_SUBW_func7_subcode_list[] = {
        [FUNC7_INSTR_ADDW] = {NULL, instr_ADDW, NULL},
        [FUNC7_INSTR_SUBW] = {NULL, instr_SUBW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(ADDW_SUBW_func7_subcode_list);

    static instruction_hook_td ADDW_SUBW_SLLW_SRLW_SRAW_func3_subcode_list[] = {
        [FUNC3_INSTR_ADDW_SUBW] = {preparation_func7, NULL, &ADDW_SUBW_func7_subcode_list_desc},
        [FUNC3_INSTR_SLLW] = {preparation_func7, NULL, &SLLW_func7_subcode_list_desc},
        [FUNC3_INSTR_SRLW_SRAW] = {preparation_func7, instr_ADDIW, &SRLW_SRAW_func7_subcode_list_desc},
    };
    INIT_INSTRUCTION_LIST_DESC(ADDW_SUBW_SLLW_SRLW_SRAW_func3_subcode_list);
#endif

#ifdef CSR_SUPPORT
    static instruction_hook_td ECALL_EBREAK_MRET_SRET_URET_func12_subcode_list[] = {
        [FUNC12_ECALL] = {NULL, instr_ECALL, NULL},
        [FUNC12_EBREAK] = {NULL, instr_EBREAK, NULL},
        [FUNC12_INSTR_MRET] = {NULL, instr_MRET, NULL},
        [FUNC12_INSTR_SRET] = {NULL, instr_SRET, NULL},
        [FUNC12_INSTR_URET] = {NULL, instr_URET, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(ECALL_EBREAK_MRET_SRET_URET_func12_subcode_list);

    static instruction_hook_td ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_func3_subcode_list[] = {
        [FUNC3_INSTR_CSRRW] = {NULL, instr_CSRRW, NULL},
        [FUNC3_INSTR_CSRRS] = {NULL, instr_CSRRS, NULL},
        [FUNC3_INSTR_CSRRC] = {NULL, instr_CSRRC, NULL},
        [FUNC3_INSTR_CSRRWI] = {NULL, instr_CSRRWI, NULL},
        [FUNC3_INSTR_CSRRSI] = {NULL, instr_CSRRSI, NULL},
        [FUNC3_INSTR_CSRRCI] = {NULL, instr_CSRRCI, NULL},
        [FUNC3_INSTR_ECALL_EBREAK_MRET_SRET_URET] = {preparation_func12, NULL, &ECALL_EBREAK_MRET_SRET_URET_func12_subcode_list_desc}
    };
    INIT_INSTRUCTION_LIST_DESC(ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_func3_subcode_list);
#endif

static instruction_hook_td RV_opcode_list[] = {
    [INSTR_LUI] = {U_type_preparation, instr_LUI, NULL},
    [INSTR_AUIPC] = {U_type_preparation, instr_AUIPC, NULL},
    [INSTR_JAL] = {J_type_preparation, instr_JAL, NULL},
    [INSTR_JALR] = {I_type_preparation, NULL, &JALR_func3_subcode_list_desc},
    [INSTR_BEQ_BNE_BLT_BGE_BLTU_BGEU] = {B_type_preparation, NULL, &BEQ_BNE_BLT_BGE_BLTU_BGEU_func3_subcode_list_desc},
    [INSTR_LB_LH_LW_LBU_LHU_LWU_LD] = {I_type_preparation, NULL, &LB_LH_LW_LBU_LHU_LWU_LD_func3_subcode_list_desc},
    [INSTR_SB_SH_SW_SD] = {S_type_preparation, NULL, &SB_SH_SW_SD_func3_subcode_list_desc},
    [INSTR_ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI] = {I_type_preparation, NULL, &ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list_desc},
    [INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND] = {R_type_preparation, NULL, &ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list_desc},
    [INSTR_FENCE_FENCE_I] = {NULL, instr_NOP, NULL}, /* Not implemented */

    #ifdef CSR_SUPPORT
        [INSTR_ECALL_EBREAK_MRET_SRET_URET_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI] = {I_type_preparation, NULL, &ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_func3_subcode_list_desc},
    #endif

    #ifdef RV64
        [INSTR_ADDIW_SLLIW_SRLIW_SRAIW] = {I_type_preparation, NULL, &SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list_desc},
        [INSTR_ADDW_SUBW_SLLW_SRLW_SRAW] = {R_type_preparation, NULL, &ADDW_SUBW_SLLW_SRLW_SRAW_func3_subcode_list_desc},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(RV_opcode_list);

static void rv_call_from_opcode_list(rv_core_td *rv_core, instruction_desc_td *opcode_list_desc, uint32_t opcode)
{
    int32_t next_subcode = -1;

    // if(rv_core->pc == 0x800008ac)
    // {
    //     printf("DBG %x %x\n", rv_core->func3, rv_core->func7);
    // }

    unsigned int list_size = opcode_list_desc->instruction_hook_list_size;
    instruction_hook_td *opcode_list = opcode_list_desc->instruction_hook_list;

    if( (opcode_list[opcode].preparation_cb == NULL) && 
        (opcode_list[opcode].execution_cb == NULL) && 
        (opcode_list[opcode].next == NULL) )
        die_msg("Unknown instruction: %08x PC: "PRINTF_FMT"\n", rv_core->instruction, rv_core->pc);

    if(opcode >= list_size) 
        die_msg("Unknown instruction: %08x PC: "PRINTF_FMT"\n", rv_core->instruction, rv_core->pc);

    if(opcode_list[opcode].preparation_cb != NULL)
        opcode_list[opcode].preparation_cb(rv_core, &next_subcode);

    if(opcode_list[opcode].execution_cb != NULL)
        rv_core->execute_cb = opcode_list[opcode].execution_cb;

    if((next_subcode != -1) && (opcode_list[opcode].next != NULL))
        rv_call_from_opcode_list(rv_core, opcode_list[opcode].next, next_subcode);
}

#ifdef CSR_SUPPORT
    static inline void rv_core_update_interrupts(rv_core_td *rv_core)
    {
        uint8_t timer_interrupt = (rv_core->clint.regs[clint_mtime] >= rv_core->clint.regs[clint_mtimecmp]);

        /* map msip register to the CSR */
        assign_xlen_bit(rv_core->mip, CSR_MIE_MIP_MSI_BIT, (rv_core->clint.regs[clint_msip] & 1));

        /* map timer interrupt */
        assign_xlen_bit(rv_core->mip, CSR_MIE_MIP_MTI_BIT, timer_interrupt & 1);

        // if(timer_interrupt)
        //     printf("timer irq %d %lx %ld\n", timer_interrupt, *rv_core->mip, rv_core->clint.regs[clint_mtime]);
    }

    static inline void rv_core_do_irq(rv_core_td *rv_core, rv_uint_xlen mepc, rv_uint_xlen mcause, uint8_t is_sync_trap)
    {
        if(!rv_core->in_irq)
        {
            *rv_core->mcause = mcause;
            *rv_core->mepc = mepc;
            rv_core->pc = *rv_core->mtvec;

            /* Save MPP and MIE */
            *rv_core->mstatus |= rv_core->curr_priv_mode << CSR_MSTATUS_MPP_BIT;
            assign_xlen_bit(rv_core->mstatus, CSR_MSTATUS_MPIE_BIT, (*rv_core->mstatus >> CSR_MSTATUS_MIE_BIT) & CSR_MSTATUS_MIE_MASK);

            /* now clear MIE */
            CLEAR_BIT(*rv_core->mstatus, CSR_MSTATUS_MIE_BIT);

            /* internal */
            rv_core->in_irq = 1;
            rv_core->is_sync_trap = is_sync_trap;
        }
    }

    /* Interrupts are prioritized as follows, in decreasing order of priority:
       Machine external interrupts (with configurable external priority)
       Machine software interrupts
       Machine timer interrupts
    */
    static inline uint8_t rv_core_prepare_interrupts(rv_core_td *rv_core)
    {
        /* check if interrupts are globally enabled */
        if(CHECK_BIT(*rv_core->mstatus, CSR_MSTATUS_MIE_BIT))
        {
            /* check if MSI interrupt is enabled */
            if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MSI_BIT))
            {
                /* check if interrupt pending */
                if(CHECK_BIT(*rv_core->mip, CSR_MIE_MIP_MSI_BIT))
                {
                    rv_core_do_irq(rv_core, rv_core->pc, (1UL<<(XLEN-1)) | CSR_MCAUSE_MSI, 0);
                    return 1;
                }
            }

            /* Timer interrupt */
            if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MTI_BIT))
            {
                /* check if interrupt pending */
                if(CHECK_BIT(*rv_core->mip, CSR_MIE_MIP_MTI_BIT))
                {
                    rv_core_do_irq(rv_core, rv_core->pc, (1UL<<(XLEN-1)) | CSR_MCAUSE_MTI, 0);
                    return 1;
                }
            }
        }

        if(rv_core->sync_trap_pending)
        {
            rv_core_do_irq(rv_core, rv_core->pc - 4, rv_core->sync_trap_cause, 1);
            return 1;
        }

        return 0;
    }
#endif

static inline rv_uint_xlen rv_core_fetch(rv_core_td *rv_core)
{
    int err = RV_CORE_E_ERR;
    rv_uint_xlen addr = rv_core->pc;

    rv_core->instruction = rv_core->read_mem(rv_core->priv, addr, &err);

    // printf("INSTR: %x\n", rv_core->instruction);
    // getchar();

    return err;
}

static inline rv_uint_xlen rv_core_decode(rv_core_td *rv_core)
{
    rv_core->opcode = (rv_core->instruction & 0x7F);
    rv_core->rd = 0;
    rv_core->rs1 = 0;
    rv_core->rs2 = 0;
    rv_core->func3 = 0;
    rv_core->func7 = 0;
    rv_core->immediate = 0;
    rv_core->jump_offset = 0;

    rv_call_from_opcode_list(rv_core, &RV_opcode_list_desc, rv_core->opcode);

    return 0;
}

static rv_uint_xlen rv_core_execute(rv_core_td *rv_core)
{
    rv_core->execute_cb(rv_core);

    /* clear x0 if any instruction has written into it */
    rv_core->x[0] = 0;

    return 0;
}

/******************* Public functions *******************************/
void rv_core_run(rv_core_td *rv_core)
{
    rv_core->next_pc = 0;

    if(rv_core_fetch(rv_core))
    {
        prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ACCESS_FAULT);
    }
    else
    {
        rv_core_decode(rv_core);
        rv_core_execute(rv_core);
    }

    /* increase program counter here */
    rv_core->pc = rv_core->next_pc ? rv_core->next_pc : rv_core->pc + 4;

    #ifdef CSR_SUPPORT
        /* interrupt handling */
        rv_core_update_interrupts(rv_core);
        rv_core_prepare_interrupts(rv_core);

        clint_update(&rv_core->clint);
    #endif
}

void rv_core_reg_dump(rv_core_td *rv_core)
{
    (void) rv_core;

    int i = 0;

    DEBUG_PRINT("pc: " PRINTF_FMT "\n", rv_core->pc);
    DEBUG_PRINT("instr: %08x\n", rv_core->instruction);
    for(i=0;i<NR_RVI_REGS;i++)
    {
        DEBUG_PRINT("x[%2d]: " PRINTF_FMT "\n", i, rv_core->x[i]);
    }
}

void rv_core_reg_internal_after_exec(rv_core_td *rv_core)
{
    (void) rv_core;

    DEBUG_PRINT("internal regs after execution:\n");
    DEBUG_PRINT("instruction: %x\n", rv_core->instruction);
    DEBUG_PRINT("rd: %x rs1: %x rs2: %x imm: "PRINTF_FMT"\n", rv_core->rd, rv_core->rs1, rv_core->rs2, rv_core->immediate);
    DEBUG_PRINT("func3: %x func7: %x jump_offset "PRINTF_FMT"\n", rv_core->func3, rv_core->func7, rv_core->jump_offset);
    DEBUG_PRINT("next pc: "PRINTF_FMT"\n", rv_core->pc);
    DEBUG_PRINT("\n");
}

void rv_core_init(rv_core_td *rv_core,
                  void *priv,
                  rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address, int *err),
                  void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes),
                  csr_reg_desc_td *csr_table
                  )
{
    memset(rv_core, 0, sizeof(rv_core_td));

    rv_core->curr_priv_mode = machine_mode;
    rv_core->pc = RAM_BASE_ADDR;
    rv_core->x[XREG_THREAD_POINTER] = RAM_BASE_ADDR;
    rv_core->x[XREG_STACK_POINTER] = STACK_POINTER_START_VAL;

    rv_core->priv = priv;
    rv_core->read_mem = read_mem;
    rv_core->write_mem = write_mem;

    rv_core->csr_table = csr_table;

    /* set fast access pointers */
    rv_core->mstatus = get_reg_reference(csr_table, CSR_ADDR_MSTATUS);
    rv_core->mcause = get_reg_reference(csr_table, CSR_ADDR_MCAUSE);
    rv_core->mepc = get_reg_reference(csr_table, CSR_ADDR_MEPC);
    rv_core->mtvec = get_reg_reference(csr_table, CSR_ADDR_MTVEC);
    rv_core->mie = get_reg_reference(csr_table, CSR_ADDR_MIE);
    rv_core->mip = get_reg_reference(csr_table, CSR_ADDR_MIP);
}
