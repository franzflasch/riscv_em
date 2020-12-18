#ifndef RISCV_INSTR_H
#define RISCV_INSTR_H

/* R-Type Instructions */
#define INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_MUL_MULH_MULHSU_MULHU_DIV_DIVU_REM_REMU 0x33
    #define FUNC3_INSTR_ADD_SUB_MUL 0x0
        #define FUNC7_INSTR_ADD 0x00
        #define FUNC7_INSTR_MUL 0x01
        #define FUNC7_INSTR_SUB 0x20
    #define FUNC3_INSTR_SLL_MULH 0x1
        #define FUNC7_INSTR_SLL 0x00
        #define FUNC7_INSTR_MULH 0x01
    #define FUNC3_INSTR_SLT_MULHSU 0x2
        #define FUNC7_INSTR_SLT 0x00
        #define FUNC7_INSTR_MULHSU 0x01
    #define FUNC3_INSTR_SLTU_MULHU 0x3
        #define FUNC7_INSTR_SLTU 0x00
        #define FUNC7_INSTR_MULHU 0x01
    #define FUNC3_INSTR_XOR_DIV 0x4
        #define FUNC7_INSTR_XOR 0x00
        #define FUNC7_INSTR_DIV 0x01
    #define FUNC3_INSTR_SRL_SRA_DIVU 0x5
        #define FUNC7_INSTR_SRL 0x00
        #define FUNC7_INSTR_SRA 0x20
        #define FUNC7_INSTR_DIVU 0x01
    #define FUNC3_INSTR_OR_REM 0x6
        #define FUNC7_INSTR_OR 0x00
        #define FUNC7_INSTR_REM 0x01
    #define FUNC3_INSTR_AND_REMU 0x7
        #define FUNC7_INSTR_AND 0x00
        #define FUNC7_INSTR_REMU 0x01

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
        #define FUNC6_INSTR_SRLI 0x0
        #define FUNC6_INSTR_SRAI 0x10

#define INSTR_LB_LH_LW_LBU_LHU_LWU_LD 0x03
    #define FUNC3_INSTR_LB 0x0
    #define FUNC3_INSTR_LH 0x1
    #define FUNC3_INSTR_LW 0x2
    #define FUNC3_INSTR_LBU 0x4
    #define FUNC3_INSTR_LHU 0x5
    #define FUNC3_INSTR_LWU 0x6
    #define FUNC3_INSTR_LD 0x3

/* S-Type Instructions */
#define INSTR_SB_SH_SW_SD 0x23
    #define FUNC3_INSTR_SB 0x0
    #define FUNC3_INSTR_SH 0x1
    #define FUNC3_INSTR_SW 0x2
    #define FUNC3_INSTR_SD 0x3

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

#define INSTR_ECALL_EBREAK_MRET_SRET_URET_CSRRW_CSRRS_CSRRC_CSRRWI_CSRRSI_CSRRCI 0x73
    #define FUNC3_INSTR_ECALL_EBREAK_MRET_SRET_URET 0x0
        #define FUNC12_ECALL 0x0
        #define FUNC12_EBREAK 0x1
        #define FUNC12_INSTR_MRET 0x302
        #define FUNC12_INSTR_SRET 0x102
        #define FUNC12_INSTR_URET 0x2
    #define FUNC3_INSTR_CSRRW 0x1
    #define FUNC3_INSTR_CSRRS 0x2
    #define FUNC3_INSTR_CSRRC 0x3
    #define FUNC3_INSTR_CSRRWI 0x5
    #define FUNC3_INSTR_CSRRSI 0x6
    #define FUNC3_INSTR_CSRRCI 0x7

#define INSTR_ADDIW_SLLIW_SRLIW_SRAIW 0x1B
    #define FUNC3_INSTR_SLLIW 0x1
    #define FUNC3_INSTR_SRLIW_SRAIW 0x5
        #define FUNC7_INSTR_SRLIW 0x0
        #define FUNC7_INSTR_SRAIW 0x20
    #define FUNC3_INSTR_ADDIW 0x0

#define INSTR_ADDW_SUBW_SLLW_SRLW_SRAW_MULW_DIVW_DIVUW_REMW_REMUW 0x3B
    #define FUNC3_INSTR_ADDW_SUBW_MULW 0x0
        #define FUNC7_INSTR_ADDW 0x00
        #define FUNC7_INSTR_SUBW 0x20
        #define FUNC7_INSTR_MULW 0x01
    #define FUNC3_INSTR_SLLW 0x1
    #define FUNC3_INSTR_DIVW 0x4
    #define FUNC3_INSTR_SRLW_SRAW_DIVUW 0x5
        #define FUNC7_INSTR_SRLW 0x00
        #define FUNC7_INSTR_SRAW 0x20
        #define FUNC7_INSTR_DIVUW 0x01
    #define FUNC3_INSTR_REMW 0x6
    #define FUNC3_INSTR_REMUW 0x7

/* Atomic Instructions */
#define INSTR_AMO_W_D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU 0x2F
    #define FUNC3_INSTR_W_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU 0x2
    #define FUNC3_INSTR_D_LR_SC_SWAP_ADD_XOR_AND_OR_MIN_MAX_MINU_MAXU 0x3
        #define FUNC5_INSTR_AMO_LR 0x2
        #define FUNC5_INSTR_AMO_SC 0x3
        #define FUNC5_INSTR_AMO_SWAP 0x1
        #define FUNC5_INSTR_AMO_ADD 0x0
        #define FUNC5_INSTR_AMO_XOR 0x4
        #define FUNC5_INSTR_AMO_AND 0xC
        #define FUNC5_INSTR_AMO_OR 0x8
        #define FUNC5_INSTR_AMO_MIN 0x10
        #define FUNC5_INSTR_AMO_MAX 0x14
        #define FUNC5_INSTR_AMO_MINU 0x18
        #define FUNC5_INSTR_AMO_MAXU 0x1C

#endif /* RISCV_INSTR_H */
