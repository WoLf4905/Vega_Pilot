#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/*
 * Configure and start ARIES V3 Timer 0
 * with a nominal one-second periodic interval.
 *
 * Returns 0 on success, -1 on failure.
 */
int timer0_init_1hz(void);

/*
 * Check whether a timer event has occurred.
 *
 * Returns:
 *   1 = one or more timer events occurred
 *   0 = no pending event
 *
 * Reading this function clears the pending software flag.
 */
int timer0_take_tick(void);

#endif
