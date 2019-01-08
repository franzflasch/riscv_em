#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NR_RV32I_REGS 32
#define XREG_ZERO 0
#define XREG_RETURN_ADDRESS 0
#define XREG_STACK_POINTER 2

#define NR_RAM_WORDS 1024
#define NR_ROM_WORDS 1024

#define STACK_POINTER_START_VAL (4*NR_RAM_WORDS)
#define PROGRAM_COUNTER_START_VAL 0x100000

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
  #define FUNC3_INSTR_JALR  0x0

#define INSTR_ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI 0x13
  #define FUNC3_INSTR_ADDI  0x0
  #define FUNC3_INSTR_SLTI  0x2
  #define FUNC3_INSTR_SLTIU 0x3
  #define FUNC3_INSTR_XORI  0x4
  #define FUNC3_INSTR_ORI   0x6
  #define FUNC3_INSTR_ANDI  0x7
  #define FUNC3_INSTR_SLLI  0x1
  #define FUNC3_INSTR_SRLI_SRAI  0x5

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

typedef uint32_t (*read_mem_func)(void *priv, uint32_t address);
typedef void (*write_mem_func)(void *priv, uint32_t address, uint32_t value, uint8_t nr_bytes);

static inline uint32_t extract32(uint32_t value, int start, int length)
{
    return (value >> start) & (~0U >> (32 - length));
}

typedef struct rv32_core_struct
{
  /* Registers */
  uint32_t x[NR_RV32I_REGS];
  uint32_t pc;

  uint32_t instruction;
  uint8_t opcode;
  uint8_t rd;
  uint8_t rs1;
  uint8_t rs2;
  uint8_t func3;
  uint8_t func7;
  uint32_t immediate;
  int32_t jump_offset;

  /* points to the next instruction */
  void (*execute_cb)(void *rv32_core);

  /* externally hooked */
  void *priv;
  read_mem_func read_mem;
  write_mem_func write_mem;

} rv32_core_td;

static void instr_LUI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = (rv32_core->immediate << 12);
}

static void instr_AUIPC(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = (rv32_core->pc-4) + (rv32_core->immediate << 12);
}

static void instr_JAL(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->pc;
  rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_JALR(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  rv32_core->jump_offset = rv32_core->immediate;
  if((1<<11) & rv32_core->jump_offset) rv32_core->jump_offset=(rv32_core->jump_offset | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->pc;
  rv32_core->pc = (rv32_core->x[rv32_core->rs1] + rv32_core->jump_offset);
  rv32_core->pc &= ~(1<<0);
}

static void instr_BEQ(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  if(rv32_core->x[rv32_core->rs1] == rv32_core->x[rv32_core->rs2])
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_BNE(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  if(rv32_core->x[rv32_core->rs1] != rv32_core->x[rv32_core->rs2])
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_BLT(void *rv32_core_data)
{
  int32_t signed_rs = 0;
  int32_t signed_rs2 = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  signed_rs = rv32_core->x[rv32_core->rs1];
  signed_rs2 = rv32_core->x[rv32_core->rs2];

  if(signed_rs < signed_rs2)
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_BGE(void *rv32_core_data)
{
  int32_t signed_rs = 0;
  int32_t signed_rs2 = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  signed_rs = rv32_core->x[rv32_core->rs1];
  signed_rs2 = rv32_core->x[rv32_core->rs2];

  if(signed_rs >= signed_rs2)
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_BLTU(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if(rv32_core->x[rv32_core->rs1] < rv32_core->x[rv32_core->rs2])
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_BGEU(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if(rv32_core->x[rv32_core->rs1] >= rv32_core->x[rv32_core->rs2])
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_ADDI(void *rv32_core_data)
{
  int32_t signed_immediate = 0;
  int32_t signed_rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) signed_immediate = (rv32_core->immediate | 0xFFFFF000);
  else signed_immediate = rv32_core->immediate;

  signed_rs_val = rv32_core->x[rv32_core->rs1];
  rv32_core->x[rv32_core->rd] = (signed_immediate + signed_rs_val);
}

static void instr_SLTI(void *rv32_core_data)
{
  int32_t signed_immediate = 0;
  int32_t signed_rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) signed_immediate = (rv32_core->immediate | 0xFFFFF000);
  else signed_immediate = rv32_core->immediate;

  signed_rs_val = rv32_core->x[rv32_core->rs1];

  if(signed_rs_val < signed_immediate)
    rv32_core->x[rv32_core->rd] = 1;
  else
    rv32_core->x[rv32_core->rd] = 0;
}

static void instr_SLTIU(void *rv32_core_data)
{
  uint32_t unsigned_immediate = 0;
  uint32_t unsigned_rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) unsigned_immediate = (rv32_core->immediate | 0xFFFFF000);
  else unsigned_immediate = rv32_core->immediate;

  unsigned_rs_val = rv32_core->x[rv32_core->rs1];

  if(unsigned_rs_val < unsigned_immediate)
    rv32_core->x[rv32_core->rd] = 1;
  else
    rv32_core->x[rv32_core->rd] = 0;
}

static void instr_XORI(void *rv32_core_data)
{
  int32_t signed_immediate = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_immediate = rv32_core->immediate;

  if(signed_immediate == -1)
    rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] ^ 1;
  else
    rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] ^ rv32_core->immediate;
}

static void instr_ORI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate=(rv32_core->immediate | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] | rv32_core->immediate;
}

static void instr_ANDI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate=(rv32_core->immediate | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] & rv32_core->immediate;
}

static void instr_SLLI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = (rv32_core->x[rv32_core->rs1] << rv32_core->immediate);
}

