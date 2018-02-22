#include "shared.h"
#include "mac_utils.h"

#include <stdio.h>

char setting_debug = 0;

/**
 * Enables the debug printing for the application.
 */
void enable_debug_print() {
    setting_debug = 1;
}

/**
 * Formats and prints a line of text if, and only if, debug printing is enabled.
 * For arguments, see the printf documentation.
 */
void debug_print(char * str, ...) {
    if (!setting_debug) return;
    
    va_list args;
    va_start(args, str);
    vprintf(str, args);
}

/**
 * If debug printing is enabled; format and print a single ethernet frame.
 * frame - The ethernet frame struct to print.
 */
void debug_print_frame(struct ethernet_frame * frame) {
    if (!setting_debug) return;

    printf("Source MAC:\n");
    print_mac(frame->source);
    printf("Destination:\n");
    print_mac(frame->destination);
    printf("Protocol: %d\n", frame->protocol);
    printf("%s\n", frame->msg);
}

/**
 * If debug printing is enabled; format and print a single message send over the unix socket.
 */
void debug_print_message(struct ux_message *msg) {
    if (!setting_debug) return;

    printf("Address (to or from): %d\n", msg->mip_addr);
    printf("Message: %s\n", msg->msg);
}

//
// MIP PACKET RELATED FUNCTIONS
//

/**
 * Get whether it is a transport MIP packet.
 * packetHeader - A pointer to the packet header.
 * Return: 1 if transport, 0 if not.
 */
uint8_t mip_is_transport(uint32_t * packetHeader) {
    return (uint8_t)((*packetHeader >> 31) & 1);
}

/**
 * Get whether it is a routing MIP packet.
 * packetHeader - A pointer to the packet header.
 * Return: 1 if routing, 0 if not.
 */
uint8_t mip_is_routing(uint32_t *packetHeader) {
    return (uint8_t)((*packetHeader >> 30) & 1);
}

/**
 * Get whether it is a arp MIP packet.
 * packetHeader - A pointer to the packet header.
 * Return: 1 if arp, 0 if not.
 */
uint8_t mip_is_arp(uint32_t *packetHeader) {
    return (uint8_t)((*packetHeader >> 29) & 1);
}

/**
 * Get the destination MIP address from a MIP header.
 * packetHeader - A pointer to the packet header.
 * output - The location to output the MIP address.
 */
void mip_get_dest(uint32_t * packetHeader, uint8_t * output) {
    *output = (uint8_t)((*packetHeader >> 21) & 0x000000FF);
}

/**
 * Get the source MIP address from a MIP header.
 * packetHeader - A pointer to the packet header.
 * output - The location to output the MIP address.
 */
void mip_get_src(uint32_t * packetHeader, uint8_t * output) {
    *output = (uint8_t)((*packetHeader >> 13) & 0x000000FF);
}

/**
 * Get the payload length of a MIP packet.
 * packetHeader - A pointer to the packet header.
 * output - The location to output the length, in number of bytes.
 */
void mip_get_payload_length(uint32_t * packetHeader, uint32_t * output) {
    *output = ((*packetHeader >> 4) & 0x000001FF) * 4;
}

/**
 * Build a packet header based on the specified input, and place it in the specified output.
 * isTransport - 1 if transport, 0 otherwise.
 * isRouting - 1 if routing, 0 otherwise.
 * isArp - 1 if arp, 0 otherwise.
 * destination - Destination MIP address.
 * source - source MIP address.
 * payloadLength - Length of the payload.
 * ttl - The time to live (15).
 * output - Pointer to a location to store the result.
 */
void mip_build_header(
    uint8_t isTransport,
    uint8_t isRouting,
    uint8_t isArp,
    uint8_t destination,
    uint8_t source,
    uint32_t payloadLength,
    uint32_t * output
) {
    uint32_t result = {0};

    // For each: Add relevant bits, then shift far enough to make space for the next field.

    if (isTransport) {
        result = result | 1; // Transport bit.
    }
    result = result << 1;

    if (isRouting) {
        result = result | 1; // Routing bit.
    }
    result = result << 1;

    if (isRouting) {
        result = result | 1; // ARP bit.
    }
    result = result << 8;

    result = (result | destination) << 8; // Destination MIP addr.

    result = (result | source) << 9; // Source MIP addr.

    result = (result | (payloadLength / 4)) << 4; // Payload Length.

    result = result | 15; // TTL.

    *output = result; // Write changes.
}
