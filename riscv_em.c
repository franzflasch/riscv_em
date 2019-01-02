#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define NR_RV32I_REGS 32
#define XREG_ZERO 0
#define XREG_RETURN_ADDRESS 0
#define XREG_STACK_POINTER 2

#define NR_RAM_WORDS 128
#define NR_ROM_WORDS 128

#define STACK_POINTER_START_VAL (4*NR_RAM_WORDS)
#define PROGRAM_COUNTER_START_VAL 0x00000000

/* U-Type Instructions */
#define INSTR_LUI 0x37   /* LOAD UPPER IMMEDIATE INTO DESTINATION REGISTER */
#define INSTR_AUIPC 0x17 /* ADD UPPER IMMEDIATE TO PROGRAM COUNTER */

/* UJ-Type Instructions */
#define INSTR_JAL 0x6F   /* JUMP and Link */

typedef struct rv32_core_struct
{
  /* Registers */
  uint32_t x[NR_RV32I_REGS];
  uint32_t pc;

  uint32_t instruction;
  uint8_t opcode;
  uint8_t rd;
  uint32_t immediate;

  /* externally hooked */
  void *priv;
  uint32_t (*read_mem)(void *priv, uint32_t address);
  void (*write_mem)(void *priv, uint32_t address, uint32_t value);

} rv32_core_td;

uint32_t rv32_core_fetch(rv32_core_td *rv32_core)
{
  uint32_t addr = rv32_core->pc;
  uint32_t instruction = 0;

  rv32_core->instruction = rv32_core->read_mem(rv32_core->priv, addr);

  /* increase program counter here */
  rv32_core->pc += 4;

  return 0;
}

uint32_t rv32_core_decode(rv32_core_td *rv32_core)
{
  rv32_core->opcode = (rv32_core->instruction & 0x7F);
  rv32_core->rd = 0;
  rv32_core->immediate = 0;

  switch(rv32_core->opcode)
  {
    case INSTR_LUI:
      /* get destination register */
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      /* get immediate value */
      rv32_core->immediate = ((rv32_core->instruction >> 12) & 0xFFFFF);
    break;
  }

  return 0;
}

uint32_t rv32_core_execute(rv32_core_td *rv32_core)
{
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
  return rv32_soc->ram[address];
}

void rv32_soc_write_mem(rv32_soc_td *rv32_soc, uint32_t address, uint32_t value)
{
  rv32_soc->ram[address] = value;
}

void rv32_soc_init(rv32_soc_td *rv32_soc)
{
  memset(rv32_soc, 0, sizeof(rv32_soc_td));

  rv32_core_init(&rv32_soc->rv32_core, &rv32_soc, rv32_soc_read_mem, rv32_soc_write_mem);

  printf("RV32 SOC initialized!\n");
}

int main(int argc, char *argv[])
{
  rv32_soc_td rv32_soc;

  rv32_soc_init(&rv32_soc);
}