static void instr_SRAI(void *rv32_core_data)
{
  int32_t rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  /* a right shift on signed ints seem to be always arithmetic */
  rs_val = rv32_core->x[rv32_core->rs1];
  rs_val = rs_val >> rv32_core->immediate;
  rv32_core->x[rv32_core->rd] = rs_val;
}

static void instr_SRLI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = (rv32_core->x[rv32_core->rs1] >> rv32_core->immediate);
}

static void instr_SRLI_SRAI(void *rv32_core_data)
{
  uint8_t arithmetic_shift = 0;
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  arithmetic_shift = ((rv32_core->instruction >> 25) & 0x7F);

  if(arithmetic_shift & (1<<5)) instr_SRAI(rv32_core);
  else instr_SRLI(rv32_core);
}

static void instr_ADD(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] + rv32_core->x[rv32_core->rs2];
}

static void instr_SUB(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] - rv32_core->x[rv32_core->rs2];
}

static void instr_SLL(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] << (rv32_core->x[rv32_core->rs2] & 0x1F);
}

static void instr_SLT(void *rv32_core_data)
{
  int32_t signed_rs = 0;
  int32_t signed_rs2 = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  signed_rs = rv32_core->x[rv32_core->rs1];
  signed_rs2 = rv32_core->x[rv32_core->rs2];

  if(signed_rs < signed_rs2) rv32_core->x[rv32_core->rd] = 1;
  else rv32_core->x[rv32_core->rd] = 0;
}

static void instr_SLTU(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if(rv32_core->rs1 == 0)
  {
    if(rv32_core->x[rv32_core->rs2])
      rv32_core->x[rv32_core->rd] = 1;
    else
      rv32_core->x[rv32_core->rd] = 0;
  }
  else
  {
    if(rv32_core->x[rv32_core->rs1] < rv32_core->x[rv32_core->rs2]) rv32_core->x[rv32_core->rd] = 1;
    else rv32_core->x[rv32_core->rd] = 0;
  }
}

static void instr_XOR(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] ^ rv32_core->x[rv32_core->rs2];
}

static void instr_SRL(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] >> (rv32_core->x[rv32_core->rs2] & 0x1F);
}

static void instr_OR(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] | (rv32_core->x[rv32_core->rs2]);
}

static void instr_AND(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs1] & (rv32_core->x[rv32_core->rs2]);
}

static void instr_SRA(void *rv32_core_data)
{
  int32_t signed_rs = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  signed_rs = rv32_core->x[rv32_core->rs1];
  rv32_core->x[rv32_core->rd] = signed_rs >> (rv32_core->x[rv32_core->rs2] & 0x1F);
}

