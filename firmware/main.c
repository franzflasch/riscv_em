#include <stdint.h>
#include <stddef.h>

#define reg_uart_data (*(volatile uint8_t*)0x3000000UL)

void putchar(char c)
{
    reg_uart_data = c;
}

void print(const char *p)
{
    while (*p)
        putchar(*(p++));
}

int main(void)
{
    print("Hello World from a simple RV32I ISA emulator!\n");

    return 0;
}
