#ifndef RISCV_CORE_H
#define RISCV_CORE_H

#define NR_RV32I_REGS 32

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
    uint32_t (*read_mem)(void *priv, uint32_t address);
    void (*write_mem)(void *priv, uint32_t address, uint32_t value, uint8_t nr_bytes);

} rv32_core_td;

uint32_t rv32_core_fetch(rv32_core_td *rv32_core);
uint32_t rv32_core_decode(rv32_core_td *rv32_core);
uint32_t rv32_core_execute(rv32_core_td *rv32_core);
void rv32_core_run(rv32_core_td *rv32_core);
void rv32_core_reg_dump_before_exec(rv32_core_td *rv32_core);
void rv32_core_reg_internal_after_exec(rv32_core_td *rv32_core);
void rv32_core_init(rv32_core_td *rv32_core,
                    void *priv,
                    uint32_t (*read_mem)(void *priv, uint32_t address),
                    void (*write_mem)(void *priv, uint32_t address, uint32_t value, uint8_t nr_bytes)
                    );

#endif /* RISCV_CORE_H */