#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_config.h>
#include <riscv_helper.h>
#include <riscv_instr.h>

#include <core.h>

// #define CORE_DEBUG
#ifdef CORE_DEBUG
#define CORE_DBG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define CORE_DBG(...) do{ } while ( 0 )
#endif

/* Defines */
#define XREG_STACK_POINTER 2
#define XREG_T0 5

#define STACK_POINTER_START_VAL 0x0

/* Helpers */
/* portable signextension from: https://stackoverflow.com/a/31655073 */
#ifdef RV64
    #define SIGNEX(v, sb) ((v) | (((v) & (1LL << (sb))) ? ~((1LL << (sb))-1LL) : 0))
#else
    #define SIGNEX(v, sb) ((v) | (((v) & (1 << (sb))) ? ~((1 << (sb))-1) : 0))
#endif

#define ADDR_MISALIGNED(addr) (addr & 0x3)

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
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    (void) rv_core;
    return;
}

/* RISCV Instructions */
static void instr_LUI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = (rv_core->immediate << 12);
}

static void instr_AUIPC(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = (rv_core->pc) + (rv_core->immediate << 12);
}

static void instr_JAL(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = rv_core->pc + 4;

    if(ADDR_MISALIGNED(rv_core->jump_offset))
    {
        die_msg("Addr misaligned!\n");
        prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
        return;
    }

    rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
}

static void instr_JALR(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_uint_xlen curr_pc = rv_core->pc + 4;
    rv_core->jump_offset = SIGNEX(rv_core->immediate, 11);

    rv_core->next_pc = (rv_core->x[rv_core->rs1] + rv_core->jump_offset);
    rv_core->next_pc &= ~(1<<0);

    if(ADDR_MISALIGNED(rv_core->next_pc))
    {
        die_msg("Addr misaligned!\n");
        prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
        return;
    }

    rv_core->x[rv_core->rd] = curr_pc;
}

static void instr_BEQ(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    if(rv_core->x[rv_core->rs1] == rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            die_msg("Addr misaligned!\n");
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BNE(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    if(rv_core->x[rv_core->rs1] != rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            die_msg("Addr misaligned!\n");
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BLT(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
    rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs < signed_rs2)
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            die_msg("Addr misaligned!\n");
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BGE(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
    rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs >= signed_rs2)
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            die_msg("Addr misaligned!\n");
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BLTU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    if(rv_core->x[rv_core->rs1] < rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            die_msg("Addr misaligned!\n");
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_BGEU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    if(rv_core->x[rv_core->rs1] >= rv_core->x[rv_core->rs2])
    {
        if(ADDR_MISALIGNED(rv_core->jump_offset))
        {
            die_msg("Addr misaligned!\n");
            prepare_sync_trap(rv_core, CSR_MCAUSE_INSTR_ADDR_MISALIGNED);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_ADDI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x %lx\n", __func__, rv_core->instruction, rv_core->pc);
    rv_int_xlen signed_immediate = SIGNEX(rv_core->immediate, 11);
    rv_int_xlen signed_rs_val = rv_core->x[rv_core->rs1];
    CORE_DBG("%s: %lx %lx %lx %x\n", __func__, rv_core->x[rv_core->rs1], signed_rs_val, signed_immediate, rv_core->rs1);
    rv_core->x[rv_core->rd] = (signed_immediate + signed_rs_val);
}

static void instr_SLTI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_immediate = SIGNEX(rv_core->immediate, 11);
    rv_int_xlen signed_rs_val = rv_core->x[rv_core->rs1];

    if(signed_rs_val < signed_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTIU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_uint_xlen unsigned_immediate = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen unsigned_rs_val = rv_core->x[rv_core->rs1];

    if(unsigned_rs_val < unsigned_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_XORI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
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
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | rv_core->immediate;
}

static void instr_ANDI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & rv_core->immediate;
}

static void instr_SLLI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    #ifdef RV64
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x3F));
    #else
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x1F));
    #endif
}

static void instr_SRAI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
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
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    #ifdef RV64
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] >> (rv_core->immediate & 0x3F));
    #else
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] >> (rv_core->immediate & 0x1F));
    #endif
}

static void instr_ADD(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    CORE_DBG("%s: %lx %x\n", __func__, rv_core->x[rv_core->rs1], rv_core->rs1);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] + rv_core->x[rv_core->rs2];
}

static void instr_SUB(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] - rv_core->x[rv_core->rs2];
}

static void instr_SLL(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    #ifdef RV64
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] << (rv_core->x[rv_core->rs2] & 0x3F);
    #else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] << (rv_core->x[rv_core->rs2]);
    #endif
}

static void instr_SLT(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
    rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs < signed_rs2) rv_core->x[rv_core->rd] = 1;
    else rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
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
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ rv_core->x[rv_core->rs2];
}

static void instr_SRL(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    #ifdef RV64
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] >> (rv_core->x[rv_core->rs2] & 0x3F);
    #else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] >> (rv_core->x[rv_core->rs2]);
    #endif
}

static void instr_OR(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | (rv_core->x[rv_core->rs2]);
}

static void instr_AND(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & (rv_core->x[rv_core->rs2]);
}

