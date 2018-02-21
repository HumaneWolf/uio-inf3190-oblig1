#ifndef _shared_h
#define _shared_h

#include <stdarg.h>
#include <stdint.h>

void enable_debug_print();

void debug_print(char *str, ...);

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