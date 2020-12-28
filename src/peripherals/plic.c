#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <riscv_helper.h>
#include <plic.h>

// #define PLIC_DEBUG
#ifdef PLIC_DEBUG
#define PLIC_DBG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define PLIC_DBG(...) do{ } while ( 0 )
#endif

#define PLIC_MAX_INTERRUPTS 255

#define PLIC_PRIORITY_ADDR_OFFS 0x0
#define PLIC_PRIORITY_SIZE_BYTES 0x400

#define PLIC_PENDING_ADDR_OFFS 0x1000
#define PLIC_PENDING_SIZE_BYTES 0x20

#define PLIC_IRQ_ENABLE_ADDR_OFFS 0x2000
#define PLIC_IRQ_ENABLE_SIZE_BYTES 0x20

#define PLIC_PRIO_THRESH_ADDR_OFFS 0x200000
#define PLIC_PRIO_THRESH_SIZE_BYTES 0x4

#define PLIC_PRIO_CLAIM_ADDR_OFFS 0x200004
#define PLIC_PRIO_CLAIM_SIZE_BYTES 0x4

static uint8_t *get_u8_reg_ptr(plic_td *plic, rv_uint_xlen address, uint8_t *is_claim_complete)
{
    uint8_t *ret_ptr = NULL;
    rv_uint_xlen tmp_address = 0;

    if(address < PLIC_PRIORITY_SIZE_BYTES)
    {
        tmp_address = address - PLIC_PRIORITY_ADDR_OFFS;
        /* first one is actually reserved */
        if(tmp_address >= 0x4)
        {
            ret_ptr = (uint8_t *)plic->priority;
            return &ret_ptr[tmp_address];
        }
    }
    else if(ADDR_WITHIN(address, PLIC_PENDING_ADDR_OFFS, PLIC_PENDING_SIZE_BYTES))
    {
        tmp_address = address - PLIC_PENDING_ADDR_OFFS;
        ret_ptr = (uint8_t *)plic->pending_bits;
        return &ret_ptr[tmp_address];
    }
    else if(ADDR_WITHIN(address, PLIC_IRQ_ENABLE_ADDR_OFFS, PLIC_IRQ_ENABLE_SIZE_BYTES))
    {
        tmp_address = address - PLIC_IRQ_ENABLE_ADDR_OFFS;
        ret_ptr = (uint8_t *)plic->enable_bits;
        return &ret_ptr[tmp_address];
    }
    else if(ADDR_WITHIN(address, PLIC_PRIO_THRESH_ADDR_OFFS, PLIC_PRIO_THRESH_SIZE_BYTES))
    {
        tmp_address = address - PLIC_PRIO_THRESH_ADDR_OFFS;
        ret_ptr = (uint8_t *)&plic->priority_threshold;
        return &ret_ptr[tmp_address];
    }
    else if(ADDR_WITHIN(address, PLIC_PRIO_CLAIM_ADDR_OFFS, PLIC_PRIO_CLAIM_SIZE_BYTES))
    {
        tmp_address = address - PLIC_PRIO_CLAIM_ADDR_OFFS;
        ret_ptr = (uint8_t *)&plic->claim_complete;
        *is_claim_complete = 1;
        return &ret_ptr[tmp_address];
    }

    return ret_ptr;
}

static void plic_check_sanity(plic_td *plic)
{
    uint32_t i = 0;

    for(i=0;i<NR_PRIO_MEM_REGS;i++)
    {
        plic->priority[i] = plic->priority[i] & 0x7;
    }

    plic->priority_threshold = plic->priority_threshold & 0x7;
}

void plic_add_pending(plic_td *plic, uint32_t interrupt_id, uint8_t pending)
{
    uint32_t irq_reg = interrupt_id/32;
    uint32_t irq_bit = interrupt_id%32;
    assign_u32_bit(&plic->pending_bits[irq_reg], irq_bit, pending);
}

uint8_t plic_update(plic_td *plic)
{
    uint32_t i,j = 0;
    uint32_t irq_id_count = 0;
    uint32_t irq_to_trigger = 0;
    uint8_t highest_prio = 0;

    /* check for any enabled interrupt and threshold */
    for(i=0;i<NR_ENABLE_REGS;i++)
        for(j=0;j<sizeof(plic->enable_bits[0])*8;j++)
        {
            if( CHECK_BIT(plic->enable_bits[i], j) && 
                CHECK_BIT(plic->pending_bits[i], j) && 
                (plic->priority[irq_id_count] >= plic->priority_threshold) )
            {
                /* find irq with highest prio */
                if( !CHECK_BIT(plic->claimed_bits[i], j) && (plic->priority[irq_id_count] > highest_prio))
                {
                    highest_prio = plic->priority[irq_id_count];
                    irq_to_trigger = irq_id_count;
                }
            }

            irq_id_count++;
        }

    if(irq_to_trigger > 0)
    {
        PLIC_DBG("plic !!IRQ!! trigger! %d\n", irq_to_trigger);
        plic->claim_complete = irq_to_trigger;
        return 1;
    }

    return 0;
}

int plic_read_reg(void *priv, rv_uint_xlen address, rv_uint_xlen *out_val)
{
    plic_td *plic = priv;
    uint8_t is_claim_complete = 0;
    uint32_t irq_reg = 0;
    uint32_t irq_bit = 0;
    uint8_t *u8_ptr = get_u8_reg_ptr(plic, address, &is_claim_complete);

    if(u8_ptr)
    {
        memcpy(out_val, u8_ptr, sizeof(*out_val));
        
        /* check if it is the claim complete reg */
        if(is_claim_complete)
        {
            irq_reg = *out_val/32;
            irq_bit = *out_val%32;
            assign_u32_bit(&plic->claimed_bits[irq_reg], irq_bit, 1);
        }

        return RV_MEM_ACCESS_OK;
    }

    return RV_MEM_ACCESS_ERR;
}

int plic_write_reg(void *priv, rv_uint_xlen address, rv_uint_xlen val, uint8_t nr_bytes)
{
    plic_td *plic = priv;
    uint8_t is_claim_complete = 0;
    uint32_t irq_reg = 0;
    uint32_t irq_bit = 0;
    uint8_t *u8_ptr = get_u8_reg_ptr(plic, address, &is_claim_complete);

    if(u8_ptr)
    {
        memcpy(u8_ptr, &val, nr_bytes);

        /* check if it is the claim complete reg */
        if(is_claim_complete)
        {
            irq_reg = val/32;
            irq_bit = val%32;
            assign_u32_bit(&plic->claimed_bits[irq_reg], irq_bit, 0);
        }

        /* be sure that all updates values are sane */
        plic_check_sanity(plic);
        return RV_MEM_ACCESS_OK;
    }

    return RV_MEM_ACCESS_ERR;
}
