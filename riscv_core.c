#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_config.h>
#include <riscv_helper.h>
#include <riscv_core.h>

/* Helpers */
static inline uint32_t extract32(uint32_t value, int start, int length)
{
    return (value >> start) & (~0U >> (32 - length));
}

/* portable signextension from: https://stackoverflow.com/a/31655073 */
#ifdef RV64
    #define SIGNEX(v, sb) ((v) | (((v) & (1LL << (sb))) ? ~((1LL << (sb))-1LL) : 0))
#else
    #define SIGNEX(v, sb) ((v) | (((v) & (1 << (sb))) ? ~((1 << (sb))-1) : 0))
#endif

/* Defines */
#define XREG_ZERO 0
#define XREG_RETURN_ADDRESS 0
#define XREG_STACK_POINTER 2
#define XREG_THREAD_POINTER 5

#define STACK_POINTER_START_VAL 0x0

/* R-Type Instructions */
#define INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND 0x33
    #define FUNC3_INSTR_ADD_SUB 0x0
        #define FUNC7_INSTR_ADD 0x00
        #define FUNC7_INSTR_SUB 0x20
    #define FUNC3_INSTR_SLL 0x1
        #define FUNC7_INSTR_SLL 0x00
    #define FUNC3_INSTR_SLT 0x2
        #define FUNC7_INSTR_SLT 0x00
    #define FUNC3_INSTR_SLTU 0x3
        #define FUNC7_INSTR_SLTU 0x00
    #define FUNC3_INSTR_XOR 0x4
        #define FUNC7_INSTR_XOR 0x00
    #define FUNC3_INSTR_SRL_SRA 0x5
        #define FUNC7_INSTR_SRL 0x00
        #define FUNC7_INSTR_SRA 0x20
    #define FUNC3_INSTR_OR 0x6
        #define FUNC7_INSTR_OR 0x00
    #define FUNC3_INSTR_AND 0x7
        #define FUNC7_INSTR_AND 0x00

/* I-Type Instructions */
#define INSTR_JALR 0x67
    #define FUNC3_INSTR_JALR    0x0

#define INSTR_ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI 0x13
    #define FUNC3_INSTR_ADDI    0x0
    #define FUNC3_INSTR_SLTI    0x2
    #define FUNC3_INSTR_SLTIU 0x3
    #define FUNC3_INSTR_XORI    0x4
    #define FUNC3_INSTR_ORI     0x6
    #define FUNC3_INSTR_ANDI    0x7
    #define FUNC3_INSTR_SLLI    0x1
    #define FUNC3_INSTR_SRLI_SRAI  0x5
        #define FUNC7_INSTR_SRLI 0x0
        #define FUNC7_INSTR_SRAI 0x20

#define INSTR_LB_LH_LW_LBU_LHU 0x03
    #define FUNC3_INSTR_LB 0x0
    #define FUNC3_INSTR_LH 0x1
    #define FUNC3_INSTR_LW 0x2
    #define FUNC3_INSTR_LBU 0x4
    #define FUNC3_INSTR_LHU 0x5

/* S-Type Instructions */
#define INSTR_SB_SH_SW 0x23
    #define FUNC3_INSTR_SB 0x0
    #define FUNC3_INSTR_SH 0x1
    #define FUNC3_INSTR_SW 0x2

/* B-Type Instructions */
#define INSTR_BEQ_BNE_BLT_BGE_BLTU_BGEU 0x63
    #define FUNC3_INSTR_BEQ 0x0
    #define FUNC3_INSTR_BNE 0x1
    #define FUNC3_INSTR_BLT 0x4
    #define FUNC3_INSTR_BGE 0x5
    #define FUNC3_INSTR_BLTU 0x6
    #define FUNC3_INSTR_BGEU 0x7

/* U-Type Instructions */
#define INSTR_LUI 0x37   /* LOAD UPPER IMMEDIATE INTO DESTINATION REGISTER */
#define INSTR_AUIPC 0x17 /* ADD UPPER IMMEDIATE TO PROGRAM COUNTER */

/* J-Type Instructions */
#define INSTR_JAL 0x6F   /* JUMP and Link */

