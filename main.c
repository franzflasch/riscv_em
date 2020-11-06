#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <riscv_soc.h>

int main(int argc, char *argv[])
{
    uint32_t success_pc = 0;
    uint64_t num_cycles = 0;
    uint64_t curr_cycle = 0;

    if(argc < 2)
    {
        printf("please specify a fw file!\n");
        exit(-1);
    }

    if(argc < 3)
    {
        printf("ERROR: please specify success PC!\n");
        exit(2);
    }

    if(argc < 4)
    {
        printf("ERROR: please specify number of cycles! - 0 means endlessly\n");
        exit(3);
    }

    success_pc = strtol(argv[2], NULL, 16);
    num_cycles = strtol(argv[3], NULL, 10);

    rv32_soc_td rv32_soc;
    rv32_soc_init(&rv32_soc, argv[1]);

    // rv32_soc_dump_mem(&rv32_soc);

    printf("Now starting RV32I core, loaded program file will now be started...\n\n\n");

    while(1)
    {
        rv32_core_reg_dump_before_exec(&rv32_soc.rv32_core);
        rv32_core_run(&rv32_soc.rv32_core);
        // rv32_core_reg_internal_after_exec(&rv32_soc.rv32_core);

        if((rv32_soc.rv32_core.pc == (success_pc + 4)))
            break;

        if((num_cycles != 0) && (curr_cycle >= num_cycles))
            break;

        curr_cycle++;
    }
}
