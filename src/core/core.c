#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_types.h>
#include <riscv_helper.h>
#include <riscv_instr.h>

#include <core.h>

// #define CORE_DEBUG
#ifdef CORE_DEBUG
    #define CORE_DBG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
    #define CORE_DBG(...) do{ } while ( 0 )
#endif

#define ADDR_MISALIGNED(addr) (addr & 0x3)

/*
 * Functions for internal use
 */
static inline void prepare_sync_trap(rv_core_td *rv_core, rv_uint_xlen cause, rv_uint_xlen tval)
{
    if(!rv_core->sync_trap_pending)
    {
        rv_core->sync_trap_pending = 1;
        rv_core->sync_trap_cause = cause;
        rv_core->sync_trap_tval = tval;
    }
}

static inline privilege_level check_mprv_override(rv_core_td *rv_core, bus_access_type access_type)
{
    if(access_type == bus_instr_access)
        return rv_core->curr_priv_mode;

    int mprv = extractxlen(*rv_core->trap.m.regs[trap_reg_status], TRAP_XSTATUS_MPRV_BIT, 1);
    privilege_level ret_val = extractxlen(*rv_core->trap.m.regs[trap_reg_status], TRAP_XSTATUS_MPP_BIT, 2);
    return mprv ? ret_val : rv_core->curr_priv_mode;
}

rv_ret pmp_checked_bus_access(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen addr, void *value, uint8_t len)
{
    (void) priv_level;
    rv_core_td *rv_core = priv;

    rv_uint_xlen trap_cause = (access_type == bus_instr_access) ? trap_cause_instr_access_fault :
                              (access_type == bus_read_access) ? trap_cause_load_access_fault :
                              trap_cause_store_amo_access_fault;

    if(pmp_mem_check(&rv_core->pmp, priv_level, addr, len, access_type))
    {
        printf("PMP Violation!\n");
        prepare_sync_trap(rv_core, trap_cause, addr);
        return rv_err;
    }

    return rv_core->bus_access(rv_core->priv, priv_level, access_type, addr, value, len);
}

rv_ret mmu_checked_bus_access(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen addr, void *value, uint8_t len)
{
    (void) priv_level;
    rv_core_td *rv_core = priv;
    privilege_level internal_priv_level = check_mprv_override(rv_core, access_type);
    rv_uint_xlen trap_cause = (access_type == bus_instr_access) ? trap_cause_instr_page_fault :
                              (access_type == bus_read_access) ? trap_cause_load_page_fault :
                              trap_cause_store_amo_page_fault;
    mmu_ret mmu_ret_val = mmu_ok;

    uint8_t mxr = CHECK_BIT(*rv_core->trap.m.regs[trap_reg_status], TRAP_XSTATUS_MXR_BIT) ? 1 : 0;
    uint8_t sum = CHECK_BIT(*rv_core->trap.m.regs[trap_reg_status], TRAP_XSTATUS_SUM_BIT) ? 1 : 0;

    rv_uint_xlen tmp = 0;
    memcpy(&tmp, value, len);
    uint64_t phys_addr = mmu_virt_to_phys(&rv_core->mmu, internal_priv_level, addr, access_type, mxr, sum, &mmu_ret_val, rv_core, tmp);

    if(mmu_ret_val != mmu_ok)
    {
        prepare_sync_trap(rv_core, trap_cause, addr);
        return rv_err;
    }

    return rv_core->mmu.bus_access(rv_core->mmu.priv, internal_priv_level, access_type, phys_addr, value, len);
}

/*
 * Implementations of the RISCV instructions
 */
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
        prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
        return;
    }

    rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
}

static void instr_JALR(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_uint_xlen curr_pc = rv_core->pc + 4;
    rv_core->jump_offset = SIGNEX_BIT_11(rv_core->immediate);

    rv_core->next_pc = (rv_core->x[rv_core->rs1] + rv_core->jump_offset);
    rv_core->next_pc &= ~(1<<0);

    if(ADDR_MISALIGNED(rv_core->next_pc))
    {
        die_msg("Addr misaligned!\n");
        prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
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
            prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
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
            prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
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
            prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
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
            prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
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
            prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
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
            prepare_sync_trap(rv_core, trap_cause_instr_addr_misalign, 0);
            return;
        }

        rv_core->next_pc = rv_core->pc + rv_core->jump_offset;
    }
}

