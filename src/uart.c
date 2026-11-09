#include "uart.h"

/* This clock value is retained from the previously hardware-tested UART setup. */
#define UART_CLOCK_HZ 100000000UL

/* UART register offsets. */
#define UART_REG_DR       0x00U
#define UART_REG_IE       0x04U
#define UART_REG_IIR_FCR  0x08U
#define UART_REG_LCR      0x0CU
#define UART_REG_LSR      0x14U

#define UART_LCR_DLAB     (1U << 7)
#define UART_LCR_8N1      0x03U

#define UART_LSR_DR       (1U << 0)
#define UART_LSR_THRE     (1U << 5)
#define UART_IER_RX_DATA_AVAILABLE (1U << 0)

static const uintptr_t uart_base[] =
{
    0x10000100UL,
    0x10000200UL,
    0x10000300UL
};

static inline volatile uint32_t *uart_reg(uart_id_t uart, uint32_t offset)
{
    return (volatile uint32_t *)(uart_base[uart] + offset);
}

int uart_init(uart_id_t uart, const uart_config_t *config)
{
    uint32_t divisor;

    if ((unsigned int)uart > UART_2 || config == 0 || config->baud_rate == 0)
        return -1;

    divisor = UART_CLOCK_HZ / (16U * config->baud_rate);
    if (divisor == 0 || divisor > 65535U)
        return -1;

    /* DLAB=1: offsets 0x00 and 0x04 are divisor low/high. */
    *uart_reg(uart, UART_REG_LCR) = UART_LCR_DLAB;
    *uart_reg(uart, UART_REG_DR) = divisor & 0xFFU;
    *uart_reg(uart, UART_REG_IE) = (divisor >> 8) & 0xFFU;

    /* DLAB=0: offset 0x04 is now IER. Configure 8N1 and disable IRQs. */
    *uart_reg(uart, UART_REG_LCR) = UART_LCR_8N1;
    *uart_reg(uart, UART_REG_IE) = 0U;
    *uart_reg(uart, UART_REG_IIR_FCR) = 0U;

    __asm__ volatile ("fence" ::: "memory");
    return 0;
}

int uart_tx_ready(uart_id_t uart)
{
    if ((unsigned int)uart > UART_2)
        return 0;

    return ((*uart_reg(uart, UART_REG_LSR) & UART_LSR_THRE) != 0U);
}

void uart_write_byte(uart_id_t uart, uint8_t data)
{
    if ((unsigned int)uart > UART_2)
        return;

    while (!uart_tx_ready(uart))
    {
    }

    *uart_reg(uart, UART_REG_DR) = data;
    __asm__ volatile ("fence" ::: "memory");
}

void uart_write(uart_id_t uart, const uint8_t *data, uint32_t length)
{
    if ((unsigned int)uart > UART_2 || data == 0)
        return;

    for (uint32_t i = 0; i < length; ++i)
        uart_write_byte(uart, data[i]);
}

void uart_puts(uart_id_t uart, const char *str)
{
    if ((unsigned int)uart > UART_2 || str == 0)
        return;

    while (*str)
    {
        if (*str == '\n')
            uart_write_byte(uart, '\r');

        uart_write_byte(uart, (uint8_t)*str++);
    }
}

int uart_read_byte_with_status(uart_id_t uart,
                               uint8_t *data,
                               uint32_t *line_status)
{
    uint32_t status;

    if (line_status != 0)
        *line_status = 0U;

    if ((unsigned int)uart > UART_2 || data == 0)
        return 0;

    status = *uart_reg(uart, UART_REG_LSR);
    if (line_status != 0)
        *line_status = status;

    if ((status & UART_LSR_DR) == 0U)
        return 0;

    *data = (uint8_t)(*uart_reg(uart, UART_REG_DR) & 0xFFU);
    return 1;
}

int uart_read_byte(uart_id_t uart, uint8_t *data)
{
    return uart_read_byte_with_status(uart, data, 0);
}

uint8_t uart_read_byte_blocking(uart_id_t uart)
{
    uint8_t data;

    while (!uart_read_byte(uart, &data))
    {
    }

    return data;
}

int uart_enable_rx_interrupt(uart_id_t uart)
{
    uint32_t lcr;

    if ((unsigned int)uart > UART_2)
        return -1;

    lcr = *uart_reg(uart, UART_REG_LCR);
    lcr &= ~UART_LCR_DLAB;
    *uart_reg(uart, UART_REG_LCR) = lcr;
    *uart_reg(uart, UART_REG_IE) = UART_IER_RX_DATA_AVAILABLE;

    __asm__ volatile ("fence" ::: "memory");
    return 0;
}

int uart_disable_interrupts(uart_id_t uart)
{
    uint32_t lcr;

    if ((unsigned int)uart > UART_2)
        return -1;

    lcr = *uart_reg(uart, UART_REG_LCR);
    lcr &= ~UART_LCR_DLAB;
    *uart_reg(uart, UART_REG_LCR) = lcr;
    *uart_reg(uart, UART_REG_IE) = 0U;

    __asm__ volatile ("fence" ::: "memory");
    return 0;
}