static void instr_LB(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;
  uint8_t tmp_load_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  tmp_load_val = rv32_core->read_mem(rv32_core->priv, address) & 0x000000FF;

  if((1<<7) & tmp_load_val) rv32_core->x[rv32_core->rd] = (tmp_load_val | 0xFFFFFF00);
  else rv32_core->x[rv32_core->rd] = tmp_load_val;
}

static void instr_LH(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;
  uint16_t tmp_load_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  tmp_load_val = rv32_core->read_mem(rv32_core->priv, address) & 0x0000FFFF;

  if((1<<15) & tmp_load_val) rv32_core->x[rv32_core->rd] = (tmp_load_val | 0xFFFF0000);
  else rv32_core->x[rv32_core->rd] = tmp_load_val;
}

static void instr_LW(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  rv32_core->x[rv32_core->rd] = rv32_core->read_mem(rv32_core->priv, address);
}

static void instr_LBU(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  rv32_core->x[rv32_core->rd] = rv32_core->read_mem(rv32_core->priv, address) & 0x000000FF;
}

static void instr_LHU(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  rv32_core->x[rv32_core->rd] = rv32_core->read_mem(rv32_core->priv, address) & 0x0000FFFF;
}

static void instr_SB(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;
  uint8_t value_to_write = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  value_to_write = (uint8_t)rv32_core->x[rv32_core->rs2];
  rv32_core->write_mem(rv32_core->priv, address, value_to_write, 1);
}

static void instr_SH(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;
  uint16_t value_to_write = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  value_to_write = (uint16_t)rv32_core->x[rv32_core->rs2];
  rv32_core->write_mem(rv32_core->priv, address, value_to_write, 2);
}

static void instr_SW(void *rv32_core_data)
{
  int32_t signed_offset = 0;
  uint32_t address = 0;
  uint32_t value_to_write = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate = (rv32_core->immediate | 0xFFFFF000);

  signed_offset = rv32_core->immediate;
  address = rv32_core->x[rv32_core->rs1] + signed_offset;
  value_to_write = (uint32_t)rv32_core->x[rv32_core->rs2];
  rv32_core->write_mem(rv32_core->priv, address, value_to_write, 4);
}

static void die(uint32_t instruction)
{
  printf("Unknown instruction %x\n", instruction);
  exit(-1);
}

typedef struct instruction_hook_struct
{
  uint32_t opcode;
  void (*preparation_cb)(rv32_core_td *rv32_core, int32_t *next_subcode);
  void (*execution_cb)(void *rv32_core_data);
  struct instruction_desc_struct *next;

} instruction_hook_td;

typedef struct instruction_desc_struct
{
  uint32_t instruction_hook_list_size;
  instruction_hook_td *instruction_hook_list;

} instruction_desc_td;
#define INIT_INSTRUCTION_LIST_DESC(instruction_list) \
  static instruction_desc_td  instruction_list##_desc = { sizeof(instruction_list)/sizeof(instruction_list[0]), instruction_list }

static void R_type_preparation_func3(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
  rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);
  rv32_core->rs1 = ((rv32_core->instruction >> 15) & 0x1F);
  rv32_core->rs2 = ((rv32_core->instruction >> 20) & 0x1F);
  *next_subcode = rv32_core->func3;
}

static void R_type_preparation_func7(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->func7 = ((rv32_core->instruction >> 25) & 0x7F);
  *next_subcode = rv32_core->func7;
}

static void I_type_preparation(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
  rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);
  rv32_core->rs1 = ((rv32_core->instruction >> 15) & 0x1F);
  rv32_core->immediate = ((rv32_core->instruction >> 20) & 0xFFF);
  *next_subcode = rv32_core->func3;
}

static void S_type_preparation(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);
  rv32_core->rs1 = ((rv32_core->instruction >> 15) & 0x1F);
  rv32_core->rs2 = ((rv32_core->instruction >> 20) & 0x1F);
  rv32_core->immediate = (((rv32_core->instruction >> 25) << 5) | ((rv32_core->instruction >> 7) & 0x1F));
  *next_subcode = rv32_core->func3;
}

