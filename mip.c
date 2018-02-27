#include "mip.h"

#include <arpa/inet.h>
#include <math.h>
#include <string.h>

/**
 * Get whether it is a transport MIP packet.
 * packetHeader - A pointer to the packet header.
 * Return: 1 if transport, 0 if not.
 */
uint8_t mip_is_transport(char *packetHeader) {
    uint32_t temp;
    memcpy(&temp, packetHeader, 4);
    return (uint8_t)((temp >> 31) & 1);
}

/**
 * Get whether it is a routing MIP packet.
 * packetHeader - A pointer to the packet header.
 * Return: 1 if routing, 0 if not.
 */
uint8_t mip_is_routing(char *packetHeader) {
    uint32_t temp;
    memcpy(&temp, packetHeader, 4);
    return (uint8_t)((temp >> 30) & 1);
}

/**
 * Get whether it is a arp MIP packet.
 * packetHeader - A pointer to the packet header.
 * Return: 1 if arp, 0 if not.
 */
uint8_t mip_is_arp(char *packetHeader) {
    uint32_t temp;
    memcpy(&temp, packetHeader, 4);
    return (uint8_t)((temp >> 29) & 1);
}

/**
 * Get the destination MIP address from a MIP header.
 * packetHeader - A pointer to the packet header.
 * output - The location to output the MIP address.
 */
uint8_t mip_get_dest(char *packetHeader) {
    uint32_t temp;
    memcpy(&temp, packetHeader, 4);
    return (uint8_t)((temp >> 21) & 0xFF);
}

/**
 * Get the source MIP address from a MIP header.
 * packetHeader - A pointer to the packet header.
 * output - The location to output the MIP address.
 */
uint8_t mip_get_src(char *packetHeader) {
    uint32_t temp;
    memcpy(&temp, packetHeader, 4);
    return (uint8_t)((temp >> 13) & 0xFF);
}

/**
 * Get the payload length of a MIP packet.
 * packetHeader - A pointer to the packet header.
 * output - The location to output the length, in number of bytes.
 */
uint32_t mip_get_payload_length(char *packetHeader) {
    uint32_t temp;
    memcpy(&temp, packetHeader, 4);
    return ntohs((*packetHeader >> 4) & 0x000001FF) * 4;
}

/**
 * Calculate the length of the payload, in 4 byte groups.
 */
uint16_t mip_calc_payload_length(int length) {
    return (uint16_t)ceilf(length / 4);
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
    char *output
) {
    uint32_t result = 0;

    // For each: Add relevant bits, then shift far enough to make space for the next field.

    if (isTransport) {
        result = result | 1; // Transport bit.
    }
    result = result << 1;

    if (isRouting) {
        result = result | 1; // Routing bit.
    }
    result = result << 1;

    if (isArp) {
        result = result | 1; // ARP bit.
    }
    result = result << 8;

    result = (result | destination) << 8; // Destination MIP addr.

    result = (result | source) << 9; // Source MIP addr.

    result = (result | htons(payloadLength & 0x01FF)) << 4; // Payload Length.

    result = result | 0xF; // TTL.

    // Ensure the bytes are in the right order, not swapped due to endian-ness.
    result = htonl(result);

    memcpy(output, &result, 4);
}