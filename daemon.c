#include "daemon.h"

#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>

int main() {
    struct ifaddrs *addrs, *tmp;

    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp)
    {
        if (
            tmp->ifa_addr
            && tmp->ifa_addr->sa_family == AF_PACKET
            && !(tmp->ifa_flags & IFF_LOOPBACK))
        {
            printf("%s\n", tmp->ifa_name);
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
}