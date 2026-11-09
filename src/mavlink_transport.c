#include "mavlink_transport.h"

#include "uart.h"
#include "ring_buffer.h"
#include "interrupt.h"

#include <stdint.h>

#define TRANSPORT_UART UART_0
#define TRANSPORT_IRQ  VEGA_IRQ_UART0

#define RISCV_MSTATUS_MIE (1U << 3)

static ring_buffer_t rx_buffer;
static ring_buffer_t tx_buffer;

static volatile uint8_t transport_initialized = 0U;

/*
 * Save global interrupt state and disable machine interrupts.
 */
static inline uint32_t irq_save(void)
{
    uint32_t previous;

    __asm__ volatile (
        "csrrc %0, mstatus, %1"
        : "=r"(previous)
        : "r"(RISCV_MSTATUS_MIE)
        : "memory"
    );

    return previous;
}

/*
 * Restore the previous global interrupt state.
 */
static inline void irq_restore(uint32_t previous)
{
    if ((previous & RISCV_MSTATUS_MIE) != 0U)
    {
        __asm__ volatile (
            "csrs mstatus, %0"
            :
            : "r"(RISCV_MSTATUS_MIE)
            : "memory"
        );
    }
}

/*
 * UART0 interrupt handler.
 *
 * RX: UART hardware -> RX ring buffer
 * TX: TX ring buffer -> UART hardware
 *
 * No MAVLink processing happens inside this ISR.
 */
static void uart0_isr(void)
{
    uint8_t received;
    uint8_t transmit_byte;

    uint32_t line_status;
    uint32_t previous;

    int available;

    /*
     * ---------------- RX SERVICE ----------------
     */
    do
    {
        available = uart_read_byte_with_status(
            TRANSPORT_UART,
            &received,
            &line_status
        );

        if (available)
        {
            (void)ring_buffer_push(&rx_buffer, received);
        }

    } while (available);

    /*
     * ---------------- TX SERVICE ----------------
     */
    if (uart_tx_ready(TRANSPORT_UART))
    {
        previous = irq_save();

        if (ring_buffer_pop(&tx_buffer, &transmit_byte))
        {
            uart_write_byte(TRANSPORT_UART, transmit_byte);

            /*
             * Disable TX interrupt when no data remains.
             */
            if (ring_buffer_is_empty(&tx_buffer))
            {
                (void)uart_disable_tx_interrupt(TRANSPORT_UART);
            }
        }
        else
        {
            (void)uart_disable_tx_interrupt(TRANSPORT_UART);
        }

        irq_restore(previous);
    }
}

/*
 * Initialize UART0 and its interrupt path.
 */
int mavlink_transport_init(uint32_t baud_rate)
{
    uart_config_t config;

    if (baud_rate == 0U)
    {
        return -1;
    }

    transport_initialized = 0U;

    /*
     * Initialize software queues before enabling interrupts.
     */
    ring_buffer_init(&rx_buffer);
    ring_buffer_init(&tx_buffer);

    /*
     * Initialize UART.
     */
    config.baud_rate = baud_rate;

    if (uart_init(TRANSPORT_UART, &config) != 0)
    {
        return -1;
    }

    /*
     * Initialize RISC-V trap vector and VEGA interrupt controller.
     */
    interrupt_init();

    /*
     * Register UART0 interrupt handler.
     */
    if (interrupt_register_handler(
            TRANSPORT_IRQ,
            uart0_isr) != 0)
    {
        return -1;
    }

    /*
     * Enable UART peripheral RX interrupt.
     */
    if (uart_enable_rx_interrupt(TRANSPORT_UART) != 0)
    {
        return -1;
    }

    /*
     * Enable UART0 interrupt-controller source.
     */
    if (interrupt_enable_source(TRANSPORT_IRQ) != 0)
    {
        return -1;
    }

    transport_initialized = 1U;

    return 0;
}

/*
 * Write a complete byte sequence to the TX queue.
 *
 * The complete sequence is queued atomically with respect
 * to the UART ISR. It is either accepted in full or rejected.
 */
int mavlink_transport_write(const uint8_t *data, uint16_t length)
{
    uint32_t previous;
    uint32_t head;
    uint32_t tail;
    uint32_t used;
    uint32_t free_space;

    if (!transport_initialized || data == 0)
    {
        return -1;
    }

    if (length == 0U)
    {
        return 0;
    }

    if ((uint32_t)length >= RX_BUFFER_SIZE)
    {
        return -2;
    }

    /*
     * Prevent the TX ISR from consuming data while
     * checking available queue space and inserting bytes.
     */
    previous = irq_save();

    head = tx_buffer.head;
    tail = tx_buffer.tail;

    used = (head - tail) & (RX_BUFFER_SIZE - 1U);

    free_space = (RX_BUFFER_SIZE - 1U) - used;

    if ((uint32_t)length > free_space)
    {
        irq_restore(previous);
        return -2;
    }

    /*
     * Insert the entire byte sequence.
     */
    for (uint32_t i = 0U; i < length; ++i)
    {
        if (!ring_buffer_push(&tx_buffer, data[i]))
        {
            irq_restore(previous);
            return -2;
        }
    }

    /*
     * Start interrupt-driven transmission.
     */
    if (uart_enable_tx_interrupt(TRANSPORT_UART) != 0)
    {
        irq_restore(previous);
        return -3;
    }

    irq_restore(previous);

    return 0;
}

/*
 * Read one byte from the RX queue.
 */
int mavlink_transport_read_byte(uint8_t *byte)
{
    if (!transport_initialized || byte == 0)
    {
        return 0;
    }

    return ring_buffer_pop(&rx_buffer, byte);
}
