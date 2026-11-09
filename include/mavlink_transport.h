#ifndef MAVLINK_TRANSPORT_H
#define MAVLINK_TRANSPORT_H

#include <stdint.h>

/*
 * Initialize UART0, RX/TX queues and interrupt handling.
 */
int mavlink_transport_init(uint32_t baud_rate);

/*
 * Queue raw bytes for transmission.
 *
 * Returns:
 *   0  success
 *  -1  invalid argument or transport not initialized
 *  -2  insufficient TX queue space
 *  -3  failed to enable UART TX interrupt
 */
int mavlink_transport_write(const uint8_t *data, uint16_t length);

/*
 * Retrieve one received byte.
 *
 * Returns:
 *   1  byte received
 *   0  no byte available
 */
int mavlink_transport_read_byte(uint8_t *byte);

#endif