static void instr_ADDI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x "PRINTF_FMT"\n", __func__, rv_core->instruction, rv_core->pc);
    rv_int_xlen signed_immediate = SIGNEX_BIT_11(rv_core->immediate);
    rv_int_xlen signed_rs_val = rv_core->x[rv_core->rs1];
    CORE_DBG("%s: "PRINTF_FMT" "PRINTF_FMT" "PRINTF_FMT" %x\n", __func__, rv_core->x[rv_core->rs1], signed_rs_val, signed_immediate, rv_core->rs1);
    rv_core->x[rv_core->rd] = (signed_immediate + signed_rs_val);
}

static void instr_SLTI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_immediate = SIGNEX_BIT_11(rv_core->immediate);
    rv_int_xlen signed_rs_val = rv_core->x[rv_core->rs1];

    if(signed_rs_val < signed_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTIU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_uint_xlen unsigned_immediate = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen unsigned_rs_val = rv_core->x[rv_core->rs1];

    if(unsigned_rs_val < unsigned_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_XORI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_immediate = SIGNEX_BIT_11(rv_core->immediate);
    rv_core->immediate = signed_immediate;

    if(signed_immediate == -1)
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ -1;
    else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ rv_core->immediate;
}

static void instr_ORI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->immediate = SIGNEX_BIT_11(rv_core->immediate);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | rv_core->immediate;
}

static void instr_ANDI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->immediate = SIGNEX_BIT_11(rv_core->immediate);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & rv_core->immediate;
}

static void instr_SLLI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << (rv_core->immediate & SHIFT_OP_MASK));
}

static void instr_SRAI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen rs_val = rv_core->x[rv_core->rs1];

    /* a right shift on signed ints seem to be always arithmetic */
    rs_val = rs_val >> (rv_core->immediate & SHIFT_OP_MASK);
    rv_core->x[rv_core->rd] = rs_val;
}

static void instr_SRLI(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] >> (rv_core->immediate & SHIFT_OP_MASK));
}

static void instr_ADD(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    CORE_DBG("%s: "PRINTF_FMT" %x\n", __func__, rv_core->x[rv_core->rs1], rv_core->rs1);
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
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] << (rv_core->x[rv_core->rs2] & SHIFT_OP_MASK);
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
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] >> (rv_core->x[rv_core->rs2] & SHIFT_OP_MASK);
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
    rv_core->x[rv_core->rd] = signed_rs >> (rv_core->x[rv_core->rs2] & SHIFT_OP_MASK);
}

static void instr_LB(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    uint8_t tmp_load_val = 0;
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 1) == rv_ok)
        rv_core->x[rv_core->rd] = SIGNEX_BIT_7(tmp_load_val);
}

static void instr_LH(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    uint16_t tmp_load_val = 0;
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 2) == rv_ok)
        rv_core->x[rv_core->rd] = SIGNEX_BIT_15(tmp_load_val);
}

static void instr_LW(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    int32_t tmp_load_val = 0;
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 4) == rv_ok)
        rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_LBU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    uint8_t tmp_load_val = 0;
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 1) == rv_ok)
        rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_LHU(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    uint16_t tmp_load_val = 0;
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 2) == rv_ok)
        rv_core->x[rv_core->rd] = tmp_load_val;
}

static void instr_SB(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint8_t value_to_write = (uint8_t)rv_core->x[rv_core->rs2];
    mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &value_to_write, 1);
}

static void instr_SH(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    uint16_t value_to_write = (uint16_t)rv_core->x[rv_core->rs2];
    mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &value_to_write, 2);
}

static void instr_SW(rv_core_td *rv_core)
{
    CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
    rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
    rv_uint_xlen value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
    mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &value_to_write, 4);
}

