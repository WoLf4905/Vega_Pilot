#include "timer.h"
#include "interrupt.h"
#include "config.h"

#include <stdint.h>

/*
 * Timer 0 register layout.
 *
 * Each register is 32 bits.
 */
typedef struct
{
    volatile uint32_t LOAD_COUNT;       /* 0x00 */
    volatile uint32_t CURRENT_VALUE;    /* 0x04 */
    volatile uint32_t CONTROL;          /* 0x08 */
    volatile uint32_t EOI;              /* 0x0C */
    volatile uint32_t INTR_STATUS;      /* 0x10 */

} timer_registers_t;

/*
 * Timer 0 memory-mapped hardware address.
 */
#define TIMER0 \
    ((timer_registers_t *)(uintptr_t)TIMER_BASE_ADDRESS)

/*
 * Control register bits.
 *
 * Bit 0: Timer enable
 * Bit 1: Periodic mode
 * Bit 2: Interrupt mask (0 = unmasked)
 *
 * Control = 0x03 enables periodic interrupts.
 */
#define TIMER_CTRL_ENABLE       (1U << 0)
#define TIMER_CTRL_PERIODIC     (1U << 1)

#define TIMER_CTRL_RUN \
    (TIMER_CTRL_ENABLE | TIMER_CTRL_PERIODIC)

/*
 * Vendor Arduino Timer implementation uses:
 *
 * ratio = F_CPU / 1000000
 *
 * ARIES V3 boards.txt specifies F_CPU = 100 MHz.
 *
 * A one-second period therefore uses 100 million counts.
 */
#define TIMER0_CLOCK_HZ         100000000UL
#define TIMER0_PERIOD_TICKS     TIMER0_CLOCK_HZ

/*
 * Set by the timer ISR and consumed by main().
 *
 * Multiple ticks before consumption are coalesced into
 * one pending event. This is sufficient for a 1 Hz heartbeat.
 */
static volatile uint32_t tick_pending = 0U;

/*
 * Save global machine interrupt state and disable interrupts.
 */
static inline uint32_t timer_irq_save(void)
{
    uint32_t previous;

    __asm__ volatile (
        "csrrc %0, mstatus, %1"
        : "=r"(previous)
        : "r"(RISCV_MSTATUS_MIE_BIT)
        : "memory"
    );

    return previous;
}

/*
 * Restore the previous machine interrupt state.
 */
static inline void timer_irq_restore(uint32_t previous)
{
    if ((previous & RISCV_MSTATUS_MIE_BIT) != 0U)
    {
        __asm__ volatile (
            "csrs mstatus, %0"
            :
            : "r"(RISCV_MSTATUS_MIE_BIT)
            : "memory"
        );
    }
}

/*
 * Timer 0 interrupt handler.
 *
 * IMPORTANT:
 * 1. Read EOI to acknowledge the hardware interrupt.
 * 2. Set a software flag.
 * 3. Do not construct or transmit MAVLink here.
 */
static void timer0_isr(void)
{
    /*
     * Reading EOI clears/acknowledges the timer request,
     * following the vendor Timer.cpp implementation.
     */
    (void)TIMER0->EOI;

    /*
     * Notify the main application.
     */
    tick_pending = 1U;
}

/*
 * Initialize Timer 0 for a nominal 1 Hz periodic interrupt.
 */
int timer0_init_1hz(void)
{
    /*
     * Stop timer before configuring it.
     */
    TIMER0->CONTROL = 0U;

    __asm__ volatile ("fence" ::: "memory");

    /*
     * Load the one-second countdown.
     */
    TIMER0->LOAD_COUNT = TIMER0_PERIOD_TICKS;

    __asm__ volatile ("fence" ::: "memory");

    /*
     * Clear any stale interrupt request.
     */
    (void)TIMER0->EOI;

    tick_pending = 0U;

    /*
     * Register the handler before enabling the source.
     */
    if (interrupt_register_handler(
            VEGA_IRQ_TIMER0,
            timer0_isr) != 0)
    {
        return -1;
    }

    /*
     * Enable Timer 0 in the VEGA interrupt controller.
     *
     * This also enables machine external and global
     * machine interrupts through your existing driver.
     */
    if (interrupt_enable_source(VEGA_IRQ_TIMER0) != 0)
    {
        TIMER0->CONTROL = 0U;
        return -1;
    }

    /*
     * Start Timer 0 in periodic interrupt mode.
     *
     * Equivalent to the vendor Timer.cpp:
     * TIMER_REG(id, TIMER_REG_Control) = 0x03;
     */
    TIMER0->CONTROL = TIMER_CTRL_RUN;

    __asm__ volatile ("fence" ::: "memory");

    return 0;
}

/*
 * Atomically retrieve and clear the pending timer flag.
 *
 * Main code calls this function.
 */
int timer0_take_tick(void)
{
    uint32_t previous;
    uint32_t pending;

    previous = timer_irq_save();

    pending = tick_pending;
    tick_pending = 0U;

    timer_irq_restore(previous);

    return (pending != 0U) ? 1 : 0;
}
