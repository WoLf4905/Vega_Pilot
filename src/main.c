#include <stdint.h>

#include "mavlink_transport.h"
#include "mavlink_protocol.h"
#include "timer.h"

#define MAVLINK_SYSTEM_ID       1U
#define MAVLINK_COMPONENT_ID    MAV_COMP_ID_AUTOPILOT1

#define UART_BAUD_RATE          115200U

/*
 * Send one HEARTBEAT message.
 */
static void send_heartbeat(void)
{
    mavlink_message_t heartbeat;

    mavlink_msg_heartbeat_pack(
        MAVLINK_SYSTEM_ID,
        MAVLINK_COMPONENT_ID,
        &heartbeat,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_GENERIC,
        0U,
        0U,
        MAV_STATE_STANDBY
    );

    (void)mavlink_protocol_send(&heartbeat);
}

/*
 * Application handler for incoming PING messages.
 *
 * This callback runs in main/application context,
 * not inside the UART ISR.
 */
static void handle_ping(const mavlink_message_t *message)
{
    mavlink_ping_t incoming_ping;
    mavlink_message_t ping_response;

    /*
     * Decode the incoming PING payload.
     */
    mavlink_msg_ping_decode(message, &incoming_ping);

    /*
     * Check whether the PING targets this autopilot
     * or is a broadcast request.
     */
    if ((incoming_ping.target_system != 0U &&
         incoming_ping.target_system != MAVLINK_SYSTEM_ID) ||
        (incoming_ping.target_component != 0U &&
         incoming_ping.target_component != MAVLINK_COMPONENT_ID))
    {
        return;
    }

    /*
     * Build the response addressed to the original sender.
     */
    mavlink_msg_ping_pack(
        MAVLINK_SYSTEM_ID,
        MAVLINK_COMPONENT_ID,
        &ping_response,
        incoming_ping.time_usec,
        incoming_ping.seq,
        message->sysid,
        message->compid
    );

    /*
     * Send the response through the protocol layer.
     */
    (void)mavlink_protocol_send(&ping_response);
}

int main(void)
{
    /*
     * STEP 1:
     * Initialize UART, ring buffers and interrupt controller.
     */
    if (mavlink_transport_init(UART_BAUD_RATE) != 0)
    {
        while (1)
        {
            __asm__ volatile ("wfi");
        }
    }

    /*
     * STEP 2:
     * Initialize the MAVLink protocol parser and handlers.
     */
    if (mavlink_protocol_init() != 0)
    {
        while (1)
        {
            __asm__ volatile ("wfi");
        }
    }

    /*
     * STEP 3:
     * Register the PING application handler.
     */
    if (mavlink_protocol_register_handler(
            MAVLINK_MSG_ID_PING,
            handle_ping) != 0)
    {
        while (1)
        {
            __asm__ volatile ("wfi");
        }
    }

    /*
     * STEP 4:
     * Initialize the periodic Timer 0 interrupt.
     */
    if (timer0_init_1hz() != 0)
    {
        while (1)
        {
            __asm__ volatile ("wfi");
        }
    }

    /*
     * STEP 5:
     * Send the first HEARTBEAT immediately.
     */
    send_heartbeat();

    /*
     * STEP 6:
     * Main application loop.
     */
    while (1)
    {
        /*
         * Receive, parse and dispatch complete MAVLink messages.
         */
        mavlink_protocol_process_rx();

        /*
         * Send periodic HEARTBEAT.
         */
        if (timer0_take_tick())
        {
            send_heartbeat();
        }
    }

    return 0;
}
