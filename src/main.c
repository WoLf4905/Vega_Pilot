#include "uart.h"
#include "ring_buffer.h"
#include "interrupt.h"

static ring_buffer_t rx_buffer;

/* Written only by the UART ISR; read by main() for diagnostics. */
static volatile uint32_t rx_interrupt_count;
static volatile uint32_t rx_bytes_received;
static volatile uint32_t uart_overrun_observations;
static volatile uint32_t uart_parity_observations;
static volatile uint32_t uart_framing_observations;

static void uart0_rx_isr(void)
{
    uint8_t received;
    uint32_t line_status;
    int available;

    ++rx_interrupt_count;

    /*
     * Read LSR and consume every currently available byte. The final
     * no-data read also gives us one last LSR snapshot for error flags.
     */
    do
    {
        available = uart_read_byte_with_status(
            UART_0, &received, &line_status);

        if ((line_status & UART_LINE_STATUS_OVERRUN) != 0U)
            ++uart_overrun_observations;

        if ((line_status & UART_LINE_STATUS_PARITY) != 0U)
            ++uart_parity_observations;

        if ((line_status & UART_LINE_STATUS_FRAMING) != 0U)
            ++uart_framing_observations;

        if (available)
        {
            ++rx_bytes_received;
            (void)ring_buffer_push(&rx_buffer, received);
        }
    }
    while (available);
}

static void uart_put_u32(uart_id_t uart, uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;

    if (value == 0U)
    {
        uart_write_byte(uart, '0');
        return;
    }

    while (value != 0U && count < sizeof(digits))
    {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (count > 0U)
        uart_write_byte(uart, (uint8_t)digits[--count]);
}

static void print_uart_diagnostics(void)
{
    /* Aligned 32-bit reads are atomic on this RV32 target. */
    uint32_t irq_count = rx_interrupt_count;
    uint32_t byte_count = rx_bytes_received;
    uint32_t overflow_count = rx_buffer.overflow_count;
    uint32_t overrun_count = uart_overrun_observations;
    uint32_t parity_count = uart_parity_observations;
    uint32_t framing_count = uart_framing_observations;

    uart_puts(UART_0, "\n========== UART DIAGNOSTICS ==========\n");

    uart_puts(UART_0, "RX interrupt entries       : ");
    uart_put_u32(UART_0, irq_count);
    uart_puts(UART_0, "\n");

    uart_puts(UART_0, "RX bytes read from UART    : ");
    uart_put_u32(UART_0, byte_count);
    uart_puts(UART_0, "\n");

    uart_puts(UART_0, "Ring buffer overflows      : ");
    uart_put_u32(UART_0, overflow_count);
    uart_puts(UART_0, "\n");

    uart_puts(UART_0, "LSR overrun observations   : ");
    uart_put_u32(UART_0, overrun_count);
    uart_puts(UART_0, "\n");

    uart_puts(UART_0, "LSR parity observations    : ");
    uart_put_u32(UART_0, parity_count);
    uart_puts(UART_0, "\n");

    uart_puts(UART_0, "LSR framing observations   : ");
    uart_put_u32(UART_0, framing_count);
    uart_puts(UART_0, "\n======================================\n");
}

int main(void)
{
    uart_config_t config =
    {
        .baud_rate = 115200U
    };
    uint8_t received;

    if (uart_init(UART_0, &config) != 0)
    {
        while (1)
        {
        }
    }

    ring_buffer_init(&rx_buffer);
    rx_interrupt_count = 0U;
    rx_bytes_received = 0U;
    uart_overrun_observations = 0U;
    uart_parity_observations = 0U;
    uart_framing_observations = 0U;

    interrupt_init();
    uart_puts(UART_0, "UART RX DIAGNOSTIC TEST\n");

    if (interrupt_register_handler(VEGA_IRQ_UART0, uart0_rx_isr) != 0)
    {
        uart_puts(UART_0, "IRQ registration failed\n");
        while (1)
        {
        }
    }

    if (interrupt_enable_source(VEGA_IRQ_UART0) != 0)
    {
        uart_puts(UART_0, "IRQ enable failed\n");
        while (1)
        {
        }
    }

    if (uart_enable_rx_interrupt(UART_0) != 0)
    {
        uart_puts(UART_0, "UART RX IRQ failed\n");
        while (1)
        {
        }
    }

    uart_puts(UART_0, "UART RX INTERRUPT ENABLED\n");
    uart_puts(UART_0, "Echo enabled. Send '?' for diagnostics.\n");

    while (1)
    {
        while (ring_buffer_pop(&rx_buffer, &received))
        {
            if (received == '?')
                print_uart_diagnostics();
            else
                uart_write_byte(UART_0, received);
        }
    }

    return 0;
}
