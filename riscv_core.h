#ifndef RISCV_CORE_H
#define RISCV_CORE_H

#include <riscv_types.h>
#include <riscv_csr.h>

#define NR_RVI_REGS 32

typedef struct rv_core_struct rv_core_td;
typedef struct rv_core_struct
{
    privilege_level curr_priv_mode;

    /* Registers */
    rv_uint_xlen x[NR_RVI_REGS];
    rv_uint_xlen pc;
    rv_uint_xlen next_pc;

    uint32_t instruction;
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t func3;
    uint8_t func7;
    uint16_t func12;
    rv_uint_xlen immediate;
    rv_uint_xlen jump_offset;

    /* points to the next instruction */
    void (*execute_cb)(rv_core_td *rv_core);

    /* externally hooked */
    void *priv;
    rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address);
    void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes);

    csr_reg_desc_td *csr_table;

} rv_core_td;

rv_uint_xlen rv_core_fetch(rv_core_td *rv_core);
rv_uint_xlen rv_core_decode(rv_core_td *rv_core);
rv_uint_xlen rv_core_execute(rv_core_td *rv_core);
void rv_core_run(rv_core_td *rv_core);
void rv_core_reg_dump(rv_core_td *rv_core);
void rv_core_reg_internal_after_exec(rv_core_td *rv_core);
void rv_core_init(rv_core_td *rv_core,
                  void *priv,
                  rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address),
                  void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes),
                  csr_reg_desc_td *csr_table
                  );

typedef struct instruction_hook_struct
{
    void (*preparation_cb)(rv_core_td *rv_core, int32_t *next_subcode);
    void (*execution_cb)(rv_core_td *rv_core_data);
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

#endif /* RISCV_CORE_H */