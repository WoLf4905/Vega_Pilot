
#include "ring_buffer.h"

/*
 * RISC-V memory fence.
 *
 * Prevents compiler/CPU memory operations from being
 * reordered across publication of the buffer indices.
 */
static inline void ring_buffer_barrier(void)
{
    __asm__ volatile ("fence" ::: "memory");
}

void ring_buffer_init(ring_buffer_t *rb)
{
    if (rb == 0)
        return;

    rb->head = 0;
    rb->tail = 0;
    rb->overflow_count = 0;

    ring_buffer_barrier();
}

/*
 * Producer operation: called by UART ISR.
 */
int ring_buffer_push(ring_buffer_t *rb, uint8_t data)
{
    uint32_t head;
    uint32_t tail;
    uint32_t next_head;

    if (rb == 0)
        return 0;

    head = rb->head;
    tail = rb->tail;

    next_head = (head + 1U) & (RX_BUFFER_SIZE - 1U);

    /* Buffer full? */
    if (next_head == tail)
    {
        rb->overflow_count++;
        return 0;
    }

    /* Write byte into RAM. */
    rb->data[head] = data;

    /*
     * Publish data before publishing the new head.
     */
    ring_buffer_barrier();

    rb->head = next_head;

    return 1;
}

/*
 * Consumer operation: called by main().
 */
int ring_buffer_pop(ring_buffer_t *rb, uint8_t *data)
{
    uint32_t head;
    uint32_t tail;

    if (rb == 0 || data == 0)
        return 0;

    tail = rb->tail;
    head = rb->head;

    /* Buffer empty? */
    if (head == tail)
        return 0;

    /*
     * Make sure the producer's data write is visible
     * before reading the corresponding array element.
     */
    ring_buffer_barrier();

    *data = rb->data[tail];

    /*
     * Finish reading the slot before telling the
     * producer that the slot can be reused.
     */
    ring_buffer_barrier();

    rb->tail = (tail + 1U) & (RX_BUFFER_SIZE - 1U);

    return 1;
}

int ring_buffer_is_empty(const ring_buffer_t *rb)
{
    if (rb == 0)
        return 1;

    return rb->head == rb->tail;
}
