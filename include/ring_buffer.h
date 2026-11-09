
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#define RX_BUFFER_SIZE 2048U

/*
 * Single producer: UART ISR
 * Single consumer: main()
 *
 * One array position is kept empty to distinguish
 * full from empty. Usable capacity = 127 bytes.
 */
typedef struct
{
    uint8_t data[RX_BUFFER_SIZE];

    volatile uint32_t head;
    volatile uint32_t tail;

    volatile uint32_t overflow_count;

} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb);

int ring_buffer_push(ring_buffer_t *rb, uint8_t data);

int ring_buffer_pop(ring_buffer_t *rb, uint8_t *data);

int ring_buffer_is_empty(const ring_buffer_t *rb);

#endif
