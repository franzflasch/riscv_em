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

/* I- and R-Type Instructions */
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

#define INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND 0x33

#define FUNC3_INSTR_ADD_SUB 0x0
#define FUNC7_INSTR_ADD 0x00
#define FUNC7_INSTR_SUB 0x20

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
    rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] ^ rv32_core->immediate;
}

static void instr_ORI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate=(rv32_core->immediate | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] | rv32_core->immediate;
}

static void instr_ANDI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  if((1<<11) & rv32_core->immediate) rv32_core->immediate=(rv32_core->immediate | 0xFFFFF000);

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] & rv32_core->immediate;
}

static void instr_SLLI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = (rv32_core->x[rv32_core->rs] << rv32_core->immediate);
}

static void instr_SRAI(void *rv32_core_data)
{
  int32_t rs_val = 0;

  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  /* a right shift on signed ints seem to be always arithmetic */
  rs_val = rv32_core->x[rv32_core->rs];
  rs_val = rs_val >> rv32_core->immediate;
  
  rv32_core->x[rv32_core->rd] = rs_val;
}

static void instr_SRLI(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = (rv32_core->x[rv32_core->rs] >> rv32_core->immediate);
}

static void instr_ADD(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;
  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] + rv32_core->x[rv32_core->rs2];
}

static void instr_SUB(void *rv32_core_data)
{
  rv32_core_td *rv32_core = (rv32_core_td *)rv32_core_data;

  rv32_core->x[rv32_core->rd] = rv32_core->x[rv32_core->rs] - rv32_core->x[rv32_core->rs2];
}

uint32_t rv32_core_fetch(rv32_core_td *rv32_core)
{
  uint32_t addr = rv32_core->pc;

  rv32_core->instruction = rv32_core->read_mem(rv32_core->priv, addr);

  //printf("fetching instruction: %x\n", rv32_core->instruction);

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
      rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);
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
          rv32_core->func7 = ((rv32_core->instruction >> 25) & 0x7F);
          if(rv32_core->func7 == FUNC7_INSTR_SLLI) rv32_core->execute_cb = instr_SLLI;
          break;
        case FUNC3_INSTR_SRLI_SRAI:
          rv32_core->func7 = ((rv32_core->instruction >> 25) & 0x7F);
          if(rv32_core->func7 == FUNC7_INSTR_SRLI) rv32_core->execute_cb = instr_SRLI;
          else if(rv32_core->func7 == FUNC7_INSTR_SRAI) rv32_core->execute_cb = instr_SRAI;
          break;
      }
      break;
    case INSTR_ADD_SUB_SLL_SLT_SLTU_XOR_SRL_SRA_OR_AND:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);
      rv32_core->rs = ((rv32_core->instruction >> 15) & 0x1F);
      rv32_core->rs2 = ((rv32_core->instruction >> 20) & 0x1F);
      rv32_core->func7 = ((rv32_core->instruction >> 25) & 0x7F);

      switch(rv32_core->func3)
      {
        case FUNC3_INSTR_ADD_SUB:
          if(rv32_core->func7 == FUNC7_INSTR_ADD) rv32_core->execute_cb = instr_ADD;
          else if(rv32_core->func7 == FUNC7_INSTR_SUB) rv32_core->execute_cb = instr_SUB; 
          break;
      }
      break;
    case INSTR_BEQ_BNE_BLT_BGE_BLTU_BGEU:
      rv32_core->rd = ((rv32_core->instruction >> 7) & 0x1F);
      rv32_core->func3 = ((rv32_core->instruction >> 12) & 0x7);     
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
  printf("rd: %x rs: %x rs2: %x imm: %x\n", rv32_core->rd, rv32_core->rs, rv32_core->rs2, rv32_core->immediate);
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
  (void) argc;
  (void) argv;

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
