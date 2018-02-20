#ifndef _ethernet_h
#define _ethernet_h

#include <stdint.h>

#define ETH_P_MIP 0x88B5

struct ethernet_frame {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t protocol;
    char msg[];
} __attribute__((packed));

#endif