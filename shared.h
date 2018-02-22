#ifndef _shared_h
#define _shared_h

#include "ethernet.h"

#include <stdarg.h>
#include <stdint.h>

// Debug print functions
void enable_debug_print();
void debug_print(char *str, ...);
void debug_print_frame(struct ethernet_frame * frame);


// Sending messages over the UNIX socket.
/**
 * Interface used to send messages over the UNIX socket between the daemon and the ping client/server.
 */
#define MAX_UX_MESSAGE_SIZE 1497

struct ux_message {
    uint8_t mip_addr; // Destination if sent to daemon. Source if sent from daemon.
    char msg[];
} __attribute__((packed));

void debug_print_message(struct ux_message * msg);


// MIP packet functions.
uint8_t mip_is_transport(uint32_t * packetHeader);
uint8_t mip_is_routing(uint32_t *packetHeader);
uint8_t mip_is_arp(uint32_t *packetHeader);
void mip_get_dest(uint32_t *packetHeader, uint8_t *output);
void mip_get_src(uint32_t *packetHeader, uint8_t *output);
void mip_get_payload_length(uint32_t *packetHeader, uint32_t *output);

void mip_build_header(
    uint8_t isTransport,
    uint8_t isRouting,
    uint8_t isArp,
    uint8_t destination,
    uint8_t source,
    uint32_t payloadLength,
    uint32_t * output
);

#endif