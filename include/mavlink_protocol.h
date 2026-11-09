#ifndef MAVLINK_PROTOCOL_H
#define MAVLINK_PROTOCOL_H

#include <stdint.h>

#include "common/mavlink.h"

/*
 * Application callback for a complete, valid MAVLink message.
 *
 * The message pointer is temporary: the application must not
 * retain it after the callback returns.
 */
typedef void (*mavlink_message_handler_t)(
    const mavlink_message_t *message
);

/*
 * Initialize the MAVLink parser and handler table.
 */
int mavlink_protocol_init(void);

/*
 * Register an application handler for a MAVLink message ID.
 *
 * Returns:
 *   0  success
 *  -1  invalid argument or protocol not initialized
 *  -2  handler table full
 */
int mavlink_protocol_register_handler(
    uint32_t message_id,
    mavlink_message_handler_t handler
);

/*
 * Serialize a MAVLink message and send its bytes
 * through the transport layer.
 */
int mavlink_protocol_send(const mavlink_message_t *message);

/*
 * Drain received transport bytes, parse them, and dispatch
 * each complete CRC-valid message to its registered handler.
 */
void mavlink_protocol_process_rx(void);

#endif