/* System level instructions */
#define INSTR_FENCE_FENCE_I 0x0F
    #define FUNC3_INSTR_FENCE 0x0
    #define FUNC3_INSTR_FENCE_I 0x1

#define INSTR_ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI 0x73

#define INSTR_ADDIW_SLLIW_SRLIW_SRAIW 0x1B
    #define FUNC3_INSTR_SLLIW 0x1
    #define FUNC3_INSTR_SRLIW_SRAIW 0x5
    #define FUNC3_INSTR_ADDIW 0x0
    #define FUNC7_INSTR_SLLIW 0x0
    #define FUNC7_INSTR_SRLIW 0x0
    #define FUNC7_INSTR_SRAIW 0x20

static void instr_LUI(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = (rv_core->immediate << 12);
}

static void instr_AUIPC(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = (rv_core->pc-4) + (rv_core->immediate << 12);
}

static void instr_JAL(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->pc;
    rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_JALR(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_uint_xlen curr_pc = rv_core->pc;

    rv_core->jump_offset = SIGNEX(rv_core->immediate, 11);

    rv_core->pc = (rv_core->x[rv_core->rs1] + rv_core->jump_offset);
    rv_core->pc &= ~(1<<0);
    rv_core->x[rv_core->rd] = curr_pc;
}

static void instr_BEQ(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    if(rv_core->x[rv_core->rs1] == rv_core->x[rv_core->rs2])
        rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_BNE(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    if(rv_core->x[rv_core->rs1] != rv_core->x[rv_core->rs2])
        rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_BLT(void *rv_core_data)
{
    rv_int_xlen signed_rs = 0;
    rv_int_xlen signed_rs2 = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    signed_rs = rv_core->x[rv_core->rs1];
    signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs < signed_rs2)
        rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_BGE(void *rv_core_data)
{
    rv_int_xlen signed_rs = 0;
    rv_int_xlen signed_rs2 = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    signed_rs = rv_core->x[rv_core->rs1];
    signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs >= signed_rs2)
        rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_BLTU(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    if(rv_core->x[rv_core->rs1] < rv_core->x[rv_core->rs2])
        rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_BGEU(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    if(rv_core->x[rv_core->rs1] >= rv_core->x[rv_core->rs2])
        rv_core->pc = (rv_core->pc-4) + rv_core->jump_offset;
}

static void instr_ADDI(void *rv_core_data)
{
    rv_int_xlen signed_immediate = 0;
    rv_int_xlen signed_rs_val = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    signed_immediate = SIGNEX(rv_core->immediate, 11);

    signed_rs_val = rv_core->x[rv_core->rs1];
    rv_core->x[rv_core->rd] = (signed_immediate + signed_rs_val);
}

static void instr_SLTI(void *rv_core_data)
{
    rv_int_xlen signed_immediate = 0;
    rv_int_xlen signed_rs_val = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    signed_immediate = SIGNEX(rv_core->immediate, 11);

    signed_rs_val = rv_core->x[rv_core->rs1];

    if(signed_rs_val < signed_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTIU(void *rv_core_data)
{
    rv_uint_xlen unsigned_immediate = 0;
    rv_uint_xlen unsigned_rs_val = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    unsigned_immediate = SIGNEX(rv_core->immediate, 11);

    unsigned_rs_val = rv_core->x[rv_core->rs1];

    if(unsigned_rs_val < unsigned_immediate)
        rv_core->x[rv_core->rd] = 1;
    else
        rv_core->x[rv_core->rd] = 0;
}

static void instr_XORI(void *rv_core_data)
{
    rv_int_xlen signed_immediate = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    signed_immediate = rv_core->immediate;

    if(signed_immediate == -1)
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ 1;
    else
        rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ rv_core->immediate;
}

static void instr_ORI(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | rv_core->immediate;
}

static void instr_ANDI(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->immediate = SIGNEX(rv_core->immediate, 11);
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & rv_core->immediate;
}

static void instr_SLLI(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << rv_core->immediate);
}

static void instr_SRAI(void *rv_core_data)
{
    rv_int_xlen rs_val = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    /* a right shift on signed ints seem to be always arithmetic */
    rs_val = rv_core->x[rv_core->rs1];
    rs_val = rs_val >> rv_core->immediate;
    rv_core->x[rv_core->rd] = rs_val;
}

static void instr_SRLI(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] >> rv_core->immediate);
}

static void instr_ADD(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] + rv_core->x[rv_core->rs2];
}

static void instr_SUB(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] - rv_core->x[rv_core->rs2];
}

static void instr_SLL(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] << (rv_core->x[rv_core->rs2] & 0x1F);
}

