#include <stdio.h>
#include <string.h>

#include <riscv_helper.h>
#include <plic.h>

int main(void)
{
    plic_td plic = {0};

    rv_uint_xlen tmp_val = 0;
    rv_uint_xlen tmp_read_val = 0;

    // printf("%p %p\n", get_u8_reg_ptr(&plic, 0x1004), (void *)&plic.pending_bits[1]);

    tmp_val = 42;
    plic_write_reg(&plic, 0x8, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x8, &tmp_read_val);
    printf("RESULT %x %lx\n", plic.priority[2], tmp_read_val);

    tmp_val = 43;
    plic_write_reg(&plic, 0x1004, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x1004, &tmp_read_val);
    printf("RESULT %x %lx\n", plic.pending_bits[1], tmp_read_val);

    tmp_val = 44;
    plic_write_reg(&plic, 0x2004, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x2004, &tmp_read_val);
    printf("RESULT %x %lx\n", plic.enable_bits[1], tmp_read_val);

    tmp_val = 45;
    plic_write_reg(&plic, 0x200000, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x200000, &tmp_read_val);
    printf("RESULT %x %lx\n", plic.priority_threshold, tmp_read_val);

    tmp_val = 46;
    plic_write_reg(&plic, 0x200004, tmp_val, sizeof(uint32_t));
    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    printf("RESULT %x %lx\n", plic.claim_complete, tmp_read_val);


    /* now test some irq behavior */
    memset(&plic, 0 , sizeof(plic));

    plic.priority[10] = 7;
    plic.enable_bits[0] = (1<<10);
    plic.priority_threshold = 0;
    plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");
    printf("\n\n");

    /* now set pending */
    plic.pending_bits[0] |= (1<<10);
    plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");
    printf("\n\n");

    /* test priorities */
    plic.priority[12] = 5;
    plic.enable_bits[0] |= (1<<12);
    plic.pending_bits[0] |= (1<<12);
    printf("pending %x\n", plic.enable_bits[0]);

    plic.priority[37] = 6;
    plic.enable_bits[1] |= (1<<5);
    plic.pending_bits[1] |= (1<<5);

    printf("pending %x\n", plic.enable_bits[1]);
    plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");
    printf("\n\n");

    /* now claim the interrupt and check if gets triggered again */
    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");
    printf("\n\n");

    plic_read_reg(&plic, 0x200004, &tmp_read_val);
    plic_update(&plic) ? printf("Interrupt!\n") : printf("No Interrupt.\n");
    printf("\n\n");
}