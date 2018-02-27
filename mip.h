#ifndef _mip_h
#define _mip_h

#include <stdint.h>

/**
 * Ethernet protocol number.
 */
#define ETH_P_MIP 0x88B5

// MIP packet functions.
uint8_t mip_is_transport(char *packetHeader);
uint8_t mip_is_routing(char *packetHeader);
uint8_t mip_is_arp(char *packetHeader);
uint8_t mip_get_dest(char *packetHeader);
uint8_t mip_get_src(char *packetHeader);
uint32_t mip_get_payload_length(char *packetHeader);

uint16_t mip_calc_payload_length(int length);

void mip_build_header(
    uint8_t isTransport,
    uint8_t isRouting,
    uint8_t isArp,
    uint8_t destination,
    uint8_t source,
    uint32_t payloadLength,
    char *output);

#endif