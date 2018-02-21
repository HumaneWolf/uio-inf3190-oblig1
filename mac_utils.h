#ifndef _mac_utils_h
#define _mac_utils_h

#include <stdint.h>

int get_mac_addr(int sock, uint8_t mac[6], char *interface_name);

void print_mac(uint8_t mac[6]);

#endif