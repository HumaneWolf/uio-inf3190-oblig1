#include "daemon.h"
#include "ethernet.h"
#include "shared.h"
#include "mac_utils.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * Store whether or not the daemon is expecting a packet now or not.
 */
char packetIsExpected = 0;

/**
 * Store a cache of what MAC address belongs to any MIP address.
 * Format:
 *      macCache[Mip Address] = Mac address.
 *      If mac address is all 0's, assume it is unknown, since that mac is mostly used for loopback.
 */
//uint8_t macCache[256][7] = {0};

/**
 * Add a file descriptor to the epoll.
 * epctrl - Epoll controller struct.
 * fd - The file descriptor to add.
 * Returns 0 if successful.
 */
int epoll_add(struct epoll_control *epctrl, int fd) {
    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epctrl->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_add: epoll_ctl()");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Handle an incoming connection event from any socket.
 * epctrl - The epoll controller struct.
 * n - The event to handle.
 * No return value.
 */
void epoll_event(struct epoll_control * epctrl, int n) {
    //char intBuffer[MAX_UX_MESSAGE_SIZE] = {0}; // Internal communications
    char extBuffer[MAX_FRAME_SIZE] = {0}; // External communications

    if (epctrl->events[n].data.fd == epctrl->unix_fd) {
        // Message from unix socket.
    } else {
        if (!packetIsExpected) {
            ssize_t ret = recv(epctrl->events[n].data.fd, &extBuffer, MAX_FRAME_SIZE, 0);
            if (ret == -1) {
                perror("epoll_event: recv()");
                exit (EXIT_SUCCESS);
            }
            debug_print("Unexpected packet received.");
            debug_print_frame((struct ethernet_frame*)&extBuffer);
        } else if (packetIsExpected == 1) {
            // If ARP packet is expected.
        } else {
            // If data packet is expected.
        }
    }
}

/**
 * Main method.
 */
int main(int argc, char * argv[]) {
    // Args count check
    if (argc <= 1) {
        printf("Syntax: %s [-h] [-d] <unix_socket> [MIP addresses]\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Variables
    char* sockpath = {0};
    struct eth_interface *interfaces; // Store a linked list of all interfaces.
    char hasAddr = 0; // Store whether or not this daemon has a MIP address at all.
    char* myAddresses; // Store a list of all MIP addresses assigned to this daemon.
    int startAddr = 0; // Used in loop.

    // Args parsing.
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            printf("Help\n");
            exit(EXIT_SUCCESS);
        } else if (!strcmp(argv[i], "-d")) {
            enable_debug_print();
            debug_print("Debug mode enabled.\n");
        } else if (!sockpath) {
            sockpath = argv[i];

            myAddresses = calloc(argc - i + 1, 0);
            startAddr = i;
        } else {
            myAddresses[i - startAddr] = (char)atoi(argv[i]);
            hasAddr = 1;
        }
    }
    if (!sockpath) {
        printf("Syntax: %s [-h] [-d] <unix_socket> [MIP addresses]\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    // Create the UNIX socket.
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1) {
        perror("main: socket()");
        exit(EXIT_FAILURE);
    }

    if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
        perror("main: fcntl(setting socket nonblocking)");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, sockpath);

    if (bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        perror("main: bind()");
        exit(EXIT_FAILURE);
    }

    if (unlink(sockpath) == -1) {
        perror("main: unlink()");
        printf("Daemon will continue, but UNIX socket will not be removed after completion.\n");
    }

    if (listen(sock, 100)) {
        perror("main: listen()");
        exit(EXIT_FAILURE);
    }

    // Create EPOLL.
    struct epoll_control epctrl;
    epctrl.unix_fd = sock;
    epctrl.epoll_fd = epoll_create(10);

    if (epctrl.epoll_fd == -1) {
        perror("main: epoll_create()");
        exit(EXIT_FAILURE);
    }

    // Create sockets and save each network interface to a list.
    struct ifaddrs * addrs, * tmp_addr;
    struct eth_interface * tmp_interface;

    getifaddrs(&addrs);
    tmp_addr = addrs;

    while (tmp_addr)
    {
        if (
            tmp_addr->ifa_addr
            && tmp_addr->ifa_addr->sa_family == AF_PACKET
            && !(tmp_addr->ifa_flags & IFF_LOOPBACK))
        {
            tmp_interface = calloc(0, sizeof(struct eth_interface));

            tmp_interface->name = calloc(0, strlen(tmp_addr->ifa_name));
            strcpy(tmp_interface->name, tmp_addr->ifa_name);

            // Create socket for the interface.
            sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
            if (sock == -1) {
                perror("main: socket()");
                exit(EXIT_FAILURE);
            }

            if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
                perror("main: fcntl(setting socket nonblocking)");
                exit(EXIT_FAILURE);
            }

            struct sockaddr_ll sockaddr_net;
            sockaddr_net.sll_family = AF_PACKET;
            sockaddr_net.sll_protocol = htons(ETH_P_ALL);
            sockaddr_net.sll_ifindex = if_nametoindex(tmp_addr->ifa_name);
            if (bind(sock, (struct sockaddr*)&sockaddr_net, sizeof(sockaddr_net)) == -1) {
                perror("main: bind(loop)");
                exit(EXIT_FAILURE);
            }

            tmp_interface->has_mip_addr = 0;
            if (hasAddr) {
                tmp_interface->has_mip_addr = 1;
                tmp_interface->mip_addr = myAddresses[0];
            }

            tmp_interface->sock = sock;
            get_mac_addr(sock, tmp_interface->mac, tmp_addr->ifa_name);

            tmp_interface->next = interfaces;
            interfaces = tmp_interface;

            debug_print("%s added.\n", tmp_addr->ifa_name);
        }

        tmp_addr = tmp_addr->ifa_next;
    }
    freeifaddrs(addrs);

    printf("Ready to serve.");

    // Serve
    while (1) {
        int nfds, n;
        nfds = epoll_wait(epctrl.epoll_fd, epctrl.events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("main: epoll_wait()");
            exit(EXIT_FAILURE);
        }

        debug_print("Handling connection: %d\n", nfds);

        for (n = 0; n < nfds; n++) {
            // Handle event.
            epoll_event(&epctrl, n);
        }
        break;
    }

    // Close unix socket.
    close(epctrl.unix_fd);

    // Close eth sockets and clean up memory.
    tmp_interface = interfaces;
    while (interfaces) {
        close(tmp_interface->sock);
        free(tmp_interface->name);
        interfaces = tmp_interface->next;
        free(tmp_interface);
    }

    return EXIT_SUCCESS;
}