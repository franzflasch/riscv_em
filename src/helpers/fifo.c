#include <stdint.h>
#include <string.h>
#include "fifo.h"

/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline unsigned int fifo_unused(fifo_t *fifo)
{
        return (fifo->mask + 1) - (fifo->in - fifo->out);
}

/**
 * fifo_init - initilizes a fifo structure
 * @fifo: address of the fifo to be used
 */
void fifo_init(fifo_t * fifo, uint8_t *data_arr, unsigned int data_arr_size)
{
        fifo->in    = 0;
        fifo->out   = 0;
        fifo->mask  = data_arr_size - 1;
        fifo->size  = data_arr_size;
        fifo->data  = data_arr;
}

void fifo_reset(fifo_t * fifo) 
{
        fifo->in    = fifo->out   = 0;
}

/**
 * fifo_size - returns the size of the fifo in elements
 * @fifo: address of the fifo to be used
 */
unsigned int fifo_size(fifo_t * fifo)
{
    return ((fifo)->mask + 1);
}

/**
 * fifo_len - returns the number of used elements in the fifo
 * @fifo: address of the fifo to be used
 */
unsigned int fifo_len(fifo_t * fifo)
{
    // two usigned will not make this negative 
    // even if in or out overflow as long as in is ahead of out
    return fifo->in - fifo->out;
}

/**
 * kfifo_is_empty - returns true if the fifo is empty
 * @fifo: address of the fifo to be used
 */
int fifo_is_empty(fifo_t * fifo)
{
     return fifo->in == fifo->out;
}

/**
 * fifo_is_full - returns true if the fifo is full
 * @fifo: address of the fifo to be used
 */
int fifo_is_full(fifo_t * fifo)
{
     return fifo_len(fifo) > fifo->mask;
}

/**
 * fifo_capacity - returns the number free space for new elements
 * @fifo: address of the fifo to be used
 */
unsigned int fifo_free(fifo_t * fifo)
{
     return fifo_unused(fifo);
}

int fifo_put(fifo_t * fifo, uint8_t val)
{
        int ret;
        ret = !fifo_is_full(fifo);
        if (ret) {
                fifo->data[fifo->in & fifo->mask] = val;
                fifo->in++;
        }
        return ret;
}

int fifo_get(fifo_t * fifo, uint8_t * val)
{  
        int ret;   
        ret = !fifo_is_empty(fifo);   
        if (ret) { 
                *val = fifo->data[fifo->out & fifo->mask];
                fifo->out++;
        }
        return ret;
}

static void fifo_copy_out(fifo_t *fifo, uint8_t *dst,
                unsigned int len, unsigned int off)
{
        unsigned int size = fifo->mask + 1;
        unsigned int l;

        off &= fifo->mask;
        l = size - off;
        if(l > len)
            l = len;

        memcpy(dst, fifo->data + off, l);
        memcpy(dst + l, fifo->data, len - l);
}

unsigned int fifo_out_peek(fifo_t *fifo,
                uint8_t *buf, unsigned int len)
{
        unsigned int l;

        l = fifo->in - fifo->out;
        if (len > l)
                len = l;

        fifo_copy_out(fifo, buf, len, fifo->out);
        return len;
}

unsigned int fifo_out(fifo_t * fifo, uint8_t * buf, unsigned long n) 
{ 
        unsigned long len = fifo_out_peek(fifo, buf, n);
        fifo->out += len;
        return len;
}

static void fifo_copy_in(fifo_t *fifo, const uint8_t *src,
                unsigned int len, unsigned int off)
{
        unsigned int size = fifo->mask + 1;
        unsigned int l;

        off &= fifo->mask;
        l = size - off;
	if(len < l) 
		l = len;

        memcpy(fifo->data + off, src, l);
        memcpy(fifo->data, src + l, len - l);
}

unsigned int fifo_in(fifo_t *fifo,
               const uint8_t *buf, unsigned long len)
{
        unsigned int l;

        l = fifo_unused(fifo);
        if (len > l)
                len = l;

        fifo_copy_in(fifo, buf, len, fifo->in);
        fifo->in += len;
        return len;
}