static void instr_SRA(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];

    #ifdef RV64
        rv_core->x[rv_core->rd] = signed_rs >> (rv_core->x[rv_core->rs2] & 0x3F);
    #else
        rv_core->x[rv_core->rd] = signed_rs >> (rv_core->x[rv_core->rs2]);
    #endif
}

static void instr_LB(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 7);
}

static void instr_LH(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 15);
}

static void instr_LW(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
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
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_LHU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    int err = RV_CORE_E_ERR;
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
    rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_SB(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t value_to_write = (uint8_t)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 1);
}

static void instr_SH(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t value_to_write = (uint16_t)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 2);
}

static void instr_SW(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    rv_uint_xlen value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 4);
}

#ifdef RV64
    static void instr_LWU(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int err = RV_CORE_E_ERR;
        rv_uint_xlen unsigned_offset = SIGNEX(rv_core->immediate, 11);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + unsigned_offset;
        uint32_t tmp_load_val = rv_core->read_mem(rv_core->priv, address, &err);
        rv_core->x[rv_core->rd] = tmp_load_val;
    }

    static void instr_LD(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int err = RV_CORE_E_ERR;
        rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
        rv_int_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
        CORE_DBG("%s: %lx %lx %lx %x\n", __func__, rv_core->x[rv_core->rs1], address, rv_core->immediate, rv_core->rs1);
        rv_core->x[rv_core->rd] = rv_core->read_mem(rv_core->priv, address, &err);
    }

    static void instr_SD(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen signed_offset = SIGNEX(rv_core->immediate, 11);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
        rv_uint_xlen value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
        CORE_DBG("%s: %lx %lx %lx %x\n", __func__, rv_core->x[rv_core->rs1], address, rv_core->immediate, rv_core->rs1);
        rv_core->write_mem(rv_core->priv, address, value_to_write, 8);
    }

    static void instr_SRAIW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int32_t signed_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (signed_rs_val >> (rv_core->immediate & 0x1F));
    }

    static void instr_ADDIW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int32_t signed_immediate = SIGNEX(rv_core->immediate, 11);
        int32_t signed_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (signed_rs_val + signed_immediate);
    }

    static void instr_SLLIW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x1F)) & 0xFFFFFFFF;
        rv_core->x[rv_core->rd] = SIGNEX(rv_core->x[rv_core->rd], 31);
    }

    static void instr_SRLIW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t unsigned_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (unsigned_rs_val >> (rv_core->immediate & 0x1F));
        rv_core->x[rv_core->rd] = SIGNEX(rv_core->x[rv_core->rd], 31);
    }

    static void instr_SRLW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val >> rs2_val, 31);
    }

    static void instr_SRAW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int32_t rs1_val_signed = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val_signed >> rs2_val, 31);
    }

    static void instr_SLLW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val << rs2_val, 31);
    }

    static void instr_ADDW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val + rs2_val, 31);
    }

    static void instr_SUBW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_core->x[rv_core->rd] = SIGNEX(rs1_val - rs2_val, 31);
    }
#endif