static void instr_SLT(void *rv_core_data)
{
    rv_int_xlen signed_rs = 0;
    rv_int_xlen signed_rs2 = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    signed_rs = rv_core->x[rv_core->rs1];
    signed_rs2 = rv_core->x[rv_core->rs2];

    if(signed_rs < signed_rs2) rv_core->x[rv_core->rd] = 1;
    else rv_core->x[rv_core->rd] = 0;
}

static void instr_SLTU(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    if(rv_core->rs1 == 0)
    {
        if(rv_core->x[rv_core->rs2])
            rv_core->x[rv_core->rd] = 1;
        else
            rv_core->x[rv_core->rd] = 0;
    }
    else
    {
        if(rv_core->x[rv_core->rs1] < rv_core->x[rv_core->rs2]) rv_core->x[rv_core->rd] = 1;
        else rv_core->x[rv_core->rd] = 0;
    }
}

static void instr_XOR(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] ^ rv_core->x[rv_core->rs2];
}

static void instr_SRL(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] >> (rv_core->x[rv_core->rs2] & 0x1F);
}

static void instr_OR(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] | (rv_core->x[rv_core->rs2]);
}

static void instr_AND(void *rv_core_data)
{
    rv_core_td *rv_core = (rv_core_td *)rv_core_data;
    rv_core->x[rv_core->rd] = rv_core->x[rv_core->rs1] & (rv_core->x[rv_core->rs2]);
}

static void instr_SRA(void *rv_core_data)
{
    rv_int_xlen signed_rs = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    signed_rs = rv_core->x[rv_core->rs1];
    rv_core->x[rv_core->rd] = signed_rs >> (rv_core->x[rv_core->rs2] & 0x1F);
}

static void instr_LB(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;
    uint8_t tmp_load_val = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    tmp_load_val = rv_core->read_mem(rv_core->priv, address) & 0x000000FF;

    rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 7);
}

static void instr_LH(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;
    uint16_t tmp_load_val = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    tmp_load_val = rv_core->read_mem(rv_core->priv, address) & 0x0000FFFF;

    rv_core->x[rv_core->rd] = SIGNEX(tmp_load_val, 15);
}

static void instr_LW(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    rv_core->x[rv_core->rd] = rv_core->read_mem(rv_core->priv, address);
}

static void instr_LBU(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    rv_core->x[rv_core->rd] = rv_core->read_mem(rv_core->priv, address) & 0x000000FF;
}

static void instr_LHU(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    rv_core->x[rv_core->rd] = rv_core->read_mem(rv_core->priv, address) & 0x0000FFFF;
}

static void instr_SB(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;
    uint8_t value_to_write = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    value_to_write = (uint8_t)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 1);
}

static void instr_SH(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;
    uint16_t value_to_write = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    value_to_write = (uint16_t)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 2);
}

static void instr_SW(void *rv_core_data)
{
    rv_int_xlen signed_offset = 0;
    rv_uint_xlen address = 0;
    rv_uint_xlen value_to_write = 0;

    rv_core_td *rv_core = (rv_core_td *)rv_core_data;

    rv_core->immediate = SIGNEX(rv_core->immediate, 11);

    signed_offset = rv_core->immediate;
    address = rv_core->x[rv_core->rs1] + signed_offset;
    value_to_write = (rv_uint_xlen)rv_core->x[rv_core->rs2];
    rv_core->write_mem(rv_core->priv, address, value_to_write, 4);
}

