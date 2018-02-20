#include "mac_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>

int get_mac_addr(int sock, char mac[6], char *interface_name) {
    struct ifreq dev;
    strcpy(dev.ifr_name, interface_name);

    if (ioctl(sock, SIOCGIFHWADDR, &dev) == -1) {
        perror("get_mac_addr: ioctl()");
        exit(EXIT_FAILURE);
    }

    memcpy(mac, dev.ifr_hwaddr.sa_data, 6);
    return 0;
}

void print_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        printf("%x", mac[i]);
    }
    printf("\n");
}