#ifdef CSR_SUPPORT
    static void instr_CSRRW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;

        if(csr_read_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
        {
            // die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);
            prepare_sync_trap(rv_core, CSR_MCAUSE_ILLEGAL_INSTR);
            return;
        }

        if(csr_write_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, rv_core->x[rv_core->rs1]))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRS(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        if(csr_read_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
        {
            // die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);
            prepare_sync_trap(rv_core, CSR_MCAUSE_ILLEGAL_INSTR);
            return;
        }

        if(csr_write_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val | rv_core->x[rv_core->rs1]))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRC(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        if(csr_read_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
        {
            // die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);
            prepare_sync_trap(rv_core, CSR_MCAUSE_ILLEGAL_INSTR);
            return;
        }

        if(csr_write_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val & ~rv_core->x[rv_core->rs1]))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRWI(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        if(csr_read_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
        {
            // die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);
            prepare_sync_trap(rv_core, CSR_MCAUSE_ILLEGAL_INSTR);
            return;
        }

        if(csr_write_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, rv_core->rs1))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRSI(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        if(csr_read_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
        {
            // die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);
            prepare_sync_trap(rv_core, CSR_MCAUSE_ILLEGAL_INSTR);
            return;
        }

        if(csr_write_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val | rv_core->rs1))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);

        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_CSRRCI(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        if(csr_read_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, &csr_val))
        {
            // die_msg("Error reading CSR "PRINTF_FMT"\n", rv_core->immediate);
            prepare_sync_trap(rv_core, CSR_MCAUSE_ILLEGAL_INSTR);
            return;
        }

        if(csr_write_reg(rv_core->csr_table, rv_core->curr_priv_mode, rv_core->immediate, csr_val & ~rv_core->rs1))
            DEBUG_PRINT("Error writing CSR - readonly? "PRINTF_FMT"\n", rv_core->immediate);
        
        rv_core->x[rv_core->rd] = csr_val;
    }

    static void instr_ECALL(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        prepare_sync_trap(rv_core, CSR_MCAUSE_ECALL_U + rv_core->curr_priv_mode);
    }

    static void instr_EBREAK(rv_core_td *rv_core)
    {
        /* not implemented */
        (void)rv_core;
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    }

    static void instr_MRET(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_core->next_pc = *rv_core->mepc;

        /* Restore MPP and MIE */
        rv_core->curr_priv_mode = extract32(*rv_core->mstatus, CSR_MSTATUS_MPP_BIT, 2);
        assign_xlen_bit(rv_core->mstatus, CSR_MSTATUS_MIE_BIT, (1 << CSR_MSTATUS_MPIE_BIT) >> CSR_MSTATUS_MPIE_BIT);
    }

    static void instr_SRET(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        /* not implemented */
        (void)rv_core;
    }

    static void instr_URET(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        /* not implemented */
        (void)rv_core;
    }
#endif

#ifdef ATOMIC_SUPPORT
    static void instr_LR_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_core->lr_address = rv_core->x[rv_core->rs1];
        rv_core->lr_valid = 1;
        instr_LW(rv_core);
    }

    static void instr_SC_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        if(rv_core->lr_valid && (rv_core->lr_address == rv_core->x[rv_core->rs1]))
        {
            instr_SW(rv_core);
            rv_core->x[rv_core->rd] = 0;
        }
        else
        {
            rv_core->x[rv_core->rd] = 1;
        }
        
        rv_core->lr_valid = 0;
        rv_core->lr_address = 0;
    }

    static void instr_AMOSWAP_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        uint32_t result = 0;

        instr_LW(rv_core);
        result = rs2_val;

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOADD_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rd_val = 0;
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        uint32_t result = 0;

        instr_LW(rv_core);
        rd_val = rv_core->x[rv_core->rd];
        result = rd_val + rs2_val;

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOXOR_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rd_val = 0;
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        uint32_t result = 0;

        instr_LW(rv_core);
        rd_val = rv_core->x[rv_core->rd];
        result = rd_val ^ rs2_val;

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOAND_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rd_val = 0;
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        uint32_t result = 0;

        instr_LW(rv_core);
        rd_val = rv_core->x[rv_core->rd];
        result = rd_val & rs2_val;

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOOR_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rd_val = 0;
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        uint32_t result = 0;

        instr_LW(rv_core);
        rd_val = rv_core->x[rv_core->rd];
        result = rd_val | rs2_val;

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOMIN_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        int32_t rd_val = 0;
        int32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_uint_xlen result = 0;

        instr_LW(rv_core);

        rd_val = rv_core->x[rv_core->rd];
        result = ASSIGN_MIN(rd_val, rs2_val);

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOMAX_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        int32_t rd_val = 0;
        int32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_uint_xlen result = 0;

        instr_LW(rv_core);

        rd_val = rv_core->x[rv_core->rd];
        result = ASSIGN_MAX(rd_val, rs2_val);

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOMINU_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rd_val = 0;
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_uint_xlen result = 0;

        instr_LW(rv_core);

        rd_val = rv_core->x[rv_core->rd];
        result = ASSIGN_MIN(rd_val, rs2_val);

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    static void instr_AMOMAXU_W(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen address = rv_core->x[rv_core->rs1];
        uint32_t rd_val = 0;
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        rv_uint_xlen result = 0;

        instr_LW(rv_core);

        rd_val = rv_core->x[rv_core->rd];
        result = ASSIGN_MAX(rd_val, rs2_val);

        rv_core->write_mem(rv_core->priv, address, result, 4);
    }

    #ifdef RV64
        static void instr_LR_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_core->lr_valid = 1;
            rv_core->lr_address = rv_core->x[rv_core->rs1];
            instr_LD(rv_core);
        }

        static void instr_SC_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            if(rv_core->lr_valid && (rv_core->lr_address == rv_core->x[rv_core->rs1]))
            {
                instr_SD(rv_core);
                rv_core->x[rv_core->rd] = 0;
            }
            else
            {
                rv_core->x[rv_core->rd] = 1;
            }
            
            rv_core->lr_valid = 0;
            rv_core->lr_address = 0;
        }

        static void instr_AMOSWAP_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);
            result = rs2_val;

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOADD_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rd_val = 0;
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);
            rd_val = rv_core->x[rv_core->rd];
            result = rd_val + rs2_val;

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOXOR_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rd_val = 0;
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);
            rd_val = rv_core->x[rv_core->rd];
            result = rd_val ^ rs2_val;

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOAND_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rd_val = 0;
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);
            rd_val = rv_core->x[rv_core->rd];
            result = rd_val & rs2_val;

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOOR_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rd_val = 0;
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);
            rd_val = rv_core->x[rv_core->rd];
            result = rd_val | rs2_val;

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOMIN_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_int_xlen rd_val = 0;
            rv_int_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);

            rd_val = rv_core->x[rv_core->rd];
            result = ASSIGN_MIN(rd_val, rs2_val);

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOMAX_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_int_xlen rd_val = 0;
            rv_int_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);

            rd_val = rv_core->x[rv_core->rd];
            result = ASSIGN_MAX(rd_val, rs2_val);

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOMINU_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rd_val = 0;
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);

            rd_val = rv_core->x[rv_core->rd];
            result = ASSIGN_MIN(rd_val, rs2_val);

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }

        static void instr_AMOMAXU_D(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            rv_uint_xlen address = rv_core->x[rv_core->rs1];
            rv_uint_xlen rd_val = 0;
            rv_uint_xlen rs2_val = rv_core->x[rv_core->rs2];
            rv_uint_xlen result = 0;

            instr_LD(rv_core);

            rd_val = rv_core->x[rv_core->rd];
            result = ASSIGN_MAX(rd_val, rs2_val);

            rv_core->write_mem(rv_core->priv, address, result, 8);
        }
    #endif
