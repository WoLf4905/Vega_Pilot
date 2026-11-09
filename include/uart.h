#ifndef UART_H
#define UART_H

#include <stdint.h>

/* UART peripheral identifiers. */
typedef enum
{
    UART_0 = 0,
    UART_1 = 1,
    UART_2 = 2
} uart_id_t;

typedef struct
{
    uint32_t baud_rate;
} uart_config_t;

/* UART line-status bits exposed to callers for diagnostics. */
#define UART_LINE_STATUS_OVERRUN  (1U << 1)
#define UART_LINE_STATUS_PARITY   (1U << 2)
#define UART_LINE_STATUS_FRAMING  (1U << 3)

int uart_init(uart_id_t uart, const uart_config_t *config);
void uart_write_byte(uart_id_t uart, uint8_t data);
void uart_write(uart_id_t uart, const uint8_t *data, uint32_t length);
void uart_puts(uart_id_t uart, const char *str);

/* Non-blocking receive. Returns 1 when a byte was read. */
int uart_read_byte(uart_id_t uart, uint8_t *data);

/*
 * Non-blocking receive plus the LSR snapshot associated with the read.
 * line_status may be NULL. A status snapshot is returned even when no
 * data byte is available, allowing the caller to observe LSR error flags.
 */
int uart_read_byte_with_status(uart_id_t uart,
                               uint8_t *data,
                               uint32_t *line_status);

uint8_t uart_read_byte_blocking(uart_id_t uart);
int uart_tx_ready(uart_id_t uart);
int uart_enable_rx_interrupt(uart_id_t uart);
int uart_disable_interrupts(uart_id_t uart);

#endif
