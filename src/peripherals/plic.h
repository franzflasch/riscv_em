#ifndef RISCV_PLIC_H
#define RISCV_PLIC_H

#define NR_PRIO_MEM_REGS 256
#define NR_PENDING_REGS 8
#define NR_ENABLE_REGS 8
#define NR_CLAIMED_BITS_REGS 8

typedef struct plic_struct
{
    /* 0x0C00 0000 - 0x0C00 0400
     * priority reg 0 is reserved 
     * interrupt source id[0] refers to
     * priority 1
     * 7 priority levels
     * the higher the level the higher the priority
     * if two ids have same prio, the lower interrupt id has a higher prio
     * Addressing: BASE_ADDRESS + 4*Interrupt ID
     * 
     * Bits Field Name Description
     * [2:0] Priority Sets the priority for a given global interrupt.
     * [31:3] Reserved WIRI
     */
    uint32_t priority[NR_PRIO_MEM_REGS];

    /* bits correspronding to the interrupt ids */
    uint32_t pending_bits[NR_PENDING_REGS];

    /* bits correspronding to the interrupt ids */
    uint32_t enable_bits[NR_ENABLE_REGS];

    /*
     * Interrupts with a lower prio setting than threshold will be masked out
     * Bits Field Name Description
     * [2:0] Threshold Sets the priority threshold for the E31 Coreplex.
     * [31:3] Reserved WIRI
    */
    uint32_t priority_threshold;

    uint32_t claim_complete;


    /* internal */
    uint32_t claimed_bits[NR_CLAIMED_BITS_REGS];

} plic_td;

void plic_add_pending(plic_td *plic, uint32_t interrupt_id, uint8_t pending);
uint8_t plic_update(plic_td *plic);
int plic_write_reg(void *priv, rv_uint_xlen address, rv_uint_xlen val, uint8_t nr_bytes);
int plic_read_reg(void *priv, rv_uint_xlen address, rv_uint_xlen *out_val);

#endif /* RISCV_PLIC_H */