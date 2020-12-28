#ifndef RISCV_CORE_H
#define RISCV_CORE_H

#include <riscv_types.h>

#include <csr.h>
#include <clint.h>

#define NR_RVI_REGS 32

#define RV_CORE_E_OK 0
#define RV_CORE_E_ERR 1

#define RV_CORE_INSTANTIATE_CSR_REGS_FOR_CORE(_name) \
    static csr_reg_td _name[] = { \
        /* Machine Information Registers */ \
        { CSR_ADDR_MVENDORID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO }, \
        { CSR_ADDR_MARCHID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO }, \
        { CSR_ADDR_MIMPID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO }, \
        { CSR_ADDR_MHARTID, CSR_ACCESS_RO(machine_mode), 0, CSR_MASK_ZERO }, \
        /* Machine Trap Setup */ \
        { CSR_ADDR_MSTATUS, CSR_ACCESS_RW(machine_mode), 0, CSR_MSTATUS_WR_MASK }, \
        { CSR_ADDR_MISA, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_ZERO }, \
        { CSR_ADDR_MEDELEG, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_ZERO }, \
        { CSR_ADDR_MIDELEG, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_ZERO }, \
        { CSR_ADDR_MIE, CSR_ACCESS_RW(machine_mode), 0, CSR_MIP_MIE_WR_MASK }, \
        { CSR_ADDR_MTVEC, CSR_ACCESS_RW(machine_mode), 0, CSR_MTVEC_WR_MASK }, \
        /* Machine Trap Handling */ \
        { CSR_ADDR_MSCRATCH, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_ADDR_MEPC, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_ADDR_MCAUSE, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_ADDR_MTVAL, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_ADDR_MIP, CSR_ACCESS_RW(machine_mode), 0, CSR_MIP_MIE_WR_MASK }, \
        /* Machine Protection and Translation */ \
        { CSR_PMPCFG0, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPCFG1, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPCFG2, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPCFG3, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR0, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR1, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR2, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR3, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR4, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR5, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR6, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR7, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR8, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR9, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR10, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR11, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR12, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR13, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR14, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
        { CSR_PMPADDR15, CSR_ACCESS_RW(machine_mode), 0, CSR_MASK_WR_ALL }, \
    }; \
    INIT_CSR_REG_DESC(_name);

typedef struct rv_core_struct rv_core_td;
typedef struct rv_core_struct
{
    privilege_level curr_priv_mode;
    uint64_t curr_cycle;

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
    uint8_t func6;
    uint8_t func5;
    uint16_t func12;
    rv_uint_xlen immediate;
    rv_uint_xlen jump_offset;

    uint8_t sync_trap_pending;
    rv_uint_xlen sync_trap_cause;

    /* points to the next instruction */
    void (*execute_cb)(rv_core_td *rv_core);

    /* externally hooked */
    void *priv;
    rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address, int *err);
    void (*write_mem)(void *priv, rv_uint_xlen address, rv_uint_xlen value, uint8_t nr_bytes);

    csr_reg_desc_td *csr_table;
    /* for fast access */
    rv_uint_xlen *mstatus;
    rv_uint_xlen *mcause;
    rv_uint_xlen *mepc;
    rv_uint_xlen *mtvec;
    rv_uint_xlen *mie;
    rv_uint_xlen *mip;

    int lr_valid;
    rv_uint_xlen lr_address;

} rv_core_td;

void rv_core_run(rv_core_td *rv_core);
void rv_core_process_interrupts(rv_core_td *rv_core, uint8_t mei, uint8_t msi, uint8_t mti);
void rv_core_reg_dump(rv_core_td *rv_core);
void rv_core_reg_internal_after_exec(rv_core_td *rv_core);
void rv_core_init(rv_core_td *rv_core,
                  void *priv,
                  rv_uint_xlen (*read_mem)(void *priv, rv_uint_xlen address, int *err),
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
#define INIT_INSTRUCTION_LIST_DESC(_instruction_list) \
    static instruction_desc_td  _instruction_list##_desc = \
    { sizeof(_instruction_list)/sizeof(_instruction_list[0]), _instruction_list }

#endif /* RISCV_CORE_H */