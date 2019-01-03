#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NR_RV32I_REGS 32
#define XREG_ZERO 0
#define XREG_RETURN_ADDRESS 0
#define XREG_STACK_POINTER 2

#define NR_RAM_WORDS 128
#define NR_ROM_WORDS 128

#define STACK_POINTER_START_VAL (4*NR_RAM_WORDS)
#define PROGRAM_COUNTER_START_VAL 0x100000

/* U-Type Instructions */
#define INSTR_LUI 0x37   /* LOAD UPPER IMMEDIATE INTO DESTINATION REGISTER */
#define INSTR_AUIPC 0x17 /* ADD UPPER IMMEDIATE TO PROGRAM COUNTER */

/* UJ-Type Instructions */
#define INSTR_JAL 0x6F   /* JUMP and Link */

#define INSTR_ADDI 0x13  /* Add immediate to rs and store to rd */

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
  uint32_t immediate;
  int32_t jump_offset;

  /* points to the next instruction */
  void (*execute_cb)(void *rv32_core);

  /* externally hooked */
  void *priv;
  uint32_t (*read_mem)(void *priv, uint32_t address);
  void (*write_mem)(void *priv, uint32_t address, uint32_t value);

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

  printf("JUMP ADDRESS: %x\n", rv32_core->jump_offset);
}

static void instr_ADDI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = ((rv32_core->immediate + rv32_core->rs) & 0xFFFFFFFF);
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
    case INSTR_ADDI:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->rs = ((rv32_core->instruction >> 15) & 0x1F);
      rv32_core->immediate = ((rv32_core->instruction >> 20) & 0xFFF);
      rv32_core->execute_cb = instr_ADDI;
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

  return 0;
}

uint32_t rv32_core_mem_access(rv32_core_td *rv32_core)
{
  return 0;
}

uint32_t rv32_core_write_back(rv32_core_td *rv32_core)
{
  return 0;
}

void rv32_core_run(rv32_core_td *rv32_core)
{
  rv32_core_fetch(rv32_core);
  rv32_core_decode(rv32_core);
  rv32_core_execute(rv32_core);
  rv32_core_mem_access(rv32_core);
  rv32_core_write_back(rv32_core);
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
                    void *read_mem,
                    void *write_mem
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
  if((address >= 0x0) && (address < 0x100000))
    return rv32_soc->ram[address >> 2];
  else if((address >= 0x100000) && (address <= 0x200000))
    return rv32_soc->rom[(address-0x100000) >> 2];
}

void rv32_soc_write_mem(rv32_soc_td *rv32_soc, uint32_t address, uint32_t value)
{
  rv32_soc->ram[address] = value;
}

uint32_t test_instructions[] = 
{
0x00000093,
0x00000193,
0x00000213,
0x00000293,
0x00000313,
0x00000393,
0x00000413,
0x00000493,
0x00000513,
0x00000593,
0x02c000ef,
0x00000613,
0x00000693,
0x00000713,
0x00000793,
0x00000813,
0x00000893,
0x00000913,
0x00000993,
0x00000a13,
0x00000a93,
0x00000b13,
0x00000b93,
0x00000c13,
0x00000c93,
0x00000d13,
0x00000d93,
0x00000e13,
0x00000e93,
0x00000f13,
0x00000f93,
0xfd9ff0ef,
};


void rv32_soc_init(rv32_soc_td *rv32_soc)
{
  memset(rv32_soc, 0, sizeof(rv32_soc_td));

  rv32_core_init(&rv32_soc->rv32_core, rv32_soc, rv32_soc_read_mem, rv32_soc_write_mem);

  memcpy(rv32_soc->rom, test_instructions, sizeof(test_instructions));

  int i = 0;

  printf("RV32 ROM contents\n");
  for(i=0;i<(sizeof(test_instructions)/sizeof(test_instructions[0]));i++)
  {
    printf("%x\n", rv32_soc->rom[i]);
  }

  printf("RV32 SOC initialized!\n");
}

int main(int argc, char *argv[])
{
  rv32_soc_td rv32_soc;

  rv32_soc_init(&rv32_soc);

  while(1)
  {
    rv32_core_run(&rv32_soc.rv32_core);
    rv32_core_reg_dump(&rv32_soc.rv32_core);
  }
}