static void B_type_preparation(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
  rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);
  rv32_core->rs1 = ((rv32_core->instruction >> 15) & 0x1F);
  rv32_core->rs2 = ((rv32_core->instruction >> 20) & 0x1F);
  rv32_core->jump_offset=((extract32(rv32_core->instruction, 8, 4) << 1) |
                          (extract32(rv32_core->instruction, 25, 6) << 5) |
                          (extract32(rv32_core->instruction, 7, 1) << 11));

  if((1<<11) & rv32_core->jump_offset) rv32_core->jump_offset=(rv32_core->jump_offset | 0xFFFFF000);

  *next_subcode = rv32_core->func3;
}

static void U_type_preparation(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
  rv32_core->immediate = ((rv32_core->instruction >> 12) & 0xFFFFF);
  *next_subcode = -1;
}

static void J_type_preparation(rv32_core_td *rv32_core, int32_t *next_subcode)
{
  rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
  rv32_core->jump_offset=((extract32(rv32_core->instruction, 21, 10) << 1) |
                          (extract32(rv32_core->instruction, 20, 1) << 11) |
                          (extract32(rv32_core->instruction, 12, 8) << 12) );
  /* sign extend the 20 bit number */
  if((1<<19) & rv32_core->jump_offset) rv32_core->jump_offset=(rv32_core->jump_offset | 0xFFF00000);

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
  { FUNC3_INSTR_SW, NULL, instr_SW, NULL}
};
INIT_INSTRUCTION_LIST_DESC(SB_SH_SW_func3_subcode_list);

static instruction_hook_td ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list[] = {
  { FUNC3_INSTR_ADDI, NULL, instr_ADDI, NULL},
  { FUNC3_INSTR_SLTI, NULL, instr_SLTI, NULL},
  { FUNC3_INSTR_SLTIU, NULL, instr_SLTIU, NULL},
  { FUNC3_INSTR_XORI, NULL, instr_XORI, NULL},
  { FUNC3_INSTR_ORI, NULL, instr_ORI, NULL},
  { FUNC3_INSTR_ANDI, NULL, instr_ANDI, NULL},
  { FUNC3_INSTR_SLLI, NULL, instr_SLLI, NULL},
  { FUNC3_INSTR_SRLI_SRAI, NULL, instr_SRLI_SRAI, NULL}
};
INIT_INSTRUCTION_LIST_DESC(ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI_func3_subcode_list);

static instruction_hook_td ADD_SUB_func7_subcode_list[] = {
  { FUNC7_INSTR_ADD, NULL, instr_ADD, NULL},
  { FUNC7_INSTR_SUB, NULL, instr_SUB, NULL}
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_func7_subcode_list);

static instruction_hook_td SRL_SRA_func7_subcode_list[] = {
  { FUNC7_INSTR_SRL, NULL, instr_SRL, NULL},
  { FUNC7_INSTR_SRA, NULL, instr_SRA, NULL}
};
INIT_INSTRUCTION_LIST_DESC(SRL_SRA_func7_subcode_list);

static instruction_hook_td ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list[] = {
  { FUNC3_INSTR_ADD_SUB, R_type_preparation_func7, NULL, &ADD_SUB_func7_subcode_list_desc},
  { FUNC3_INSTR_SLL, NULL, instr_SLL, NULL},
  { FUNC3_INSTR_SLT, NULL, instr_SLT, NULL},
  { FUNC3_INSTR_SLTU, NULL, instr_SLTU, NULL},
  { FUNC3_INSTR_XOR, NULL, instr_XOR, NULL},
  { FUNC3_INSTR_SRL_SRA, R_type_preparation_func7, NULL, &SRL_SRA_func7_subcode_list_desc},
  { FUNC3_INSTR_OR, NULL, instr_OR, NULL},
  { FUNC3_INSTR_AND, NULL, instr_AND, NULL}
};
INIT_INSTRUCTION_LIST_DESC(ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND_func3_subcode_list);