#endif

#ifdef MULTIPLY_SUPPORT
    static void instr_DIV(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
        rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

        /* division by zero */
        if(signed_rs2 == 0)
        {
            rv_core->x[rv_core->rd] = -1;
            return;
        }
        
        /* overflow */
        if(((rv_uint_xlen)signed_rs == XLEN_INT_MIN) && (signed_rs2 == -1))
        {
            rv_core->x[rv_core->rd] = XLEN_INT_MIN;
            return;
        }

        rv_core->x[rv_core->rd] = (signed_rs/signed_rs2);
    }

    static void instr_DIVU(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen unsigned_rs = rv_core->x[rv_core->rs1];
        rv_uint_xlen unsigned_rs2 = rv_core->x[rv_core->rs2];

        /* division by zero */
        if(unsigned_rs2 == 0)
        {
            rv_core->x[rv_core->rd] = -1;
            return;
        }

        rv_core->x[rv_core->rd] = (unsigned_rs/unsigned_rs2);
    }

    static void instr_REM(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
        rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];

        /* division by zero */
        if(signed_rs2 == 0)
        {
            rv_core->x[rv_core->rd] = signed_rs;
            return;
        }

        /* overflow */
        if(((rv_uint_xlen)signed_rs == XLEN_INT_MIN) && (signed_rs2 == -1))
        {
            rv_core->x[rv_core->rd] = 0;
            return;
        }

        rv_core->x[rv_core->rd] = (signed_rs%signed_rs2);
    }

    static void instr_REMU(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen unsigned_rs = rv_core->x[rv_core->rs1];
        rv_uint_xlen unsigned_rs2 = rv_core->x[rv_core->rs2];

        /* division by zero */
        if(unsigned_rs2 == 0)
        {
            rv_core->x[rv_core->rd] = unsigned_rs;
            return;
        }

        rv_core->x[rv_core->rd] = (unsigned_rs%unsigned_rs2);
    }

    static void instr_MUL(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen signed_rs = rv_core->x[rv_core->rs1];
        rv_int_xlen signed_rs2 = rv_core->x[rv_core->rs2];
        rv_core->x[rv_core->rd] = signed_rs * signed_rs2;
    }

    static void instr_MULH(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen result_hi = 0;
        rv_int_xlen result_lo = 0;
        MUL(rv_core->x[rv_core->rs1], rv_core->x[rv_core->rs2], &result_hi, &result_lo);
        rv_core->x[rv_core->rd] = result_hi;
    }

    static void instr_MULHU(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen result_hi = 0;
        rv_uint_xlen result_lo = 0;
        UMUL(rv_core->x[rv_core->rs1], rv_core->x[rv_core->rs2], &result_hi, &result_lo);
        rv_core->x[rv_core->rd] = result_hi;
    }

    static void instr_MULHSU(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen result_hi = 0;
        rv_int_xlen result_lo = 0;
        MULHSU(rv_core->x[rv_core->rs1], rv_core->x[rv_core->rs2], &result_hi, &result_lo);
        rv_core->x[rv_core->rd] = result_hi;
    }

    #ifdef RV64
        static void instr_MULW(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            int32_t signed_rs = rv_core->x[rv_core->rs1];
            int32_t signed_rs2 = rv_core->x[rv_core->rs2];
            int32_t result = signed_rs * signed_rs2;
            rv_core->x[rv_core->rd] = SIGNEX(result, 31);
        }

        static void instr_DIVW(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            int32_t signed_rs = rv_core->x[rv_core->rs1];
            int32_t signed_rs2 = rv_core->x[rv_core->rs2];
            int32_t result = 0;

            /* division by zero */
            if(signed_rs2 == 0)
            {
                rv_core->x[rv_core->rd] = -1;
                return;
            }
            
            /* overflow */
            if((signed_rs == INT32_MIN) && (signed_rs2 == -1))
            {
                rv_core->x[rv_core->rd] = INT32_MIN;
                return;
            }

            result = (signed_rs/signed_rs2);

            rv_core->x[rv_core->rd] = SIGNEX(result, 31);
        }

        static void instr_DIVUW(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            uint32_t unsigned_rs = rv_core->x[rv_core->rs1];
            uint32_t unsigned_rs2 = rv_core->x[rv_core->rs2];
            uint32_t result = 0;

            /* division by zero */
            if(unsigned_rs2 == 0)
            {
                rv_core->x[rv_core->rd] = -1;
                return;
            }

            result = (unsigned_rs/unsigned_rs2);

            rv_core->x[rv_core->rd] = SIGNEX(result, 31);
        }

        static void instr_REMW(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            int32_t signed_rs = rv_core->x[rv_core->rs1];
            int32_t signed_rs2 = rv_core->x[rv_core->rs2];
            int32_t result = 0;

            /* division by zero */
            if(signed_rs2 == 0)
            {
                rv_core->x[rv_core->rd] = SIGNEX(signed_rs, 31);
                return;
            }

            /* overflow */
            if((signed_rs == INT32_MIN) && (signed_rs2 == -1))
            {
                rv_core->x[rv_core->rd] = 0;
                return;
            }

            result = (signed_rs%signed_rs2);

            rv_core->x[rv_core->rd] = SIGNEX(result, 31);
        }

        static void instr_REMUW(rv_core_td *rv_core)
        {
            CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
            uint32_t unsigned_rs = rv_core->x[rv_core->rs1];
            uint32_t unsigned_rs2 = rv_core->x[rv_core->rs2];
            uint32_t result = 0;

            /* division by zero */
            if(unsigned_rs2 == 0)
            {
                rv_core->x[rv_core->rd] = SIGNEX(unsigned_rs, 31);
                return;
            }

            result = (unsigned_rs%unsigned_rs2);

            rv_core->x[rv_core->rd] = SIGNEX(result, 31);
        }
    #endif