#ifdef RV64
    static void instr_LWU(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t tmp_load_val = 0;
        rv_uint_xlen unsigned_offset = SIGNEX_BIT_11(rv_core->immediate);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + unsigned_offset;
        if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 4) == rv_ok)
            rv_core->x[rv_core->rd] = tmp_load_val;
    }

    static void instr_LD(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen tmp_load_val = 0;
        rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
        rv_int_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
        if(mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_read_access, address, &tmp_load_val, 8) == rv_ok)
            rv_core->x[rv_core->rd] = tmp_load_val;
    }

    static void instr_SD(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_int_xlen signed_offset = SIGNEX_BIT_11(rv_core->immediate);
        rv_uint_xlen address = rv_core->x[rv_core->rs1] + signed_offset;
        rv_uint_xlen value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &value_to_write, 8);
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
        int32_t signed_immediate = SIGNEX_BIT_11(rv_core->immediate);
        int32_t signed_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (signed_rs_val + signed_immediate);
    }

    static void instr_SLLIW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int32_t signed_tmp32 = (rv_core->x[rv_core->rs1] << (rv_core->immediate & 0x1F)) & 0xFFFFFFFF;
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }

    static void instr_SRLIW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t unsigned_rs_val = rv_core->x[rv_core->rs1];
        int32_t signed_tmp32 = (unsigned_rs_val >> (rv_core->immediate & 0x1F));
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }

    static void instr_SRLW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        int32_t signed_tmp32 = rs1_val >> rs2_val;
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }

    static void instr_SRAW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        int32_t rs1_val_signed = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        int32_t signed_tmp32 = rs1_val_signed >> rs2_val;
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }

    static void instr_SLLW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = (rv_core->x[rv_core->rs2] & 0x1F);
        int32_t signed_tmp32 = rs1_val << rs2_val;
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }

    static void instr_ADDW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        int32_t signed_tmp32 = rs1_val + rs2_val;
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }

    static void instr_SUBW(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        uint32_t rs1_val = rv_core->x[rv_core->rs1];
        uint32_t rs2_val = rv_core->x[rv_core->rs2];
        int32_t signed_tmp32 = rs1_val - rs2_val;
        rv_core->x[rv_core->rd] = (rv_int_xlen)signed_tmp32;
    }
#endif

