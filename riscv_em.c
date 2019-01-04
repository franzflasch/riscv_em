#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NR_RV32I_REGS 32
#define XREG_ZERO 0
#define XREG_RETURN_ADDRESS 0
#define XREG_STACK_POINTER 2

#define NR_RAM_WORDS 512 
#define NR_ROM_WORDS 512

#define STACK_POINTER_START_VAL (4*NR_RAM_WORDS)
#define PROGRAM_COUNTER_START_VAL 0x100000

/* U-Type Instructions */
#define INSTR_LUI 0x37   /* LOAD UPPER IMMEDIATE INTO DESTINATION REGISTER */
#define INSTR_AUIPC 0x17 /* ADD UPPER IMMEDIATE TO PROGRAM COUNTER */

/* UJ-Type Instructions */
#define INSTR_JAL 0x6F   /* JUMP and Link */

/* I-Type Instructions */
#define INSTR_JALR 0x67
#define INSTR_ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI 0x13 
#define FUNC3_INSTR_ADDI  0x0
#define FUNC3_INSTR_SLTI  0x2
#define FUNC3_INSTR_SLTIU 0x3
#define FUNC3_INSTR_XORI  0x4
#define FUNC3_INSTR_ORI   0x6 
#define FUNC3_INSTR_ANDI  0x7

#define FUNC3_INSTR_SLLI  0x1
#define FUNC7_INSTR_SLLI  0x00

#define FUNC3_INSTR_SRLI_SRAI  0x5
#define FUNC7_INSTR_SRLI  0x00
#define FUNC7_INSTR_SRAI  0x20

/* B-Type Instructions */
#define INSTR_BEQ_BNE_BLT_BGE_BLTU_BGEU 0x63
#define FUNC3_INSTR_BNE 0x1


typedef uint32_t (*read_mem_func)(void *priv, uint32_t address);
typedef void (*write_mem_func)(void *priv, uint32_t address, uint32_t value);

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
  uint8_t rs;
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
  rv32_core->x[rv32_core->rd] = rv32_core->pc + (rv32_core->immediate << 12);
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
  rv32_core->x[rv32_core->rd] = rv32_core->pc;

  rv32_core->pc = (rv32_core->x[rv32_core->rs] + rv32_core->jump_offset);
  rv32_core->pc &= ~(1<<0);
}

static void instr_BNE(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  if(rv32_core->x[rv32_core->rs] != rv32_core->x[rv32_core->rs2])
    rv32_core->pc = (rv32_core->pc-4) + rv32_core->jump_offset;
}

static void instr_ADDI(void *rv32_core_data)
{
  int32_t signed_immediate = 0;
  int32_t signed_rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) signed_immediate = (rv32_core->immediate | 0xFFFFF000);
  else signed_immediate = rv32_core->immediate;

  signed_rs_val = rv32_core->x[rv32_core->rs];

  rv32_core->x[rv32_core->rd] = (signed_immediate + signed_rs_val);
}

static void instr_SLTI(void *rv32_core_data)
{
  int32_t signed_immediate = 0;
  int32_t signed_rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) signed_immediate = (rv32_core->immediate | 0xFFFFF000);
  else signed_immediate = rv32_core->immediate;

  signed_rs_val = rv32_core->x[rv32_core->rs];

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

  unsigned_rs_val = rv32_core->x[rv32_core->rs];

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
    rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] ^ 1;
  else
    rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] ^ rv32_core->x[rv32_core->immediate];
}

static void instr_ORI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate=(rv32_core->immediate | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] | rv32_core->x[rv32_core->immediate];
}

static void instr_ANDI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate=(rv32_core->immediate | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] & rv32_core->x[rv32_core->immediate];
}

uint32_t rv32_core_fetch(rv32_core_td *rv32_core)
{
  uint32_t addr = rv32_core->pc;

  rv32_core->instruction = rv32_core->read_mem(rv32_core->priv, addr);

  printf("fetching instruction: %x\n", rv32_core->instruction);

  /* increase program counter here */
  rv32_core->pc += 4;

  return 0;
}