#endif

#ifdef ATOMIC_SUPPORT
    static void preparation_func5(rv_core_td *rv_core, int32_t *next_subcode)
    {
        rv_core->func5 = ((rv_core->instruction >> 27) & 0x1F);
        *next_subcode = rv_core->func5;
    }
#endif

#ifdef RV64
    static void preparation_func6(rv_core_td *rv_core, int32_t *next_subcode)
    {
        rv_core->func6 = ((rv_core->instruction >> 26) & 0x3F);
        *next_subcode = rv_core->func6;
    }
#endif

static void preparation_func7(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->func7 = ((rv_core->instruction >> 25) & 0x7F);
    *next_subcode = rv_core->func7;
}

#ifdef CSR_SUPPORT
    static void preparation_func12(rv_core_td *rv_core, int32_t *next_subcode)
    {
        rv_core->func12 = ((rv_core->instruction >> 20) & 0x0FFF);
        *next_subcode = rv_core->func12;
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
                          (extract32(rv_core->instruction, 7, 1) << 11) |
                          (extract32(rv_core->instruction, 31, 1) << 12) );
    rv_core->jump_offset = SIGNEX(rv_core->jump_offset, 12);
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
                          (extract32(rv_core->instruction, 12, 8) << 12) |
                          (extract32(rv_core->instruction, 31, 1) << 20));
    /* sign extend the 20 bit number */
    rv_core->jump_offset = SIGNEX(rv_core->jump_offset, 20);
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