#ifdef CSR_SUPPORT
    static inline void CSRRWx(rv_core_td *rv_core, rv_uint_xlen new_val)
    {
        CORE_DBG("%s: %x "PRINTF_FMT" priv level: %d\n", __func__, rv_core->instruction, rv_core->pc, rv_core->curr_priv_mode);
        rv_uint_xlen csr_val = 0;
        uint16_t csr_addr = rv_core->immediate;
        rv_uint_xlen csr_mask = csr_get_mask(rv_core->csr_regs, csr_addr);
        rv_uint_xlen not_allowed_bits = 0;
        rv_uint_xlen new_csr_val = 0;

        if(rv_core->rd != 0)
        {
            if(csr_read_reg(rv_core->csr_regs, rv_core->curr_priv_mode, csr_addr, &csr_val))
            {
                // die_msg("Error reading CSR %x "PRINTF_FMT"\n", csr_addr, rv_core->pc);
                prepare_sync_trap(rv_core, trap_cause_illegal_instr, 0);
                return;
            }
        }

        not_allowed_bits = csr_val & ~csr_mask;
        new_csr_val = not_allowed_bits | (new_val & csr_mask);

        if(csr_write_reg(rv_core->csr_regs, rv_core->curr_priv_mode, csr_addr, new_csr_val))
        {
            // die_msg("Error reading CSR %x "PRINTF_FMT"\n", csr_addr, rv_core->pc);
            prepare_sync_trap(rv_core, trap_cause_illegal_instr, 0);
            return;
        }

        rv_core->x[rv_core->rd] = csr_val & csr_mask;
    }

    static inline void CSRRSx(rv_core_td *rv_core, rv_uint_xlen new_val)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        uint16_t csr_addr = rv_core->immediate;
        rv_uint_xlen csr_mask = csr_get_mask(rv_core->csr_regs, csr_addr);
        rv_uint_xlen new_csr_val = 0;

        if(csr_read_reg(rv_core->csr_regs, rv_core->curr_priv_mode, csr_addr, &csr_val))
        {
            // die_msg("Error reading CSR %x "PRINTF_FMT"\n", csr_addr, rv_core->pc);
            prepare_sync_trap(rv_core, trap_cause_illegal_instr, 0);
            return;
        }

        new_csr_val = (new_val & csr_mask);

        if(rv_core->rs1 != 0)
        {
            if(csr_write_reg(rv_core->csr_regs, rv_core->curr_priv_mode, csr_addr, csr_val | new_csr_val))
            {
                // die_msg("Error reading CSR %x "PRINTF_FMT"\n", csr_addr, rv_core->pc);
                prepare_sync_trap(rv_core, trap_cause_illegal_instr, 0);
                return;
            }
        }

        rv_core->x[rv_core->rd] = csr_val & csr_mask;
    }

    static inline void CSRRCx(rv_core_td *rv_core, rv_uint_xlen new_val)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        rv_uint_xlen csr_val = 0;
        uint16_t csr_addr = rv_core->immediate;
        rv_uint_xlen csr_mask = csr_get_mask(rv_core->csr_regs, csr_addr);
        rv_uint_xlen new_csr_val = 0;

        if(csr_read_reg(rv_core->csr_regs, rv_core->curr_priv_mode, csr_addr, &csr_val))
        {
            // die_msg("Error reading CSR %x "PRINTF_FMT"\n", csr_addr, rv_core->pc);
            prepare_sync_trap(rv_core, trap_cause_illegal_instr, 0);
            return;
        }

        new_csr_val = (new_val & csr_mask);

        if(rv_core->rs1 != 0)
        {
            if(csr_write_reg(rv_core->csr_regs, rv_core->curr_priv_mode, csr_addr, csr_val & ~new_csr_val))
            {
                // die_msg("Error reading CSR %x "PRINTF_FMT"\n", csr_addr, rv_core->pc);
                prepare_sync_trap(rv_core, trap_cause_illegal_instr, 0);
                return;
            }
        }
        rv_core->x[rv_core->rd] = csr_val & csr_mask;
    }

    static void instr_CSRRW(rv_core_td *rv_core)
    {
        CSRRWx(rv_core, rv_core->x[rv_core->rs1]);
    }

    static void instr_CSRRS(rv_core_td *rv_core)
    {
        CSRRSx(rv_core, rv_core->x[rv_core->rs1]);
    }

    static void instr_CSRRC(rv_core_td *rv_core)
    {
        CSRRCx(rv_core, rv_core->x[rv_core->rs1]);
    }

    static void instr_CSRRWI(rv_core_td *rv_core)
    {
        CSRRWx(rv_core, rv_core->rs1);
    }

    static void instr_CSRRSI(rv_core_td *rv_core)
    {
        CSRRSx(rv_core, rv_core->rs1);
    }

    static void instr_CSRRCI(rv_core_td *rv_core)
    {
        CSRRCx(rv_core, rv_core->rs1);
    }

    static void instr_ECALL(rv_core_td *rv_core)
    {
        // printf("%s: %x from: %d\n", __func__, rv_core->instruction, trap_cause_user_ecall + rv_core->curr_priv_mode);
        prepare_sync_trap(rv_core, trap_cause_user_ecall + rv_core->curr_priv_mode, 0);
    }

    static void instr_EBREAK(rv_core_td *rv_core)
    {
        /* not implemented */
        (void)rv_core;
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
    }

    static void instr_MRET(rv_core_td *rv_core)
    {
        CORE_DBG("%s: "PRINTF_FMT"\n", __func__, *rv_core->trap.m.regs[trap_reg_ip]);
        privilege_level restored_priv_level = trap_restore_irq_settings(&rv_core->trap, rv_core->curr_priv_mode);
        rv_core->curr_priv_mode = restored_priv_level;
        rv_core->next_pc = *rv_core->trap.m.regs[trap_reg_epc];
    }

    static void instr_SRET(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        privilege_level restored_priv_level = trap_restore_irq_settings(&rv_core->trap, rv_core->curr_priv_mode);
        rv_core->curr_priv_mode = restored_priv_level;
        rv_core->next_pc = *rv_core->trap.s.regs[trap_reg_epc];
    }

    static void instr_URET(rv_core_td *rv_core)
    {
        CORE_DBG("%s: %x\n", __func__, rv_core->instruction);
        printf("URET!\n");
        while(1);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

        mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 4);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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

            mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_write_access, address, &result, 8);
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
            rv_core->x[rv_core->rd] = (rv_int_xlen)(signed_rs * signed_rs2);
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

            rv_core->x[rv_core->rd] = (rv_int_xlen)result;
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

            rv_core->x[rv_core->rd] = SIGNEX_BIT_31(result);
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
                rv_core->x[rv_core->rd] = (rv_int_xlen)signed_rs;
                return;
            }

            /* overflow */
            if((signed_rs == INT32_MIN) && (signed_rs2 == -1))
            {
                rv_core->x[rv_core->rd] = 0;
                return;
            }

            result = (signed_rs%signed_rs2);

            rv_core->x[rv_core->rd] = (rv_int_xlen)result;
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
                rv_core->x[rv_core->rd] = SIGNEX_BIT_31(unsigned_rs);
                return;
            }

            result = (unsigned_rs%unsigned_rs2);

            rv_core->x[rv_core->rd] = SIGNEX_BIT_31(result);
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