static instruction_hook_td RV32I_opcode_list[] = {
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
};
INIT_INSTRUCTION_LIST_DESC(RV32I_opcode_list);


static void rv32_call_from_opcode_list(rv32_core_td *rv32_core, instruction_desc_td *opcode_list_desc, uint32_t opcode)
{
  uint32_t opcode_index = 0;
  int32_t next_subcode = -1;

  uint32_t list_size = opcode_list_desc->instruction_hook_list_size;
  instruction_hook_td *opcode_list = opcode_list_desc->instruction_hook_list;

  for(opcode_index=0;opcode_index<list_size;opcode_index++)
  {
    if(opcode_list[opcode_index].opcode == opcode)
      break;
  }

  if(opcode_index == list_size) die(rv32_core->instruction);

  if(opcode_list[opcode_index].preparation_cb != NULL)
    opcode_list[opcode_index].preparation_cb(rv32_core, &next_subcode);

  if(opcode_list[opcode_index].execution_cb != NULL)
    rv32_core->execute_cb = opcode_list[opcode_index].execution_cb;

  if((next_subcode != -1) && (opcode_list[opcode_index].next != NULL))
    rv32_call_from_opcode_list(rv32_core, opcode_list[opcode_index].next, next_subcode);
}

uint32_t rv32_core_fetch(rv32_core_td *rv32_core)
{
  uint32_t addr = rv32_core->pc;

  rv32_core->instruction = rv32_core->read_mem(rv32_core->priv, addr);

  /* increase program counter here */
  rv32_core->pc += 4;

  return 0;
}

uint32_t rv32_core_decode(rv32_core_td *rv32_core)
{
  rv32_core->opcode = (rv32_core->instruction & 0x7F);
  rv32_core->rd = 0;
  rv32_core->rs1 = 0;
  rv32_core->rs2 = 0;
  rv32_core->func3 = 0;
  rv32_core->func7 = 0;
  rv32_core->immediate = 0;
  rv32_core->jump_offset = 0;

  rv32_call_from_opcode_list(rv32_core, &RV32I_opcode_list_desc, rv32_core->opcode);

  return 0;
}

uint32_t rv32_core_execute(rv32_core_td *rv32_core)
{
  rv32_core->execute_cb(rv32_core);

  /* clear x0 if any instruction has written into it */
  rv32_core->x[0] = 0;

  return 0;
}

void rv32_core_run(rv32_core_td *rv32_core)
{
  rv32_core_fetch(rv32_core);
  rv32_core_decode(rv32_core);
  rv32_core_execute(rv32_core);
}

void rv32_core_reg_dump_before_exec(rv32_core_td *rv32_core)
{
  int i = 0;

  for(i=0;i<NR_RV32I_REGS;i++)
  {
    printf("x[%d]: %x\n", i, rv32_core->x[i]);
  }
  printf("pc: %x\n", rv32_core->pc);
}

void rv32_core_reg_internal_after_exec(rv32_core_td *rv32_core)
{
  printf("internal regs after execution:\n");
  printf("instruction: %x\n", rv32_core->instruction);
  printf("rd: %x rs1: %x rs2: %x imm: %x\n", rv32_core->rd, rv32_core->rs1, rv32_core->rs2, rv32_core->immediate);
  printf("func3: %x func7: %x jump_offset %x\n", rv32_core->func3, rv32_core->func7, rv32_core->jump_offset);
  printf("next pc: %x\n", rv32_core->pc);
  printf("\n");
}

void rv32_core_init(rv32_core_td *rv32_core,
                    void *priv,
                    read_mem_func read_mem,
                    write_mem_func write_mem
                   )
{
  memset(rv32_core, 0, sizeof(rv32_core_td));
  rv32_core->pc = PROGRAM_COUNTER_START_VAL;
  rv32_core->x[XREG_STACK_POINTER] = STACK_POINTER_START_VAL;

  rv32_core->priv = priv;
  rv32_core->read_mem = read_mem;
  rv32_core->write_mem = write_mem;
}

typedef struct rv32_soc_struct
{
  rv32_core_td rv32_core;
  uint32_t ram[NR_RAM_WORDS];
  uint32_t rom[NR_ROM_WORDS];

} rv32_soc_td;

