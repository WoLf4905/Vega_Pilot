#include "mavlink_protocol.h"
#include "mavlink_transport.h"

#include <stdint.h>

/*
 * Fixed-size handler table.
 *
 * We use a small table because this firmware initially
 * handles only a limited number of message types.
 */
#define MAVLINK_HANDLER_CAPACITY 8U

typedef struct
{
    uint32_t message_id;
    mavlink_message_handler_t handler;
} mavlink_handler_entry_t;

static mavlink_handler_entry_t handlers[MAVLINK_HANDLER_CAPACITY];

static uint8_t protocol_initialized = 0U;

/*
 * Parser state.
 *
 * The MAVLink parser remembers partial frame state
 * between successive calls to mavlink_parse_char().
 */
static mavlink_message_t rx_message;
static mavlink_status_t rx_status;

/*
 * Initialize the protocol layer.
 *
 * Call this after mavlink_transport_init().
 */
int mavlink_protocol_init(void)
{
    for (uint32_t i = 0U; i < MAVLINK_HANDLER_CAPACITY; ++i)
    {
        handlers[i].message_id = 0U;
        handlers[i].handler = 0;
    }

    protocol_initialized = 1U;

    return 0;
}

/*
 * Register or replace a handler.
 */
int mavlink_protocol_register_handler(
    uint32_t message_id,
    mavlink_message_handler_t handler)
{
    uint32_t free_slot = MAVLINK_HANDLER_CAPACITY;

    if (!protocol_initialized || handler == 0)
    {
        return -1;
    }

    /*
     * MAVLink message IDs are 24-bit values.
     */
    if (message_id > 0x00FFFFFFUL)
    {
        return -1;
    }

    /*
     * First search for an existing registration.
     */
    for (uint32_t i = 0U; i < MAVLINK_HANDLER_CAPACITY; ++i)
    {
        if (handlers[i].handler != 0 &&
            handlers[i].message_id == message_id)
        {
            handlers[i].handler = handler;
            return 0;
        }

        if (handlers[i].handler == 0 &&
            free_slot == MAVLINK_HANDLER_CAPACITY)
        {
            free_slot = i;
        }
    }

    /*
     * No existing handler: use a free slot.
     */
    if (free_slot == MAVLINK_HANDLER_CAPACITY)
    {
        return -2;
    }

    handlers[free_slot].message_id = message_id;
    handlers[free_slot].handler = handler;

    return 0;
}

/*
 * Dispatch a complete MAVLink message to its handler.
 */
static void dispatch_message(const mavlink_message_t *message)
{
    for (uint32_t i = 0U; i < MAVLINK_HANDLER_CAPACITY; ++i)
    {
        if (handlers[i].handler != 0 &&
            handlers[i].message_id == message->msgid)
        {
            handlers[i].handler(message);
            return;
        }
    }

    /*
     * No handler registered for this message ID.
     * Ignore it for now.
     */
}

/*
 * Serialize a MAVLink message and send it through
 * the byte-oriented transport layer.
 */
int mavlink_protocol_send(const mavlink_message_t *message)
{
    uint8_t packet[MAVLINK_MAX_PACKET_LEN];
    uint16_t packet_length;

    if (!protocol_initialized || message == 0)
    {
        return -1;
    }

    /*
     * Convert the MAVLink message structure into wire bytes.
     *
     * This performs MAVLink framing and checksum generation.
     */
    packet_length = mavlink_msg_to_send_buffer(packet, message);

    if (packet_length == 0U ||
        packet_length > MAVLINK_MAX_PACKET_LEN)
    {
        return -1;
    }

    /*
     * The protocol layer hands raw bytes to transport.
     */
    return mavlink_transport_write(packet, packet_length);
}

/*
 * Receive bytes and feed them to the MAVLink parser.
 */
void mavlink_protocol_process_rx(void)
{
    uint8_t byte;
    uint8_t framing_result;

    if (!protocol_initialized)
    {
        return;
    }

    /*
     * Drain all currently queued RX bytes.
     */
    while (mavlink_transport_read_byte(&byte))
    {
        /*
         * Feed exactly one byte into the MAVLink parser.
         */
        framing_result = mavlink_parse_char(
            MAVLINK_COMM_0,
            byte,
            &rx_message,
            &rx_status
        );

        /*
         * Dispatch only complete, valid MAVLink frames.
         */
        if (framing_result == MAVLINK_FRAMING_OK)
        {
            dispatch_message(&rx_message);
        }
    }
}