static void preparation_func7_func12_sub5_extended(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->func5 = ((rv_core->instruction >> 20) & 0x1F);
    *next_subcode = rv_core->func5;
}

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
    rv_core->jump_offset = SIGNEX_BIT_12(rv_core->jump_offset);
    *next_subcode = rv_core->func3;
}

static void U_type_preparation(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->immediate = ((rv_core->instruction >> 12) & 0xFFFFF);
    rv_core->immediate = SIGNEX_BIT_19(rv_core->immediate);
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
    rv_core->jump_offset = SIGNEX_BIT_20(rv_core->jump_offset);
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
    static instruction_hook_td ECALL_EBREAK_URET_func12_sub5_subcode_list[] = {
        [FUNC5_INSTR_ECALL] = {NULL, instr_ECALL, NULL},
        [FUNC5_INSTR_EBREAK] = {NULL, instr_EBREAK, NULL},
        [FUNC5_INSTR_URET] = {NULL, instr_URET, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(ECALL_EBREAK_URET_func12_sub5_subcode_list);

    static instruction_hook_td SRET_WFI_func12_sub5_subcode_list[] = {
        [FUNC5_INSTR_SRET] = {NULL, instr_SRET, NULL},
        [FUNC5_INSTR_WFI] = {NULL, instr_NOP, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRET_WFI_func12_sub5_subcode_list);

    static instruction_hook_td ECALL_EBREAK_URET_SRET_MRET_WFI_SFENCEVMA_func7_subcode_list[] = {
        [FUNC7_INSTR_ECALL_EBREAK_URET] = {preparation_func7_func12_sub5_extended, NULL, &ECALL_EBREAK_URET_func12_sub5_subcode_list_desc},
        [FUNC7_INSTR_SRET_WFI] = {preparation_func7_func12_sub5_extended, NULL, &SRET_WFI_func12_sub5_subcode_list_desc},
        [FUNC7_INSTR_MRET] = {NULL, instr_MRET, NULL},
        [FUNC7_INSTR_SFENCEVMA] = {NULL, instr_NOP, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(ECALL_EBREAK_URET_SRET_MRET_WFI_SFENCEVMA_func7_subcode_list);

    static instruction_hook_td CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_ECALL_EBREAK_URET_SRET_MRET_WFI_SFENCEVMA_func3_subcode_list[] = {
        [FUNC3_INSTR_CSRRW] = {NULL, instr_CSRRW, NULL},
        [FUNC3_INSTR_CSRRS] = {NULL, instr_CSRRS, NULL},
        [FUNC3_INSTR_CSRRC] = {NULL, instr_CSRRC, NULL},
        [FUNC3_INSTR_CSRRWI] = {NULL, instr_CSRRWI, NULL},
        [FUNC3_INSTR_CSRRSI] = {NULL, instr_CSRRSI, NULL},
        [FUNC3_INSTR_CSRRCI] = {NULL, instr_CSRRCI, NULL},
        [FUNC3_INSTR_ECALL_EBREAK_MRET_SRET_URET_WFI_SFENCEVMA] = {preparation_func7, NULL, &ECALL_EBREAK_URET_SRET_MRET_WFI_SFENCEVMA_func7_subcode_list_desc}
    };
    INIT_INSTRUCTION_LIST_DESC(CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_ECALL_EBREAK_URET_SRET_MRET_WFI_SFENCEVMA_func3_subcode_list);
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
        [INSTR_ECALL_EBREAK_MRET_SRET_URET_WFI_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_SFENCEVMA] = {I_type_preparation, NULL, &CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI_ECALL_EBREAK_URET_SRET_MRET_WFI_SFENCEVMA_func3_subcode_list_desc},
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
    static inline void rv_core_update_interrupts(rv_core_td *rv_core, uint8_t mei, uint8_t mti, uint8_t msi)
    {
        trap_set_pending_bits(&rv_core->trap, mei, mti, msi);
    }

    static inline uint8_t rv_core_prepare_interrupts(rv_core_td *rv_core)
    {
        trap_cause_interrupt interrupt_cause = 0;
        trap_ret trap_retval = 0;
        privilege_level serving_priv_level = machine_mode;

        /* Privilege Spec: "Multiple simultaneous interrupts and traps at the same privilege level are handled in the following
         * decreasing priority order: external interrupts, software interrupts, timer interrupts, then finally any
         * synchronous traps."
         *
         * NOTE: We actually don't use this priority order here. The problem is, that if an interrupt and a synchronous
         * trap is active at the same cycle the interrupt will be handled first and in the next cycle immediately the synchronous trap
         * (as they will also occur when interrupts are globally disabled) which will potentially disrupt the privilige level at which
         * the synchronous trap should actually be handled. (This issue actually happenend when a user ecall and a timer IRQ occured at the same cycle).
         * So we use another prio order to circumvent this:
         * We handle synchronous traps at the highest prio. (Interrupts will be disabled during the handling, regardless if it is a sync trap or a IRQ)
         * This ensures atomicity among sync traps and Interrupts.
         *
         * Possible solution for this: we could remember pending sync traps for each priv level separately. And handle them only if we are in the
         * appropriate priv level. Anyway for now we just keep it simple like this, as except that we don't follow the spec here 100% it works just fine.
         * Furthermore simultanious interrupts at the same cycle should be very rare anyway.
         */
        if(rv_core->sync_trap_pending)
        {
            serving_priv_level = trap_check_exception_delegation(&rv_core->trap, rv_core->curr_priv_mode, rv_core->sync_trap_cause);

            // printf("exception! serving priv: %d cause %d edeleg %x curr priv mode %x cycle %ld\n", serving_priv_level, rv_core->sync_trap_cause, *rv_core->trap.m.regs[trap_reg_edeleg], rv_core->curr_priv_mode, rv_core->curr_cycle);
            // printf("exception! serving: %x curr priv %x "PRINTF_FMT" "PRINTF_FMT" pc: "PRINTF_FMT"\n", serving_priv_level, rv_core->curr_priv_mode, rv_core->sync_trap_cause, *rv_core->trap.m.regs[trap_reg_status], rv_core->pc);
            rv_core->pc = trap_serve_interrupt(&rv_core->trap, serving_priv_level, rv_core->curr_priv_mode, 0, rv_core->sync_trap_cause, rv_core->pc, rv_core->sync_trap_tval);
            rv_core->curr_priv_mode = serving_priv_level;
            rv_core->sync_trap_pending = 0;
            rv_core->sync_trap_cause = 0;
            rv_core->sync_trap_tval = 0;
            return 1;
        }

        /* For simplicity we just stupidly go down from machine exti to user swi
         * Altough the correct order should be (exti, swi, timer, probably for each priv level separately)
         * Simplicity definitely wins here over spec correctness
         */
        for(interrupt_cause=trap_cause_machine_exti;interrupt_cause>=trap_cause_user_swi;interrupt_cause--)
        {
            trap_retval = trap_check_interrupt_pending(&rv_core->trap, rv_core->curr_priv_mode, interrupt_cause, &serving_priv_level);
            if(trap_retval)
            {
                rv_core->pc = trap_serve_interrupt(&rv_core->trap, serving_priv_level, rv_core->curr_priv_mode, 1, interrupt_cause, rv_core->pc, rv_core->sync_trap_tval);
                rv_core->curr_priv_mode = serving_priv_level;
                return 1;
            }
        }

        return 0;
    }
#endif

static inline rv_uint_xlen rv_core_fetch(rv_core_td *rv_core)
{
    rv_uint_xlen addr = rv_core->pc;

    return mmu_checked_bus_access(rv_core, rv_core->curr_priv_mode, bus_instr_access, addr, &rv_core->instruction, sizeof(rv_uint_xlen));
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

    if(rv_core_fetch(rv_core) == rv_ok)
    {
        rv_core_decode(rv_core);
        rv_core_execute(rv_core);
    }

    /* increase program counter here */
    rv_core->pc = rv_core->next_pc ? rv_core->next_pc : rv_core->pc + 4;

    rv_core->curr_cycle++;
    rv_core->csr_regs[CSR_ADDR_MCYCLE].value = rv_core->curr_cycle;
    rv_core->csr_regs[CSR_ADDR_MINSTRET].value = rv_core->curr_cycle;
    rv_core->csr_regs[CSR_ADDR_CYCLE].value = rv_core->curr_cycle;
    rv_core->csr_regs[CSR_ADDR_TIME].value = rv_core->curr_cycle;

    #ifndef RV64
        rv_core->csr_regs[CSR_ADDR_MCYCLEH].value = rv_core->curr_cycle >> 32;
        rv_core->csr_regs[CSR_ADDR_MINSTRETH].value = rv_core->curr_cycle >> 32;
        rv_core->csr_regs[CSR_ADDR_CYCLEH].value = rv_core->curr_cycle >> 32;
        rv_core->csr_regs[CSR_ADDR_TIMEH].value = rv_core->curr_cycle >> 32;
    #endif
}

void rv_core_process_interrupts(rv_core_td *rv_core, uint8_t mei, uint8_t mti, uint8_t msi)
{
    #ifdef CSR_SUPPORT
        /* interrupt handling */
        rv_core_update_interrupts(rv_core, mei, mti, msi);
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

static void rv_core_init_csr_regs(rv_core_td *rv_core)
{
    uint16_t i = 0;
    rv_uint_xlen xstatus_warl_bits = 0;

    #ifdef RV64
        xstatus_warl_bits = (CSR_XLEN_64_BIT << CSR_UXL_BIT_BASE) | (CSR_XLEN_64_BIT << CSR_SXL_BIT_BASE);
    #endif

    /* Machine Information Registers */
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, CSR_ADDR_MVENDORID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, CSR_ADDR_MARCHID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, CSR_ADDR_MIMPID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, CSR_ADDR_MHARTID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);

    /* Machine Trap Setup */
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MSTATUS, CSR_ACCESS_RW(machine_mode), CSR_MSTATUS_MASK, xstatus_warl_bits, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_status);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MISA, CSR_ACCESS_RO(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_isa);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MEDELEG, CSR_ACCESS_RW(machine_mode), CSR_MEDELEG_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_edeleg);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MIDELEG, CSR_ACCESS_RW(machine_mode), CSR_MIDELEG_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_ideleg);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MIE, CSR_ACCESS_RW(machine_mode), CSR_MIP_MIE_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_ie);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MTVEC, CSR_ACCESS_RW(machine_mode), CSR_MTVEC_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_tvec);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, CSR_ADDR_MCOUNTEREN, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);

    /* Machine Trap Handling */
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MSCRATCH, CSR_ACCESS_RW(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_scratch);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MEPC, CSR_ACCESS_RW(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_epc);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MCAUSE, CSR_ACCESS_RW(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_cause);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MTVAL, CSR_ACCESS_RW(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_tval);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_MIP, CSR_ACCESS_RW(machine_mode), CSR_MIP_MIE_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_m_read, trap_m_write, trap_reg_ip);

    /* Set supported ISA Extension bits */
    *rv_core->trap.m.regs[trap_reg_isa] = RV_SUPPORTED_EXTENSIONS;
    #ifdef RV64
        *rv_core->trap.m.regs[trap_reg_isa] |= (CSR_XLEN_64_BIT << (XLEN-2));
    #else
        *rv_core->trap.m.regs[trap_reg_isa] |= (CSR_XLEN_32_BIT << (XLEN-2));
    #endif

    /* Machine Protection and Translation */
    for(i=0;i<PMP_NR_CFG_REGS;i++)
    {
        #ifdef PMP_SUPPORT
            INIT_CSR_REG_SPECIAL(rv_core->csr_regs, (CSR_PMPCFG0+i), CSR_ACCESS_RW(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->pmp, pmp_read_csr_cfg, pmp_write_csr_cfg, i);
        #else
            INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_PMPCFG0+i), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
        #endif
    }

    /* All others are WARL */
    for(i=PMP_NR_CFG_REGS;i<PMP_NR_CFG_REGS_WARL_MAX;i++)
        INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_PMPCFG0+i), CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);

    for(i=0;i<PMP_NR_ADDR_REGS;i++)
    {
        #ifdef PMP_SUPPORT
            INIT_CSR_REG_SPECIAL(rv_core->csr_regs, (CSR_PMPADDR0+i), CSR_ACCESS_RW(machine_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->pmp, pmp_read_csr_addr, pmp_write_csr_addr, i);
        #else
            INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_PMPADDR0+i), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
        #endif
    }

    /* All others are WARL */
    for(i=PMP_NR_ADDR_REGS;i<PMP_NR_ADDR_REGS_WARL_MAX;i++)
        INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_PMPADDR0+i), CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);

    /* Supervisor Trap Setup */
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SSTATUS, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_SSTATUS_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_status);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SEDELEG, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_SEDELEG_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_edeleg);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SIDELEG, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_SIDELEG_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_ideleg);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SIE, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_SIP_SIE_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_ie);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_STVEC, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_STVEC_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_tvec);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, CSR_ADDR_SCOUNTEREN, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);

    /* Supervisor Trap Setup */
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SSCRATCH, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_scratch);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SEPC, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_epc);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SCAUSE, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_cause);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_STVAL, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_MASK_WR_ALL, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_tval);
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SIP, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_SIP_SIE_MASK, CSR_MASK_ZERO, &rv_core->trap, trap_s_read, trap_s_write, trap_reg_ip);

    /* Supervisor Address Translation and Protection */
    INIT_CSR_REG_SPECIAL(rv_core->csr_regs, CSR_ADDR_SATP, CSR_ACCESS_RW(machine_mode) | CSR_ACCESS_RW(supervisor_mode), CSR_SATP_MASK, CSR_MASK_ZERO, &rv_core->mmu, mmu_read_csr, mmu_write_csr, 0);

    /* Performance Counters */
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_MCYCLE), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_MCYCLEH), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_MINSTRET), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_MINSTRETH), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);

    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_CYCLE), CSR_ACCESS_RO(machine_mode) | CSR_ACCESS_RO(supervisor_mode) | CSR_ACCESS_RO(user_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_CYCLEH), CSR_ACCESS_RO(machine_mode) | CSR_ACCESS_RO(supervisor_mode) | CSR_ACCESS_RO(user_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_TIME), CSR_ACCESS_RO(machine_mode) | CSR_ACCESS_RO(supervisor_mode) | CSR_ACCESS_RO(user_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);
    INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_TIMEH), CSR_ACCESS_RO(machine_mode) | CSR_ACCESS_RO(supervisor_mode) | CSR_ACCESS_RO(user_mode), 0, CSR_MASK_WR_ALL, CSR_MASK_ZERO);

    /* All others are WARL, they start at 3 */
    for(i=3;i<CSR_HPMCOUNTER_WARL_MAX;i++)
        INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_MCYCLE+i), CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);
        INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_CYCLE+i), CSR_ACCESS_RO(machine_mode) | CSR_ACCESS_RO(supervisor_mode) | CSR_ACCESS_RO(user_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);

    for(i=3;i<CSR_HPMCOUNTER_WARL_MAX;i++)
        INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_MCYCLEH+i), CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);
        INIT_CSR_REG_DEFAULT(rv_core->csr_regs, (CSR_ADDR_CYCLEH+i), CSR_ACCESS_RO(machine_mode) | CSR_ACCESS_RO(supervisor_mode) | CSR_ACCESS_RO(user_mode), 0, CSR_MASK_ZERO, CSR_MASK_ZERO);
}

void rv_core_init(rv_core_td *rv_core,
                  void *priv,
                  bus_access_func bus_access
                  )
{
    memset(rv_core, 0, sizeof(rv_core_td));

    rv_core->curr_priv_mode = machine_mode;
    rv_core->pc = MROM_BASE_ADDR;

    rv_core->priv = priv;
    rv_core->bus_access = bus_access;

    trap_init(&rv_core->trap);
    mmu_init(&rv_core->mmu, pmp_checked_bus_access, rv_core);

    rv_core_init_csr_regs(rv_core);
}
