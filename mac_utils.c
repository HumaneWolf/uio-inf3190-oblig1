#include "mac_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>

/**
 * Gets the mac address for an interface.
 * Input:
 *      sock - The socket attached to the interface.
 *      mac - A pointer to the place to store the mac address. At least 6 bytes.
 *      interface_name - The name of the interface.
 */
void get_mac_addr(int sock, uint8_t mac[6], char *interface_name) {
    struct ifreq dev;
    strcpy(dev.ifr_name, interface_name);

    if (ioctl(sock, SIOCGIFHWADDR, &dev) == -1) {
        perror("get_mac_addr: ioctl()");
        exit(EXIT_FAILURE);
    }

    memcpy(mac, dev.ifr_hwaddr.sa_data, 6);
}

/**
 * Prints a mac address.
 * Input:
 *      mac - A pointer to where a mac address is stored.
 */
void print_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        printf("%x", mac[i]);
    }
    printf("\n");
}