#ifndef RISCV_CORE_H
#define RISCV_CORE_H

#include <riscv_types.h>

#define NR_RVI_REGS 32

typedef struct rv_core_struct
{
    /* Registers */
    rv_uint_xlen x[NR_RVI_REGS];
    rv_uint_xlen pc;

    uint32_t instruction;
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t func3;
    uint8_t func7;
    rv_uint_xlen immediate;
    rv_uint_xlen jump_offset;

    /* points to the next instruction */
    void (*execute_cb)(void *rv_core);

    /* externally hooked */
    void *priv;
    rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address);
    void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes);

} rv_core_td;

rv_uint_xlen rv_core_fetch(rv_core_td *rv_core);
rv_uint_xlen rv_core_decode(rv_core_td *rv_core);
rv_uint_xlen rv_core_execute(rv_core_td *rv_core);
void rv_core_run(rv_core_td *rv_core);
void rv_core_reg_dump_before_exec(rv_core_td *rv_core);
void rv_core_reg_internal_after_exec(rv_core_td *rv_core);
void rv_core_init(rv_core_td *rv_core,
                    void *priv,
                    rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address),
                    void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes)
                    );

#endif /* RISCV_CORE_H */