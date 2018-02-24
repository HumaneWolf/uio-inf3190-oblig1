#include "daemon.h"
#include "ethernet.h"
#include "shared.h"
#include "mac_utils.h"
#include "mip.h"

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
enum packet_waiting_status packetIsExpected = NOT_WAITING;

/**
 * Store the payload we are gonna send after receiving the mac address of an ARP lookup.
 */
unsigned char destinationMip;
char arpBuffer[MAX_PAYLOAD_SIZE] = {0};

/**
 * Store a linked list of all the connected interfaces, along with information about them.
 */
struct eth_interface *interfaces;

/**
 * Store whether this daemon has any MIP address available, and what the addresses are.
 */
char *myAddresses;

/**
 * Store a cache of what MAC address belongs to any MIP address.
 * Format:
 *      macCache[Mip Address] = Mac address.
 *      If mac address is all 0's, assume it is unknown, since that mac is mostly used for loopback.
 */
uint8_t macCache[256][6] = {0};

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
    unsigned char mip_addr = 0; // Mip address storage, for sendmsg and recvmsg.
    enum error errBuffer = 0; // To store and send errors between processes.
    char intBuffer[MAX_PACKET_SIZE] = {0}; // Internal communications buffer
    char extBuffer[MAX_PACKET_SIZE + sizeof(struct ethernet_frame)] = {0}; // External communications buffer

    // Packet/frame/event type decision tree.
    if (epctrl->events[n].data.fd == epctrl->unix_fd) {

        // Creating the iov and msghdr structs for receiving here.
        struct iovec iov[2];
        iov[0].iov_base = &intBuffer;
        iov[0].iov_len = MAX_PACKET_SIZE;

        iov[1].iov_base = &mip_addr;
        iov[1].iov_len = sizeof(mip_addr);

        iov[2].iov_base = &errBuffer;
        iov[2].iov_len = sizeof(errBuffer);

        struct msghdr message = {0};
        message.msg_iov = iov;
        message.msg_iovlen = 3;

        if (recvmsg(epctrl->events[n].data.fd, &message, 0) == -1) {
            perror("epoll_event: recvmsg()");
            exit(EXIT_FAILURE);
        }

        if (strlen(intBuffer) > MAX_PAYLOAD_SIZE) {
            errBuffer = TOO_LONG_PAYLOAD;
            if (recvmsg(epctrl->events[n].data.fd, &message, 0) == -1) {
                perror("epoll_event: recvmsg()");
                exit(EXIT_FAILURE);
            }
            return;
        }
        
        char isMipKnown = 0; // Used to store whether the mip is known after checking the cache.
        int i;
        for (i = 0; i < 6; i++) {
            if (macCache[mip_addr][i] != 0) {
                isMipKnown = 1;
            }
        }

        if (isMipKnown) {
            packetIsExpected = WAITING_DATA;

            struct eth_interface * tmp_interface = interfaces;
            while (tmp_interface) {
                struct ethernet_frame * eth_frame = (struct ethernet_frame*)&extBuffer;

                memcpy(eth_frame->destination, macCache[mip_addr], 6);
                memcpy(eth_frame->source, tmp_interface->mac, 6);
                eth_frame->protocol = ETH_P_MIP;

                mip_build_header(
                    1, 0, 0,
                    mip_addr,
                    tmp_interface->mip_addr,
                    strlen(intBuffer),
                    (uint32_t*)(eth_frame->msg)
                );

                memcpy((char*)(&eth_frame->msg[4]), &intBuffer, MAX_PAYLOAD_SIZE);

                send(tmp_interface->sock, &extBuffer, sizeof(extBuffer), 0);

                printf("Frame sent:\n");
                debug_print_frame(eth_frame);

                tmp_interface = tmp_interface->next;
            }
        } else {
            // Store the message we intend to send in the buffer.
            memcpy(arpBuffer, intBuffer, MAX_PAYLOAD_SIZE);
            destinationMip = mip_addr;
            packetIsExpected = WAITING_ARP;

            // Send ARP requests.
            struct eth_interface * tmp_interface = interfaces;
            while (tmp_interface) {
                struct ethernet_frame * eth_frame = (struct ethernet_frame*)&extBuffer;

                memset(eth_frame->destination, 0xFF, 6);
                memcpy(eth_frame->source, tmp_interface->mac, 6);
                eth_frame->protocol = ETH_P_MIP;

                mip_build_header(0, 0, 1, mip_addr, tmp_interface->mip_addr, 0, (uint32_t*)(eth_frame->msg));

                if (send(tmp_interface->sock, &extBuffer, sizeof(extBuffer), 0) == -1) {
                    perror("epoll_event: send()");
                    exit(EXIT_FAILURE);
                }

                debug_print("ARP frame sent:\n");
                debug_print_frame(eth_frame);

                tmp_interface = tmp_interface->next;
            }
        }
    } else {
        if (recv(epctrl->events[n].data.fd, &extBuffer, sizeof(extBuffer), 0) == -1) {
            perror("epoll_event: recv()");
            exit(EXIT_FAILURE);
        }
        struct ethernet_frame *eth_frame = (struct ethernet_frame *)&extBuffer; // Store eth frame in buffer.

        if (packetIsExpected == NOT_WAITING) { // If no packets are expected.
            if (mip_is_arp((uint32_t*)&(eth_frame->msg))) { //If ARP packet.
                char isMe = 0;
                struct eth_interface * tmp_interface = interfaces;
                while (tmp_interface) {
                    if (mip_get_dest((uint32_t *)&(eth_frame->msg)) == tmp_interface->mip_addr) {
                        isMe = 1;
                        break;
                    }
                }
                if (isMe) {
                    uint32_t arpResponseBuffer = 0;
                    mip_build_header(
                        0, 0, 0,
                        mip_get_src((uint32_t *)&(eth_frame->msg)),
                        mip_get_dest((uint32_t *)&(eth_frame->msg)),
                        0, &arpResponseBuffer);
                    if (send(epctrl->events[n].data.fd, &arpResponseBuffer, sizeof(arpResponseBuffer), 0) == -1) {
                        perror("epoll_event: send()");
                        exit(EXIT_FAILURE);
                    }

                    memcpy(macCache[mip_get_src((uint32_t *)&(eth_frame->msg))], eth_frame->source, 6);
                }
            } else { // If not ARP packet.
                debug_print("Unexpected packet received.");
                debug_print_frame((struct ethernet_frame*)&extBuffer);
            }
        } else if (packetIsExpected == WAITING_ARP) { // If ARP response packet is expected.
            memcpy(macCache[mip_get_src((uint32_t *)&(eth_frame->msg))], eth_frame->source, 6);
            
            struct eth_interface * tmp_interface = interfaces;
            while (tmp_interface) {
                memcpy(eth_frame->destination, macCache[mip_addr], 6);
                memcpy(eth_frame->source, tmp_interface->mac, 6);
                eth_frame->protocol = ETH_P_MIP;

                mip_build_header(
                    1, 0, 0,
                    destinationMip,
                    tmp_interface->mip_addr,
                    MAX_PAYLOAD_SIZE, (uint32_t*)(eth_frame->msg)
                );

                memcpy((char*)(&eth_frame->msg[4]), arpBuffer, MAX_PAYLOAD_SIZE);

                send(tmp_interface->sock, &extBuffer, sizeof(extBuffer), 0);

                debug_print("Frame sent:\n");
                debug_print_frame(eth_frame);
            }
              
        } else { // If data packet is expected.

            // Creating the iov and msghdr structs for sending data back here.
            struct iovec iov[2];
            iov[0].iov_base = &intBuffer;
            iov[0].iov_len = MAX_PAYLOAD_SIZE;

            iov[1].iov_base = &mip_addr;
            iov[1].iov_len = sizeof(mip_addr);

            iov[2].iov_base = &errBuffer;
            iov[2].iov_len = sizeof(errBuffer);

            struct msghdr message = {0};
            message.msg_iov = iov;
            message.msg_iovlen = 3;

            errBuffer = NO_ERROR;
            mip_addr = mip_get_src((uint32_t *)&(eth_frame->msg));
            memcpy(intBuffer, (char *)(&eth_frame->msg[4]), MAX_PAYLOAD_SIZE);

            if (sendmsg(epctrl->unix_fd, &message, 0) == -1) {
                perror("epoll_event: senmsg()");
                exit(EXIT_FAILURE);
            }
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
    int addrCount = 0; // Used in loop. Used to store addresses in the right spot.

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
        } else {
            myAddresses[addrCount] = (char)atoi(argv[i]);
            addrCount++;
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

    int tmp_addrNum = 0; // Counter used to assign MIP addresses.
    while (tmp_addr)
    {
        if (
            tmp_addr->ifa_addr
            && tmp_addr->ifa_addr->sa_family == AF_PACKET
            && !(tmp_addr->ifa_flags & IFF_LOOPBACK))
        {
            // If we have no more MIP addresses we can use, stop storing data about the interfaces,
            // since we can't use the interfaces anyway.
            if (!(myAddresses[tmp_addrNum])) {
                break;
            }
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
            sockaddr_net.sll_protocol = htons(ETH_P_MIP);
            sockaddr_net.sll_ifindex = if_nametoindex(tmp_addr->ifa_name);
            if (bind(sock, (struct sockaddr*)&sockaddr_net, sizeof(sockaddr_net)) == -1) {
                perror("main: bind(loop)");
                exit(EXIT_FAILURE);
            }

            tmp_interface->mip_addr = myAddresses[tmp_addrNum];

            tmp_interface->sock = sock;
            get_mac_addr(sock, tmp_interface->mac, tmp_addr->ifa_name);

            tmp_interface->next = interfaces;
            interfaces = tmp_interface;

            tmp_addrNum++; // Increase by one.

            debug_print("%s added with MIP addr %u.\n", tmp_addr->ifa_name, tmp_interface->mip_addr);
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