#ifdef RV64
    static void shift_add_w_signed_prepare(void *rv_core_data, uint8_t shift)
    {
        int32_t signed_immediate = 0;
        int32_t signed_rs_val = 0;
        rv_core_td *rv_core = (rv_core_td *)rv_core_data;
        signed_immediate = SIGNEX(rv_core->immediate, 11);
        signed_rs_val = rv_core->x[rv_core->rs1];

        if(shift)
            rv_core->x[rv_core->rd] = (signed_rs_val >> rv_core->immediate);
        else
            rv_core->x[rv_core->rd] = (signed_rs_val + signed_immediate);
    }

    static void instr_SRAIW(void *rv_core_data)
    {
        shift_add_w_signed_prepare(rv_core_data, 1);
    }

    static void instr_ADDIW(void *rv_core_data)
    {
        shift_add_w_signed_prepare(rv_core_data, 0);
    }

    static void instr_SLLIW(void *rv_core_data)
    {
        rv_core_td *rv_core = (rv_core_td *)rv_core_data;
        rv_core->x[rv_core->rd] = (rv_core->x[rv_core->rs1] << rv_core->immediate) & 0xFFFFFFFF;
        rv_core->x[rv_core->rd] = SIGNEX(rv_core->x[rv_core->rd], 31);
    }

    static void instr_SRLIW(void *rv_core_data)
    {
        uint32_t unsigned_rs_val = 0;
        rv_core_td *rv_core = (rv_core_td *)rv_core_data;
        unsigned_rs_val = rv_core->x[rv_core->rs1];
        rv_core->x[rv_core->rd] = (unsigned_rs_val >> rv_core->immediate);
        rv_core->x[rv_core->rd] = SIGNEX(rv_core->x[rv_core->rd], 31);
    }
#endif

static void die(rv_core_td *rv_core)
{
    printf("Unknown instruction %08x "PRINTF_FMT"\n", rv_core->instruction, rv_core->pc);
    exit(-1);
}

typedef struct instruction_hook_struct
{
    uint32_t opcode;
    void (*preparation_cb)(rv_core_td *rv_core, int32_t *next_subcode);
    void (*execution_cb)(void *rv_core_data);
    struct instruction_desc_struct *next;

} instruction_hook_td;

typedef struct instruction_desc_struct
{
    unsigned int instruction_hook_list_size;
    instruction_hook_td *instruction_hook_list;

} instruction_desc_td;
#define INIT_INSTRUCTION_LIST_DESC(instruction_list) \
    static instruction_desc_td  instruction_list##_desc = \
    { sizeof(instruction_list)/sizeof(instruction_list[0]), instruction_list }

static void R_type_preparation_func3(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->rd = ((rv_core->instruction >> 7) & 0x1F);
    rv_core->func3 = ((rv_core->instruction >> 12) & 0x7);
    rv_core->rs1 = ((rv_core->instruction >> 15) & 0x1F);
    rv_core->rs2 = ((rv_core->instruction >> 20) & 0x1F);
    *next_subcode = rv_core->func3;
}

static void preparation_func7(rv_core_td *rv_core, int32_t *next_subcode)
{
    rv_core->func7 = ((rv_core->instruction >> 25) & 0x7F);
    *next_subcode = rv_core->func7;
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
    { FUNC3_INSTR_JALR, NULL, instr_JALR, NULL}
};
INIT_INSTRUCTION_LIST_DESC(JALR_func3_subcode_list);

static instruction_hook_td BEQ_BNE_BLT_BGE_BLTU_BGEU_func3_subcode_list[] = {
    { FUNC3_INSTR_BEQ, NULL, instr_BEQ, NULL},
    { FUNC3_INSTR_BNE, NULL, instr_BNE, NULL},
    { FUNC3_INSTR_BLT, NULL, instr_BLT, NULL},
    { FUNC3_INSTR_BGE, NULL, instr_BGE, NULL},
    { FUNC3_INSTR_BLTU, NULL, instr_BLTU, NULL},
    { FUNC3_INSTR_BGEU, NULL, instr_BGEU, NULL},
};
INIT_INSTRUCTION_LIST_DESC(BEQ_BNE_BLT_BGE_BLTU_BGEU_func3_subcode_list);

static instruction_hook_td LB_LH_LW_LBU_LHU_func3_subcode_list[] = {
    { FUNC3_INSTR_LB, NULL, instr_LB, NULL},
    { FUNC3_INSTR_LH, NULL, instr_LH, NULL},
    { FUNC3_INSTR_LW, NULL, instr_LW, NULL},
    { FUNC3_INSTR_LBU, NULL, instr_LBU, NULL},
    { FUNC3_INSTR_LHU, NULL, instr_LHU, NULL},
};
INIT_INSTRUCTION_LIST_DESC(LB_LH_LW_LBU_LHU_func3_subcode_list);