uint32_t rv32_core_decode(rv32_core_td *rv32_core)
{
  rv32_core->opcode = (rv32_core->instruction & 0x7F);
  rv32_core->rd = 0;
  rv32_core->rs = 0;
  rv32_core->rs2 = 0;
  rv32_core->func3 = 0;
  rv32_core->func7 = 0;
  rv32_core->immediate = 0;
  rv32_core->jump_offset = 0;

  switch(rv32_core->opcode)
  {
    case INSTR_LUI:
      /* get destination register */
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      /* get immediate value */
      rv32_core->immediate = ((rv32_core->instruction >> 12) & 0xFFFFF);
      /* set instruction callback */
      rv32_core->execute_cb = instr_LUI;
      break;
    case INSTR_AUIPC:
       /* get destination register */
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      /* get immediate value */
      rv32_core->immediate = ((rv32_core->instruction >> 12) & 0xFFFFF);
      /* set instruction callback */
      rv32_core->execute_cb = instr_AUIPC; 
      break;
    case INSTR_JAL:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->jump_offset=((extract32(rv32_core->instruction, 21, 10) << 1) | 
                              (extract32(rv32_core->instruction, 20, 1) << 11) | 
                              (extract32(rv32_core->instruction, 12, 8) << 12) );
      /* sign extend the 20 bit number */
      if((1<<19) & rv32_core->jump_offset) rv32_core->jump_offset=(rv32_core->jump_offset | 0xFFF00000);
      rv32_core->execute_cb = instr_JAL;
      break;
    case INSTR_JALR:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->rs = ((rv32_core->instruction >> 15) & 0x1F);
      rv32_core->jump_offset = ((rv32_core->instruction >> 20) & 0xFFF);
      if((1<<11) & rv32_core->jump_offset) rv32_core->jump_offset=(rv32_core->jump_offset | 0xFFFFF000);
      rv32_core->execute_cb = instr_JALR;
      break;
    case INSTR_ADDI_SLTI_SLTIU_XORI_ORI_ANDI_SLLI_SRLI_SRAI:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x3);
      rv32_core->rs = ((rv32_core->instruction >> 15) & 0x1F);
      rv32_core->immediate = ((rv32_core->instruction >> 20) & 0xFFF);
  
      switch(rv32_core->func3)
      {
        case FUNC3_INSTR_ADDI:
          rv32_core->execute_cb = instr_ADDI;
          break;
        case FUNC3_INSTR_SLTI:
          rv32_core->execute_cb = instr_SLTI;
          break;
        case FUNC3_INSTR_SLTIU:
          rv32_core->execute_cb = instr_SLTIU;
          break;
        case FUNC3_INSTR_XORI:
          rv32_core->execute_cb = instr_XORI;
          break;
        case FUNC3_INSTR_ORI:
          rv32_core->execute_cb = instr_ORI;
          break;
        case FUNC3_INSTR_ANDI:
          rv32_core->execute_cb = instr_ANDI;
          break;
        case FUNC3_INSTR_SLLI:
          rv32_core->func7 = ((rv32_core->instruction >> 25) & 0x7);
//          if(rv32_core->func7 == FUNC7_INSTR_SLLI) rv32_core->execute_cb = instr_SLLI;
          break;
        case FUNC3_INSTR_SRLI_SRAI:
          rv32_core->func7 = ((rv32_core->instruction >> 25) & 0x7);
//          if(rv32_core->func7 == FUNC7_INSTR_SRLI) rv32_core->execute_cb = instr_SRLI;
//          else if(rv32_core->func7 == FUNC7_INSTR_SRAI) rv32_core->execute_cb = instr_SRLI;
          break;
      }
      break;
    case INSTR_BEQ_BNE_BLT_BGE_BLTU_BGEU:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x3);     
      rv32_core->rs = ((rv32_core->instruction >> 15) & 0x1F);
      rv32_core->rs2 = ((rv32_core->instruction >> 20) & 0x1F);
      rv32_core->jump_offset=((extract32(rv32_core->instruction, 8, 4) << 1) | 
                              (extract32(rv32_core->instruction, 25, 6) << 5) | 
                              (extract32(rv32_core->instruction, 7, 1) << 11));
      if((1<<11) & rv32_core->jump_offset) rv32_core->jump_offset=(rv32_core->jump_offset | 0xFFFFF000);

      switch(rv32_core->func3)
      {
        case FUNC3_INSTR_BNE:
          rv32_core->execute_cb = instr_BNE;
          break;
      }
      break;
    default:
      printf("Unknown opcode! %x\n", rv32_core->opcode);
      exit(-1);
      break;
  }

  return 0;
}

uint32_t rv32_core_execute(rv32_core_td *rv32_core)
{
  rv32_core->execute_cb(rv32_core);
  
  /* clear x0 if any instruction has written into it */
  rv32_core->x[0] = 0;

  return 0;
}

/*
uint32_t rv32_core_mem_access(rv32_core_td *rv32_core)
{
  return 0;
}

uint32_t rv32_core_write_back(rv32_core_td *rv32_core)
{
  return 0;
}
*/

void rv32_core_run(rv32_core_td *rv32_core)
{
  rv32_core_fetch(rv32_core);
  rv32_core_decode(rv32_core);
  rv32_core_execute(rv32_core);

/*
  rv32_core_mem_access(rv32_core);
  rv32_core_write_back(rv32_core);
*/
}

void rv32_core_reg_dump(rv32_core_td *rv32_core)
{
  int i = 0;

  for(i=0;i<NR_RV32I_REGS;i++)
  {
    printf("x[%d]: %x\n", i, rv32_core->x[i]);
  }
  printf("pc: %x\n", rv32_core->pc);
  printf("instruction: %x\n", rv32_core->instruction);
  printf("rd: %x rs: %x imm: %x jump_offs: %x\n", rv32_core->rd, rv32_core->rs, rv32_core->immediate, rv32_core->jump_offset);
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
  if(address < 0x100000)
    return rv32_soc->ram[address >> 2];
  else if((address >= 0x100000) && (address <= 0x200000))
    return rv32_soc->rom[(address-0x100000) >> 2];

  else return 0;
}

