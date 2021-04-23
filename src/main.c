#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* for uart RX thread */
#include <termios.h>
#include <pthread.h>

#include <riscv_helper.h>
#include <riscv_example_soc.h>
#include <simple_uart.h>

char getch() 
{
    char buf = 0;
    struct termios old = {0};

    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");

    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");

    if (read(0, &buf, 1) < 0)
        perror ("read()");

    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;

    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror ("tcsetattr ~ICANON");

    return (buf);
}

void *uart_rx_thread(void* p)
{
    // (void)p;
    rv_soc_td *rv_soc = p;
    char x = 0;

    printf("Uart RX Thread running...\n");

    while(1)
    {
        // x = getchar();
        x = getch();
        // printf("Press: %c PC: "PRINTF_FMT" virt: "PRINTF_FMT" phys: "PRINTF_FMT"\n", x , rv_soc->rv_core0.pc, rv_soc->rv_core0.mmu.last_virt_pc, rv_soc->rv_core0.mmu.last_phys_pc);

        #ifdef USE_SIMPLE_UART
            simple_uart_add_rx_char(&rv_soc->uart, x);
        #else
            uart_add_rx_char(&rv_soc->uart8250, x);
        #endif
    }
}

void start_uart_rx_thread(void *p)
{
    pthread_t uart_rx_th_id;
    pthread_create(&uart_rx_th_id, NULL, uart_rx_thread, p);
}

static void parse_options(int argc, 
                          char** argv, 
                          char **fw_file, 
                          char **dtb_file, 
                          rv_uint_xlen *success_pc, 
                          uint64_t *num_cycles)
{
    int c;
    char *arg_fw_file = NULL;
    char *arg_dtb_file = NULL;
    char *arg_success_pc = NULL;
    char *arg_num_cycles = NULL;

    while ((c = getopt(argc, argv, "s:f:d:n:")) != -1)
    {
        switch (c)
        {
            case 's':
            {
                arg_success_pc = optarg;
                if (arg_success_pc)
                {
                    *success_pc = strtol(arg_success_pc, NULL, 16);
                    // printf("A %s %d\n", arg_success_pc, success_pc);
                }
                break;
            }
            case 'f':
            {
                arg_fw_file = optarg;
                // if (arg_fw_file)
                // {
                //     printf("Firmware file %s\n", arg_fw_file);
                // }
                break;
            }
            case 'd':
            {
                arg_dtb_file = optarg;
                // if (arg_fw_file)
                // {
                //     printf("Firmware file %s\n", arg_fw_file);
                // }
                break;
            }
            case 'n':
            {
                arg_num_cycles = optarg;
                if (arg_num_cycles)
                {
                    *num_cycles = strtol(arg_num_cycles, NULL, 10);
                    // printf("A %s %d\n", arg_success_pc, success_pc);
                }
                break;
            }
            case '?':
            {
                break;
            }
            default:
            {
            }
        }
    }

    if(arg_fw_file == NULL)
    {
        printf("Please specify firwmare file!\n");
        exit(1);
    }

    if(arg_dtb_file == NULL)
    {
        printf("No dtb specified! Linux will probably not work\n");
    }

    printf("FW file: %s\n", arg_fw_file);
    printf("Success PC: " PRINTF_FMT "\n", *success_pc);
    printf("Num Cycles: %ld\n", *num_cycles);

    *fw_file = arg_fw_file;
    *dtb_file = arg_dtb_file;
}


int main(int argc, char *argv[])
{
    char *fw_file = NULL;
    char *dtb_file = NULL;
    rv_uint_xlen success_pc = 0;
    uint64_t num_cycles = 0;

    parse_options(argc, argv, &fw_file, &dtb_file, &success_pc, &num_cycles);

    rv_soc_td rv_soc;
    rv_soc_init(&rv_soc, fw_file, dtb_file);

    #ifndef RISCV_EM_DEBUG
        start_uart_rx_thread(&rv_soc);
    #endif

    // rv_soc_dump_mem(&rv_soc);

    printf("Now starting rvI core, loaded program file will now be started...\n\n\n");

    rv_soc_run(&rv_soc, success_pc, num_cycles);
}