static instruction_hook_td SB_SH_SW_func3_subcode_list[] = {
    { FUNC3_INSTR_SB, NULL, instr_SB, NULL},
    { FUNC3_INSTR_SH, NULL, instr_SH, NULL},
    { FUNC3_INSTR_SW, NULL, instr_SW, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SB_SH_SW_func3_subcode_list);

static instruction_hook_td SRLI_SRAI_func7_subcode_list[] = {
    { FUNC7_INSTR_SRLI, NULL, instr_SRLI, NULL},
    { FUNC7_INSTR_SRAI, NULL, instr_SRAI, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SRLI_SRAI_func7_subcode_list);

static instruction_hook_td ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list[] = {
    { FUNC3_INSTR_ADDI, NULL, instr_ADDI, NULL},
    { FUNC3_INSTR_SLTI, NULL, instr_SLTI, NULL},
    { FUNC3_INSTR_SLTIU, NULL, instr_SLTIU, NULL},
    { FUNC3_INSTR_XORI, NULL, instr_XORI, NULL},
    { FUNC3_INSTR_ORI, NULL, instr_ORI, NULL},
    { FUNC3_INSTR_ANDI, NULL, instr_ANDI, NULL},
    { FUNC3_INSTR_SLLI, NULL, instr_SLLI, NULL},
    { FUNC3_INSTR_SRLI_SRAI, preparation_func7, NULL, &SRLI_SRAI_func7_subcode_list_desc},
};
INIT_INSTRUCTION_LIST_DESC(ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list);

static instruction_hook_td ADD_SUB_func7_subcode_list[] = {
    { FUNC7_INSTR_ADD, NULL, instr_ADD, NULL},
    { FUNC7_INSTR_SUB, NULL, instr_SUB, NULL},
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_func7_subcode_list);

static instruction_hook_td SRL_SRA_func7_subcode_list[] = {
    { FUNC7_INSTR_SRL, NULL, instr_SRL, NULL},
    { FUNC7_INSTR_SRA, NULL, instr_SRA, NULL},
};
INIT_INSTRUCTION_LIST_DESC(SRL_SRA_func7_subcode_list);

static instruction_hook_td ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list[] = {
    { FUNC3_INSTR_ADD_SUB, preparation_func7, NULL, &ADD_SUB_func7_subcode_list_desc},
    { FUNC3_INSTR_SLL, NULL, instr_SLL, NULL},
    { FUNC3_INSTR_SLT, NULL, instr_SLT, NULL},
    { FUNC3_INSTR_SLTU, NULL, instr_SLTU, NULL},
    { FUNC3_INSTR_XOR, NULL, instr_XOR, NULL},
    { FUNC3_INSTR_SRL_SRA, preparation_func7, NULL, &SRL_SRA_func7_subcode_list_desc},
    { FUNC3_INSTR_OR, NULL, instr_OR, NULL},
    { FUNC3_INSTR_AND, NULL, instr_AND, NULL},
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list);

#ifdef RV64
    static instruction_hook_td SLLIW_func7_subcode_list[] = {
        { FUNC7_INSTR_SLLIW, NULL, instr_SLLIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SLLIW_func7_subcode_list);

    static instruction_hook_td SRLIW_SRAIW_func7_subcode_list[] = {
        { FUNC7_INSTR_SRLIW, NULL, instr_SRLIW, NULL},
        { FUNC7_INSTR_SRAIW, NULL, instr_SRAIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SRLIW_SRAIW_func7_subcode_list);

    static instruction_hook_td SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list[] = {
        { FUNC3_INSTR_SLLIW, preparation_func7, NULL, &SLLIW_func7_subcode_list_desc},
        { FUNC3_INSTR_SRLIW_SRAIW, preparation_func7, NULL, &SRLIW_SRAIW_func7_subcode_list_desc},
        { FUNC3_INSTR_ADDIW, NULL, instr_ADDIW, NULL},
    };
    INIT_INSTRUCTION_LIST_DESC(SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list);
#endif

static instruction_hook_td RV_opcode_list[] = {
    { INSTR_LUI, U_type_preparation, instr_LUI, NULL},
    { INSTR_AUIPC, U_type_preparation, instr_AUIPC, NULL},
    { INSTR_JAL, J_type_preparation, instr_JAL, NULL},
    { INSTR_JALR, I_type_preparation, NULL, &JALR_func3_subcode_list_desc},
    { INSTR_BEQ_BNE_BLT_BGE_BLTU_BGEU, B_type_preparation, NULL, &BEQ_BNE_BLT_BGE_BLTU_BGEU_func3_subcode_list_desc},
    { INSTR_LB_LH_LW_LBU_LHU, I_type_preparation, NULL, &LB_LH_LW_LBU_LHU_func3_subcode_list_desc},
    { INSTR_SB_SH_SW, S_type_preparation, NULL, &SB_SH_SW_func3_subcode_list_desc},
    { INSTR_ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI, I_type_preparation, NULL, &ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list_desc},
    { INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND, R_type_preparation_func3, NULL, &ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list_desc},
    { INSTR_FENCE_FENCE_I, NULL, NULL, NULL}, /* Not implemented */
    { INSTR_ECALL_EBREAK_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI, NULL, NULL, NULL}, /* Not implemented */

    #ifdef RV64
    { INSTR_ADDIW_SLLIW_SRLIW_SRAIW, I_type_preparation, NULL, &SLLIW_SRLIW_SRAIW_ADDIW_func3_subcode_list_desc}, /* Not implemented */
    #endif
};
INIT_INSTRUCTION_LIST_DESC(RV_opcode_list);

static void rv_call_from_opcode_list(rv_core_td *rv_core, instruction_desc_td *opcode_list_desc, uint32_t opcode)
{
    unsigned int opcode_index = 0;
    int32_t next_subcode = -1;

    unsigned int list_size = opcode_list_desc->instruction_hook_list_size;
    instruction_hook_td *opcode_list = opcode_list_desc->instruction_hook_list;

    for(opcode_index=0;opcode_index<list_size;opcode_index++)
    {
        if(opcode_list[opcode_index].opcode == opcode)
            break;
    }

    if(opcode_index == list_size) die(rv_core);

    if(opcode_list[opcode_index].preparation_cb != NULL)
        opcode_list[opcode_index].preparation_cb(rv_core, &next_subcode);

    if(opcode_list[opcode_index].execution_cb != NULL)
        rv_core->execute_cb = opcode_list[opcode_index].execution_cb;

    if((next_subcode != -1) && (opcode_list[opcode_index].next != NULL))
        rv_call_from_opcode_list(rv_core, opcode_list[opcode_index].next, next_subcode);
}


/******************* Public functions *******************************/
rv_uint_xlen rv_core_fetch(rv_core_td *rv_core)
{
    rv_uint_xlen addr = rv_core->pc;

    rv_core->instruction = rv_core->read_mem(rv_core->priv, addr);

    /* increase program counter here */
    rv_core->pc += 4;

    return 0;
}

rv_uint_xlen rv_core_decode(rv_core_td *rv_core)
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

rv_uint_xlen rv_core_execute(rv_core_td *rv_core)
{
    rv_core->execute_cb(rv_core);

    /* clear x0 if any instruction has written into it */
    rv_core->x[0] = 0;

    return 0;
}

void rv_core_run(rv_core_td *rv_core)
{
    rv_core_fetch(rv_core);
    rv_core_decode(rv_core);
    rv_core_execute(rv_core);
}

void rv_core_reg_dump_before_exec(rv_core_td *rv_core)
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
                    rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address),
                    void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes)
                    )
{
    memset(rv_core, 0, sizeof(rv_core_td));
    rv_core->pc = RAM_BASE_ADDR;
    rv_core->x[XREG_THREAD_POINTER] = RAM_BASE_ADDR;
    rv_core->x[XREG_STACK_POINTER] = STACK_POINTER_START_VAL;

    rv_core->priv = priv;
    rv_core->read_mem = read_mem;
    rv_core->write_mem = write_mem;
}