void rv32_soc_write_mem(rv32_soc_td *rv32_soc, uint32_t address, uint32_t value)
{
  rv32_soc->ram[address] = value;
}

uint32_t test_instructions[] = 
{
0x008000ef,
0x04200f93,
0x00000093,
0x00008f13,
0x00000e93,
0x00200193,
0xffdf16e3,
0x00100093,
0x00108f13,
0x00200e93,
0x00300193,
0xfddf1ce3,
0x00300093,
0x00708f13,
0x00a00e93,
0x00400193,
0xfddf12e3,
0x00000093,
0x80008f13,
0x80000e93,
0x00500193,
0xfbdf18e3,
0x800000b7,
0x00008f13,
0x80000eb7,
0x00600193,
0xf9df1ee3,
0x800000b7,
0x80008f13,
0x80000eb7,
0x800e8e93,
0x00700193,
0xf9df12e3,
0x00000093,
0x7ff08f13,
0x7ff00e93,
0x00800193,
0xf7df18e3,
0x800000b7,
0xfff08093,
0x00008f13,
0x80000eb7,
0xfffe8e93,
0x00900193,
0xf5df1ae3,
0x800000b7,
0xfff08093,
0x7ff08f13,
0x80000eb7,
0x7fee8e93,
0x00a00193,
0xf3df1ce3,
0x800000b7,
0x7ff08f13,
0x80000eb7,
0x7ffe8e93,
0x00b00193,
0xf3df10e3,
0x800000b7,
0xfff08093,
0x80008f13,
0x7ffffeb7,
0x7ffe8e93,
0x00c00193,
0xf1df12e3,
0x00000093,
0xfff08f13,
0xfff00e93,
0x00d00193,
0xefdf18e3,
0xfff00093,
0x00108f13,
0x00000e93,
0x00e00193,
0xeddf1ee3,
0xfff00093,
0xfff08f13,
0xffe00e93,
0x00f00193,
0xeddf14e3,
0x800000b7,
0xfff08093,
0x00108f13,
0x80000eb7,
0x01000193,
0xebdf18e3,
0x00d00093,
0x00b08093,
0x01800e93,
0x01100193,
0xe9d09ee3,
0x00000213,
0x00d00093,
0x00b08f13,
0x000f0313,
0x00120213,
0x00200293,
0xfe5216e3,
0x01800e93,
0x01200193,
0xe7d31ae3,
0x00000213,
0x00d00093,
0x00a08f13,
0x00000013,
0x000f0313,
0x00120213,
0x00200293,
0xfe5214e3,
0x01700e93,
0x01300193,
0xe5d314e3,
0x00000213,
0x00d00093,
0x00908f13,
0x00000013,
0x00000013,
0x000f0313,
0x00120213,
0x00200293,
0xfe5212e3,
0x01600e93,
0x01400193,
0xe1d31ce3,
0x00000213,
0x00d00093,
0x00b08f13,
0x00120213,
0x00200293,
0xfe5218e3,
0x01800e93,
0x01500193,
0xdfdf1ae3,
0x00000213,
0x00d00093,
0x00000013,
0x00a08f13,
0x00120213,
0x00200293,
0xfe5216e3,
0x01700e93,
0x01600193,
0xdddf16e3,
0x00000213,
0x00d00093,
0x00000013,
0x00000013,
0x00908f13,
0x00120213,
0x00200293,
0xfe5214e3,
0x01600e93,
0x01700193,
0xdbdf10e3,
0x02000093,
0x02000e93,
0x01800193,
0xd9d098e3,
0x02100093,
0x03208013,
0x00000e93,
0x01900193,
0xd7d01ee3,

};


void rv32_soc_init(rv32_soc_td *rv32_soc)
{
  memset(rv32_soc, 0, sizeof(rv32_soc_td));

  rv32_core_init(&rv32_soc->rv32_core, rv32_soc,(read_mem_func)rv32_soc_read_mem,(write_mem_func)rv32_soc_write_mem);

  memcpy(rv32_soc->rom, test_instructions, sizeof(test_instructions));

  uint32_t i = 0;

  printf("RV32 ROM contents\n");
  for(i=0;i<(sizeof(test_instructions)/sizeof(test_instructions[0]));i++)
  {
    printf("%x\n", rv32_soc->rom[i]);
  }

  printf("RV32 SOC initialized!\n");
}

int main(int argc, char *argv[])
{
  (void) argc;
  (void) argv;

  rv32_soc_td rv32_soc;

  rv32_soc_init(&rv32_soc);

  while(1)
  {
    rv32_core_run(&rv32_soc.rv32_core);
    rv32_core_reg_dump(&rv32_soc.rv32_core);
  }
}
