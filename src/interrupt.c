#include "interrupt.h"

/* Defined by startup/interrupt_entry.S. */
extern void INTERRUPT_Handler_0(void);

static interrupt_handler_t volatile irq_handlers[VEGA_IRQ_SOURCE_COUNT];

static inline uint32_t read_mcause(void)
{
    uint32_t value;
    __asm__ volatile ("csrr %0, mcause" : "=r" (value));
    return value;
}

void interrupt_init(void)
{
    uint32_t mask;
    uintptr_t vector_address = (uintptr_t)&INTERRUPT_Handler_0;

    /* Start with machine interrupts disabled while the system is prepared. */
    mask = RISCV_MSTATUS_MIE_BIT;
    __asm__ volatile ("csrc mstatus, %0" :: "r" (mask) : "memory");

    mask = RISCV_MIE_MEIE_BIT;
    __asm__ volatile ("csrc mie, %0" :: "r" (mask) : "memory");

    /* Clear the software handler table explicitly. */
    for (uint32_t i = 0; i < VEGA_IRQ_SOURCE_COUNT; ++i)
    {
        irq_handlers[i] = (interrupt_handler_t)0;
    }

    /* mtvec.MODE=00: direct mode; all traps enter INTERRUPT_Handler_0. */
    __asm__ volatile ("csrw mtvec, %0" :: "r" (vector_address) : "memory");
}

int interrupt_register_handler(uint8_t source, interrupt_handler_t handler)
{
    if ((source >= VEGA_IRQ_SOURCE_COUNT) || (handler == (interrupt_handler_t)0))
    {
        return -1;
    }

    /* Registration is expected before enabling this source. */
    irq_handlers[source] = handler;
    __asm__ volatile ("fence" ::: "memory");
    return 0;
}

int interrupt_enable_source(uint8_t source)
{
    uint32_t source_mask;
    uint32_t cpu_mask;

    if ((source >= VEGA_IRQ_SOURCE_COUNT) ||
        (irq_handlers[source] == (interrupt_handler_t)0))
    {
        return -1;
    }

    source_mask = (uint32_t)1u << source;

    /* Enable the source in the VEGA interrupt controller first. */
    VEGA_INTERRUPT_CONTROLLER->INTR_EN |= source_mask;
    __asm__ volatile ("fence" ::: "memory");

    /* Enable machine external interrupts at the CPU level. */
    cpu_mask = RISCV_MIE_MEIE_BIT;
    __asm__ volatile ("csrs mie, %0" :: "r" (cpu_mask) : "memory");

    /* Global enable is deliberately the last CPU-side step. */
    cpu_mask = RISCV_MSTATUS_MIE_BIT;
    __asm__ volatile ("csrs mstatus, %0" :: "r" (cpu_mask) : "memory");

    return 0;
}

int interrupt_disable_source(uint8_t source)
{
    uint32_t source_mask;

    if (source >= VEGA_IRQ_SOURCE_COUNT)
    {
        return -1;
    }

    source_mask = (uint32_t)1u << source;
    VEGA_INTERRUPT_CONTROLLER->INTR_EN &= ~source_mask;
    __asm__ volatile ("fence" ::: "memory");
    return 0;
}

/* Called only by the assembly trap entry. Do not call this from main(). */
void interrupt_handler(void)
{
    uint32_t mcause = read_mcause();
    uint32_t cause_code;
    uint32_t pending;

    /* A synchronous exception is not a peripheral interrupt. */
    if ((mcause & RISCV_MCAUSE_INTERRUPT_BIT) == 0u)
    {
        for (;;)
        {
            __asm__ volatile ("wfi");
        }
    }

    cause_code = mcause & RISCV_MCAUSE_CODE_MASK;

    /* This initial layer handles the VEGA external interrupt controller only. */
    if (cause_code != RISCV_CAUSE_MACHINE_EXTERNAL_INTERRUPT)
    {
        /* Software/timer interrupts are not configured by this UART exercise. */
        for (;;)
        {
            __asm__ volatile ("wfi");
        }
    }

    __asm__ volatile ("fence" ::: "memory");
    pending = VEGA_INTERRUPT_CONTROLLER->INTR_STATUS;

    for (uint32_t source = 0; source < VEGA_IRQ_SOURCE_COUNT; ++source)
    {
        uint32_t source_mask = (uint32_t)1u << source;

        if ((pending & source_mask) != 0u)
        {
            interrupt_handler_t handler = irq_handlers[source];

            /* Never jump through an unregistered/null function pointer. */
            if (handler != (interrupt_handler_t)0)
            {
                handler();
            }
        }
    }

    /*
     * Do not write INTR_STATUS here. The supplied C-DAC driver only reads it;
     * acknowledgement/deassertion must be performed by the peripheral ISR.
     * For UART RX, that normally means draining the UART receive data register
     * according to the UART's documented interrupt/FIFO behavior.
     */
    __asm__ volatile ("fence" ::: "memory");
}
