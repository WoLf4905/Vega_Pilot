#ifndef ARIES_V3_INTERRUPT_H
#define ARIES_V3_INTERRUPT_H

#include <stdint.h>

/*
 * ARIES V3 / VEGA interrupt-controller interface.
 *
 * The source numbering below is the RV32 mapping used by the supplied
 * C-DAC AriesV3_interrupt_driver.h. This project currently targets RV32.
 */
#if defined(__riscv_xlen) && (__riscv_xlen != 32)
#error "This interrupt layer is currently written for the ARIES V3 RV32 target."
#endif

#define VEGA_IRQ_UART0       0u
#define VEGA_IRQ_UART1       1u
#define VEGA_IRQ_UART2       2u
#define VEGA_IRQ_SPI0        3u
#define VEGA_IRQ_SPI1        4u
#define VEGA_IRQ_I2C0        5u
#define VEGA_IRQ_I2C1        6u
#define VEGA_IRQ_TIMER0      7u
#define VEGA_IRQ_TIMER1      8u
#define VEGA_IRQ_TIMER2      9u
#define VEGA_IRQ_GPIO0       10u
#define VEGA_IRQ_GPIO1       11u
#define VEGA_IRQ_GPIO2       12u
#define VEGA_IRQ_GPIO3       13u
#define VEGA_IRQ_GPIO4       14u
#define VEGA_IRQ_GPIO5       15u
#define VEGA_IRQ_GPIO6       16u
#define VEGA_IRQ_GPIO7       17u
#define VEGA_IRQ_GPIO8       18u
#define VEGA_IRQ_GPIO9       19u
#define VEGA_IRQ_GPIO10      20u
#define VEGA_IRQ_GPIO11      21u
#define VEGA_IRQ_GPIO12      22u

#define VEGA_IRQ_SOURCE_COUNT 32u

/* Standard RISC-V machine-level interrupt cause numbers and enable bits. */
#define RISCV_CAUSE_MACHINE_SOFTWARE_INTERRUPT  3u
#define RISCV_CAUSE_MACHINE_EXTERNAL_INTERRUPT 11u
#define RISCV_MSTATUS_MIE_BIT                  (1u << 3)
#define RISCV_MIE_MEIE_BIT                     (1u << 11)
#define RISCV_MCAUSE_INTERRUPT_BIT             0x80000000u
#define RISCV_MCAUSE_CODE_MASK                 0x7fffffffu

/*
 * VEGA custom interrupt controller register layout.
 * Offsets confirmed by the C-DAC AriesV3_interrupt_driver.h:
 * RAW_INTR      0x00
 * reserved      0x04
 * INTR_EN       0x08
 * reserved      0x0C
 * INTR_STATUS   0x10
 */
typedef struct
{
    volatile uint32_t RAW_INTR;
    uint32_t reserved_04;
    volatile uint32_t INTR_EN;
    uint32_t reserved_0C;
    volatile uint32_t INTR_STATUS;
} vega_interrupt_controller_t;

#define VEGA_INTERRUPT_CONTROLLER_BASE 0x20010000UL
#define VEGA_INTERRUPT_CONTROLLER \
    ((vega_interrupt_controller_t *)(uintptr_t)VEGA_INTERRUPT_CONTROLLER_BASE)

/* A peripheral ISR must be short, non-blocking, and clear its own request. */
typedef void (*interrupt_handler_t)(void);

/*
 * Install the machine trap vector and leave machine interrupts disabled.
 * Call this once during startup, before registering/enabling sources.
 */
void interrupt_init(void);

/* Register a handler for one VEGA controller source. Returns 0 on success. */
int interrupt_register_handler(uint8_t source, interrupt_handler_t handler);

/*
 * Enable one controller source and machine external interrupts.
 * A handler must already be registered. Call only after peripheral state
 * and any ISR-shared buffers have been initialized.
 */
int interrupt_enable_source(uint8_t source);

/* Disable one source at the VEGA controller. */
int interrupt_disable_source(uint8_t source);

#endif /* ARIES_V3_INTERRUPT_H */
