
#include "uart.h"
#include "ring_buffer.h"
#include "interrupt.h"

#include <stdint.h>

#define RISCV_MSTATUS_MIE (1U << 3)

static ring_buffer_t rx_buffer;
static ring_buffer_t tx_buffer;

/* Save and disable global machine interrupts. */
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

/* Restore the previous interrupt state. */
static inline void irq_restore(uint32_t previous)
{
    if (previous & RISCV_MSTATUS_MIE)
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
 * Queue one byte for transmission.
 *
 * Main loop = TX producer
 * UART ISR  = TX consumer
 */
static int uart0_write_async(uint8_t data)
{
    uint32_t previous = irq_save();

    if (!ring_buffer_push(&tx_buffer, data))
    {
        irq_restore(previous);
        return -1;
    }

    /*
     * Enable TX-empty interrupt after placing data
     * in the queue. Keep this operation protected
     * against an ISR race.
     */
    (void)uart_enable_tx_interrupt(UART_0);

    irq_restore(previous);

    return 0;
}

/*
 * Combined UART0 RX + TX interrupt handler.
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
     *
     * Read all currently available bytes from UART.
     * Put them into the RX ring buffer.
     */
    do
    {
        available = uart_read_byte_with_status(
            UART_0,
            &received,
            &line_status
        );

        if (available)
        {
            /*
             * Queue received binary byte.
             * No ASCII interpretation.
             */
            (void)ring_buffer_push(
                &rx_buffer,
                received
            );
        }

    } while (available);

    /*
     * ---------------- TX SERVICE ----------------
     *
     * Transmit one byte when the UART reports
     * that its transmit holding register is ready.
     */
    if (uart_tx_ready(UART_0))
    {
        /*
         * Preserve the queue pop + empty check +
         * interrupt disable as one protected operation.
         *
         * On this single-core target, the main loop
         * cannot execute while this ISR is running.
         */
        previous = irq_save();

        if (ring_buffer_pop(&tx_buffer, &transmit_byte))
        {
            uart_write_byte(UART_0, transmit_byte);

            /*
             * If the queue became empty, disable
             * TX interrupt now.
             */
            if (ring_buffer_is_empty(&tx_buffer))
            {
                (void)uart_disable_tx_interrupt(UART_0);
            }
        }
        else
        {
            (void)uart_disable_tx_interrupt(UART_0);
        }

        irq_restore(previous);
    }
}

int main(void)
{
    uart_config_t config =
    {
        .baud_rate = 115200U
    };

    uint8_t received;

    /*
     * STEP 1: Initialize UART hardware.
     */
    if (uart_init(UART_0, &config) != 0)
    {
        while (1)
        {
        }
    }

    /*
     * STEP 2: Initialize software ring buffers.
     */
    ring_buffer_init(&rx_buffer);
    ring_buffer_init(&tx_buffer);

    /*
     * STEP 3: Initialize interrupt controller.
     */
    interrupt_init();

    /*
     * STEP 4: Register UART0 interrupt handler.
     */
    if (interrupt_register_handler(
            VEGA_IRQ_UART0,
            uart0_isr) != 0)
    {
        while (1)
        {
        }
    }

    /*
     * STEP 5: Enable UART peripheral RX interrupt.
     */
    if (uart_enable_rx_interrupt(UART_0) != 0)
    {
        while (1)
        {
        }
    }

    /*
     * STEP 6: Enable UART0 source in interrupt controller.
     */
    if (interrupt_enable_source(VEGA_IRQ_UART0) != 0)
    {
        while (1)
        {
        }
    }

    /*
     * STEP 7: Main application loop.
     *
     * Consume received bytes from RX queue
     * and enqueue the exact same bytes for TX.
     *
     * No special characters.
     * No string handling.
     * No ASCII conversion.
     */
    while (1)
    {
        while (ring_buffer_pop(&rx_buffer, &received))
        {
            (void)uart0_write_async(received);
        }
    }

    return 0;
}
