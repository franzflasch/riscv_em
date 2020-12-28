#include <stdio.h>

#include <fifo.h>

int main(void)
{
    uint8_t data_arr[16];
    fifo_t fifo = {0};
    fifo_init(&fifo, data_arr, 16);


    uint8_t tmp_data = 42;
    fifo_in(&fifo, &tmp_data, 1);
    tmp_data = 43;
    fifo_in(&fifo, &tmp_data, 1);
    tmp_data = 44;
    fifo_in(&fifo, &tmp_data, 1);
    tmp_data = 45;
    fifo_in(&fifo, &tmp_data, 1);

    uint8_t tmp_data_out = 0;
    fifo_out(&fifo, &tmp_data_out, 1);
    fifo_out(&fifo, &tmp_data_out, 1);

    printf("Hello %d %d\n", tmp_data_out, fifo_len(&fifo));

}