uint32_t rv32_soc_read_mem(rv32_soc_td *rv32_soc, uint32_t address)
{
  uint8_t align_offset = address & 0x3;
  uint32_t read_val = 0;
  uint32_t read_val2 = 0;
  uint32_t return_val = 0;

  if(address < 0x100000)
  {
    read_val = rv32_soc->ram[address >> 2];
    if(align_offset)
      read_val2 = rv32_soc->ram[(address >> 2) + 1];
  }
  else if((address >= 0x100000) && (address <= 0x200000))
  {
    read_val = rv32_soc->rom[(address-0x100000) >> 2];
    if(align_offset)
      read_val2 = rv32_soc->ram[((address-0x100000) >> 2) + 1];
  }

  switch(align_offset)
  {
    case 1:
      return_val = (read_val2 << 24) | (read_val >> 8);
      break;
    case 2:
      return_val = (read_val2 << 16) | (read_val >> 16);
      break;
    case 3:
      return_val = (read_val2 << 8) | (read_val >> 24);
      break;
    default:
      return_val = read_val;
      break;
  }

  return return_val;
}

void rv32_soc_write_mem(rv32_soc_td *rv32_soc, uint32_t address, uint32_t value, uint8_t nr_bytes)
{
  uint8_t align_offset = address & 0x3;
  uint32_t address_for_write = 0;
  uint8_t *ptr_address = NULL;

  printf("writing to value %x to address %x\n", value, address >> 2);
  if(address < 0x100000)
  {
    address_for_write = address >> 2;
    ptr_address = (uint8_t *)&rv32_soc->ram[address_for_write];

  }
  else if((address >= 0x100000) && (address <= 0x200000))
  {
    address_for_write = (address-0x100000) >> 2;
    ptr_address = (uint8_t *)&rv32_soc->rom[address_for_write];
  }

  memcpy(ptr_address+align_offset, &value, nr_bytes);
  printf("NEW VAL in address: %x %x\n", address, rv32_soc->rom[address_for_write]);

  return;
}

void rv32_soc_init(rv32_soc_td *rv32_soc, char *rom_file_name)
{
  FILE * p_rom_file = NULL;
  unsigned long lsize = 0;
  size_t result = 0;

  p_rom_file = fopen(rom_file_name, "rb");
  if(p_rom_file == NULL)
  {
    printf("Could not open rom file!\n");
    exit(-1);
  }

  fseek(p_rom_file, 0, SEEK_END);
  lsize = ftell(p_rom_file);
  rewind(p_rom_file);

  if(lsize > sizeof(rv32_soc->rom))
  {
    printf("Not able to load rom file of size %lu, rom space is %lu\n", lsize, sizeof(rv32_soc->rom));
    exit(-2);
  }

  memset(rv32_soc, 0, sizeof(rv32_soc_td));

  rv32_core_init(&rv32_soc->rv32_core, rv32_soc,(read_mem_func)rv32_soc_read_mem,(write_mem_func)rv32_soc_write_mem);

  result = fread(&rv32_soc->rom, sizeof(char), lsize, p_rom_file);
  if(result != lsize)
  {
    printf("Error while reading file!\n");
    exit(-3);
  }

  uint32_t i = 0;
  printf("RV32 ROM contents\n");
  for(i=0;i<lsize/sizeof(uint32_t);i++)
  {
    printf("%x\n", rv32_soc->rom[i]);
  }

  fclose(p_rom_file);

  printf("RV32 SOC initialized!\n");
}

int main(int argc, char *argv[])
{
  if(argc < 2)
  {
    printf("please specify a rom file!\n");
    exit(-1);
  }

  rv32_soc_td rv32_soc;
  rv32_soc_init(&rv32_soc, argv[1]);

  while(1)
  {
    rv32_core_reg_dump_before_exec(&rv32_soc.rv32_core);
    rv32_core_run(&rv32_soc.rv32_core);
    rv32_core_reg_internal_after_exec(&rv32_soc.rv32_core);
  }
}