static instruction_hook_td ADD_SUB_MUL_func7_subcode_list[] = {
    [FUNC7_INSTR_ADD] = {NULL, instr_ADD, NULL},
    [FUNC7_INSTR_SUB] = {NULL, instr_SUB, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_MUL] = {NULL, instr_MUL, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_MUL_func7_subcode_list);

static instruction_hook_td SLL_MULH_func7_subcode_list[] = {
    [FUNC7_INSTR_SLL] = {NULL, instr_SLL, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_MUL] = {NULL, instr_MULH, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(SLL_MULH_func7_subcode_list);

static instruction_hook_td SLT_MULHSU_func7_subcode_list[] = {
    [FUNC7_INSTR_SLT] = {NULL, instr_SLT, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_MULHSU] = {NULL, instr_MULHSU, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(SLT_MULHSU_func7_subcode_list);

static instruction_hook_td SLTU_MULHU_func7_subcode_list[] = {
    [FUNC7_INSTR_SLTU] = {NULL, instr_SLTU, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_MULHU] = {NULL, instr_MULHU, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(SLTU_MULHU_func7_subcode_list);

static instruction_hook_td XOR_DIV_func7_subcode_list[] = {
    [FUNC7_INSTR_XOR] = {NULL, instr_XOR, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_DIV] = {NULL, instr_DIV, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(XOR_DIV_func7_subcode_list);

static instruction_hook_td SRL_SRA_DIVU_func7_subcode_list[] = {
    [FUNC7_INSTR_SRL] = {NULL, instr_SRL, NULL},
    [FUNC7_INSTR_SRA] = {NULL, instr_SRA, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_DIVU] = {NULL, instr_DIVU, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(SRL_SRA_DIVU_func7_subcode_list);

static instruction_hook_td OR_REM_func7_subcode_list[] = {
    [FUNC7_INSTR_OR] = {NULL, instr_OR, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_REM] = {NULL, instr_REM, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(OR_REM_func7_subcode_list);

static instruction_hook_td AND_REMU_func7_subcode_list[] = {
    [FUNC7_INSTR_AND] = {NULL, instr_AND, NULL},
    #ifdef MULTIPLY_SUPPORT
        [FUNC7_INSTR_REMU] = {NULL, instr_REMU, NULL},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(AND_REMU_func7_subcode_list);

static instruction_hook_td ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list[] = {
    [FUNC3_INSTR_ADD_SUB_MUL] = {preparation_func7, NULL, &ADD_SUB_MUL_func7_subcode_list_desc},
    [FUNC3_INSTR_SLL_MULH] = {preparation_func7, NULL, &SLL_MULH_func7_subcode_list_desc},
    [FUNC3_INSTR_SLT_MULHSU] = {preparation_func7, NULL, &SLT_MULHSU_func7_subcode_list_desc},
    [FUNC3_INSTR_SLTU_MULHU] = {preparation_func7, NULL, &SLTU_MULHU_func7_subcode_list_desc},
    [FUNC3_INSTR_XOR_DIV] = {preparation_func7, NULL, &XOR_DIV_func7_subcode_list_desc},
    [FUNC3_INSTR_SRL_SRA_DIVU] = {preparation_func7, NULL, &SRL_SRA_DIVU_func7_subcode_list_desc},
    [FUNC3_INSTR_OR_REM] = {preparation_func7, NULL, &OR_REM_func7_subcode_list_desc},
    [FUNC3_INSTR_AND_REMU] = {preparation_func7, NULL, &AND_REMU_func7_subcode_list_desc},
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list);

#ifdef RV64
    static instruction_hook_td SRLIW_SRAIW_func7_subcode_list[] = {
        [FUNC7_INSTR_SRLIW] = {NULL, instr_SRLIW, NULL},
        [FUNC7_INSTR_SRAIW] = {NULL, instr_SRAIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRLIW_SRAIW_func7_subcode_list);

    static instruction_hook_td SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list[] = {
        [FUNC3_INSTR_SLLIW] = {NULL, instr_SLLIW, NULL},
        [FUNC3_INSTR_SRLIW_SRAIW] = {preparation_func7, NULL, &SRLIW_SRAIW_func7_subcode_list_desc},
        [FUNC3_INSTR_ADDIW] = {NULL, instr_ADDIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list);

    static instruction_hook_td SRLW_SRAW_DIVUW_func7_subcode_list[] = {
        [FUNC7_INSTR_SRLW] = {NULL, instr_SRLW, NULL},
        [FUNC7_INSTR_SRAW] = {NULL, instr_SRAW, NULL},
        #ifdef MULTIPLY_SUPPORT
            [FUNC7_INSTR_DIVUW] = {NULL, instr_DIVUW, NULL},
        #endif
    };
    INIT_INSTRUCTION_LIST_DESC(SRLW_SRAW_DIVUW_func7_subcode_list);

    static instruction_hook_td ADDW_SUBW_MULW_func7_subcode_list[] = {
        [FUNC7_INSTR_ADDW] = {NULL, instr_ADDW, NULL},
        [FUNC7_INSTR_SUBW] = {NULL, instr_SUBW, NULL},
        #ifdef MULTIPLY_SUPPORT
            [FUNC7_INSTR_MULW] = {NULL, instr_MULW, NULL},
        #endif
    };
    INIT_INSTRUCTION_LIST_DESC(ADDW_SUBW_MULW_func7_subcode_list);

    static instruction_hook_td ADDW_SUBW_SLLW_SRLW_SRAW_MULW_DIVW_DIVUW_REMW_REMUW_func3_subcode_list[] = {
        [FUNC3_INSTR_ADDW_SUBW_MULW] = {preparation_func7, NULL, &ADDW_SUBW_MULW_func7_subcode_list_desc},
        [FUNC3_INSTR_SLLW] = {NULL, instr_SLLW, NULL},
        [FUNC3_INSTR_SRLW_SRAW_DIVUW] = {preparation_func7, NULL, &SRLW_SRAW_DIVUW_func7_subcode_list_desc},
        #ifdef MULTIPLY_SUPPORT
            [FUNC3_INSTR_DIVW] = {NULL, instr_DIVW, NULL},
            [FUNC3_INSTR_REMW] = {NULL, instr_REMW, NULL},
            [FUNC3_INSTR_REMUW] = {NULL, instr_REMUW, NULL},
        #endif
    };
    INIT_INSTRUCTION_LIST_DESC(ADDW_SUBW_SLLW_SRLW_SRAW_MULW_DIVW_DIVUW_REMW_REMUW_func3_subcode_list);
#endif

#ifdef CSR_SUPPORT
    static instruction_hook_td ECALL_EBREAK_MRET_SRET_URET_func12_subcode_list[] = {
        [FUNC12_ECALL] = {NULL, instr_ECALL, NULL},
        [FUNC12_EBREAK] = {NULL, instr_EBREAK, NULL},
        [FUNC12_INSTR_MRET] = {NULL, instr_MRET, NULL},
        [FUNC12_INSTR_SRET] = {NULL, instr_SRET, NULL},
        [FUNC12_INSTR_URET] = {NULL, instr_URET, NULL},
        [FUNC12_INSTR_WFI] = {NULL, instr_NOP, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(ECALL_EBREAK_MRET_SRET_URET_func12_subcode_list);

    static instruction_hook_td ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_func3_subcode_list[] = {
        [FUNC3_INSTR_CSRRW] = {NULL, instr_CSRRW, NULL},
        [FUNC3_INSTR_CSRRS] = {NULL, instr_CSRRS, NULL},
        [FUNC3_INSTR_CSRRC] = {NULL, instr_CSRRC, NULL},
        [FUNC3_INSTR_CSRRWI] = {NULL, instr_CSRRWI, NULL},
        [FUNC3_INSTR_CSRRSI] = {NULL, instr_CSRRSI, NULL},
        [FUNC3_INSTR_CSRRCI] = {NULL, instr_CSRRCI, NULL},
        [FUNC3_INSTR_ECALL_EBREAK_MRET_SRET_URET_WFI] = {preparation_func12, NULL, &ECALL_EBREAK_MRET_SRET_URET_func12_subcode_list_desc}
    };
    INIT_INSTRUCTION_LIST_DESC(ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_func3_subcode_list);
#endif

#ifdef ATOMIC_SUPPORT
    static instruction_hook_td W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func5_subcode_list[] = {
        [FUNC5_INSTR_AMO_LR] = {NULL, instr_LR_W, NULL},
        [FUNC5_INSTR_AMO_SC] = {NULL, instr_SC_W, NULL},
        [FUNC5_INSTR_AMO_SWAP] = {NULL, instr_AMOSWAP_W, NULL},
        [FUNC5_INSTR_AMO_ADD] = {NULL, instr_AMOADD_W, NULL},
        [FUNC5_INSTR_AMO_XOR] = {NULL, instr_AMOXOR_W, NULL},
        [FUNC5_INSTR_AMO_AND] = {NULL, instr_AMOAND_W, NULL},
        [FUNC5_INSTR_AMO_OR] = {NULL, instr_AMOOR_W, NULL},
        [FUNC5_INSTR_AMO_MIN] = {NULL, instr_AMOMIN_W, NULL},
        [FUNC5_INSTR_AMO_MAX] = {NULL, instr_AMOMAX_W, NULL},
        [FUNC5_INSTR_AMO_MINU] = {NULL, instr_AMOMINU_W, NULL},
        [FUNC5_INSTR_AMO_MAXU] = {NULL, instr_AMOMAXU_W, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func5_subcode_list);

    #ifdef RV64
        static instruction_hook_td D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func5_subcode_list[] = {
            [FUNC5_INSTR_AMO_LR] = {NULL, instr_LR_D, NULL},
            [FUNC5_INSTR_AMO_SC] = {NULL, instr_SC_D, NULL},
            [FUNC5_INSTR_AMO_SWAP] = {NULL, instr_AMOSWAP_D, NULL},
            [FUNC5_INSTR_AMO_ADD] = {NULL, instr_AMOADD_D, NULL},
            [FUNC5_INSTR_AMO_XOR] = {NULL, instr_AMOXOR_D, NULL},
            [FUNC5_INSTR_AMO_AND] = {NULL, instr_AMOAND_D, NULL},
            [FUNC5_INSTR_AMO_OR] = {NULL, instr_AMOOR_D, NULL},
            [FUNC5_INSTR_AMO_MIN] = {NULL, instr_AMOMIN_D, NULL},
            [FUNC5_INSTR_AMO_MAX] = {NULL, instr_AMOMAX_D, NULL},
            [FUNC5_INSTR_AMO_MINU] = {NULL, instr_AMOMINU_D, NULL},
            [FUNC5_INSTR_AMO_MAXU] = {NULL, instr_AMOMAXU_D, NULL},
        };
        INIT_INSTRUCTION_LIST_DESC(D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func5_subcode_list);
    #endif

    static instruction_hook_td W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func3_subcode_list[] = {
        [FUNC3_INSTR_W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU] = {preparation_func5, NULL, &W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func5_subcode_list_desc},
        #ifdef RV64
            [FUNC3_INSTR_D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU] = {preparation_func5, NULL, &D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func5_subcode_list_desc},
        #endif
    };
    INIT_INSTRUCTION_LIST_DESC(W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func3_subcode_list);
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
    [INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_MUL_MULH_MULHSU_MULHU_DIV_DIVU_REM_REMU] = {R_type_preparation, NULL, &ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list_desc},
    [INSTR_FENCE_FENCE_I] = {NULL, instr_NOP, NULL}, /* Not implemented */

    #ifdef RV64
        [INSTR_ADDIW_SLLIW_SRLIW_SRAIW] = {I_type_preparation, NULL, &SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list_desc},
        [INSTR_ADDW_SUBW_SLLW_SRLW_SRAW_MULW_DIVW_DIVUW_REMW_REMUW] = {R_type_preparation, NULL, &ADDW_SUBW_SLLW_SRLW_SRAW_MULW_DIVW_DIVUW_REMW_REMUW_func3_subcode_list_desc},
    #endif

    #ifdef CSR_SUPPORT
        [INSTR_ECALL_EBREAK_MRET_SRET_URET_WFI_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI] = {I_type_preparation, NULL, &ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_func3_subcode_list_desc},
    #endif

    #ifdef ATOMIC_SUPPORT
        [INSTR_AMO_W_D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU] = {R_type_preparation, NULL, &W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU_func3_subcode_list_desc},
    #endif
};
INIT_INSTRUCTION_LIST_DESC(RV_opcode_list);

static void rv_call_from_opcode_list(rv_core_td *rv_core, instruction_desc_td *opcode_list_desc, uint32_t opcode)
{
    int32_t next_subcode = -1;

    unsigned int list_size = opcode_list_desc->instruction_hook_list_size;
    instruction_hook_td *opcode_list = opcode_list_desc->instruction_hook_list;

    if( (opcode_list[opcode].preparation_cb == NULL) && 
        (opcode_list[opcode].execution_cb == NULL) && 
        (opcode_list[opcode].next == NULL) )
        die_msg("Unknown instruction: %08x PC: "PRINTF_FMT" Cycle: %016ld\n", rv_core->instruction, rv_core->pc, rv_core->curr_cycle);

    if(opcode >= list_size) 
        die_msg("Unknown instruction: %08x PC: "PRINTF_FMT" Cycle: %016ld\n", rv_core->instruction, rv_core->pc, rv_core->curr_cycle);

    if(opcode_list[opcode].preparation_cb != NULL)
        opcode_list[opcode].preparation_cb(rv_core, &next_subcode);

    if(opcode_list[opcode].execution_cb != NULL)
        rv_core->execute_cb = opcode_list[opcode].execution_cb;

    if((next_subcode != -1) && (opcode_list[opcode].next != NULL))
        rv_call_from_opcode_list(rv_core, opcode_list[opcode].next, next_subcode);
}

#ifdef CSR_SUPPORT
    static inline void rv_core_do_irq(rv_core_td *rv_core, rv_uint_xlen mepc, rv_uint_xlen mcause)
    {
        *rv_core->mcause = mcause;
        *rv_core->mepc = mepc;
        rv_core->pc = *rv_core->mtvec;

        /* Save MPP and MIE */
        *rv_core->mstatus |= rv_core->curr_priv_mode << CSR_MSTATUS_MPP_BIT;
        assign_xlen_bit(rv_core->mstatus, CSR_MSTATUS_MPIE_BIT, (*rv_core->mstatus >> CSR_MSTATUS_MIE_BIT) & CSR_MSTATUS_MIE_MASK);

        /* now clear MIE */
        CLEAR_BIT(*rv_core->mstatus, CSR_MSTATUS_MIE_BIT);

        /* Elevate privilege mode */
        rv_core->curr_priv_mode = machine_mode;
    }

    static inline void rv_core_update_interrupts(rv_core_td *rv_core, uint8_t mei, uint8_t msi, uint8_t mti)
    {
        /* map msip register to the CSR */
        if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MEI_BIT))
            assign_xlen_bit(rv_core->mip, CSR_MIE_MIP_MEI_BIT, mei);

        /* map msip register to the CSR */
        if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MSI_BIT))
            assign_xlen_bit(rv_core->mip, CSR_MIE_MIP_MSI_BIT, msi);

        /* map timer interrupt */
        if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MTI_BIT))
            assign_xlen_bit(rv_core->mip, CSR_MIE_MIP_MTI_BIT, mti);
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
            /* check if MEI interrupt is enabled */
            if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MEI_BIT))
            {
                /* check if interrupt pending */
                if(CHECK_BIT(*rv_core->mip, CSR_MIE_MIP_MEI_BIT))
                {
                    rv_core_do_irq(rv_core, rv_core->pc, (1UL<<(XLEN-1)) | CSR_MCAUSE_MEI);
                    return 1;
                }
            }

            /* check if MSI interrupt is enabled */
            if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MSI_BIT))
            {
                /* check if interrupt pending */
                if(CHECK_BIT(*rv_core->mip, CSR_MIE_MIP_MSI_BIT))
                {
                    rv_core_do_irq(rv_core, rv_core->pc, (1UL<<(XLEN-1)) | CSR_MCAUSE_MSI);
                    return 1;
                }
            }

            /* Timer interrupt */
            if(CHECK_BIT(*rv_core->mie, CSR_MIE_MIP_MTI_BIT))
            {
                /* check if interrupt pending */
                if(CHECK_BIT(*rv_core->mip, CSR_MIE_MIP_MTI_BIT))
                {
                    rv_core_do_irq(rv_core, rv_core->pc, (1UL<<(XLEN-1)) | CSR_MCAUSE_MTI);
                    return 1;
                }
            }
        }

        if(rv_core->sync_trap_pending)
        {
            rv_core_do_irq(rv_core, rv_core->pc - 4, rv_core->sync_trap_cause);
            rv_core->sync_trap_pending = 0;
            rv_core->sync_trap_cause = 0;
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

    rv_core->curr_cycle++;
}

void rv_core_process_interrupts(rv_core_td *rv_core, uint8_t mei, uint8_t msi, uint8_t mti)
{
    #ifdef CSR_SUPPORT
        /* interrupt handling */
        rv_core_update_interrupts(rv_core, mei, msi, mti);
        rv_core_prepare_interrupts(rv_core);
    #else
        (void)rv_core;
        (void)mei;
        (void)msi;
        (void)mti;
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

void rv_core_reg_dump_more_regs(rv_core_td *rv_core)
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
    rv_core->pc = MROM_BASE_ADDR;

    rv_core->priv = priv;
    rv_core->read_mem = read_mem;
    rv_core->write_mem = write_mem;

    rv_core->csr_table = csr_table;

    #ifdef CSR_SUPPORT
        /* set fast access pointers */
        rv_core->mstatus = csr_get_reg_reference(csr_table, CSR_ADDR_MSTATUS);
        rv_core->mcause = csr_get_reg_reference(csr_table, CSR_ADDR_MCAUSE);
        rv_core->mepc = csr_get_reg_reference(csr_table, CSR_ADDR_MEPC);
        rv_core->mtvec = csr_get_reg_reference(csr_table, CSR_ADDR_MTVEC);
        rv_core->mie = csr_get_reg_reference(csr_table, CSR_ADDR_MIE);
        rv_core->mip = csr_get_reg_reference(csr_table, CSR_ADDR_MIP);
    #endif
}
