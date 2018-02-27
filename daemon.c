#include "daemon.h"
#include "ethernet.h"
#include "shared.h"
#include "mac_utils.h"
#include "mip.h"
#include "debug.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
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
enum arp_restore_status respBuffer;
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
 * Input:
 *      epctrl - Epoll controller struct.
 *      fd - The file descriptor to add.
 * Return:
 *      Returns 0 if successful.
 * Error:
 *      Will end the program in case of errors.
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
 * Input:
 *      epctrl - The epoll controller struct.
 *      n - The event counter, says which event to handle.
 * Affected by:
 *      packetIsExpected, respBuffer, macCache, destinationMip, arpBuffer.
 */
void epoll_event(struct epoll_control * epctrl, int n) {
    unsigned char mip_addr = 0; // Mip address storage, for sendmsg and recvmsg.
    enum info infoBuffer = 0; // To store and send errors and info between processes.
    char intBuffer[MAX_PACKET_SIZE] = {0}; // Internal communications buffer
    char extBuffer[MAX_PACKET_SIZE + sizeof(struct ethernet_frame)] = {0}; // External communications buffer

    if (epctrl->events[n].data.fd == epctrl->sock_fd) { // If the incoming event is creating a socket connection.
        if (epctrl->unix_fd && epctrl->unix_fd != -1) {
            close(epctrl->unix_fd);
        }

        // Accept connection.
        epctrl->unix_fd = accept(epctrl->sock_fd, &(epctrl->sockaddr), &(epctrl->sockaddrlen));
        if (epctrl->unix_fd == -1) {
            perror("epoll_event: accept()");
            exit(EXIT_FAILURE);
        }

        // Update the event so the rest of the decision making ends up correct.
        epctrl->events[n].data.fd = epctrl->unix_fd;

        // Add to epoll.
        epoll_add(epctrl, epctrl->unix_fd);
    }

    // Packet/frame/event type decision tree.
    if (epctrl->events[n].data.fd == epctrl->unix_fd) { // If the incoming event is on the established socket.
        // Creating the iov and msghdr structs for receiving here.
        struct iovec iov[3];
        iov[0].iov_base = &mip_addr;
        iov[0].iov_len = sizeof(mip_addr);

        iov[1].iov_base = &infoBuffer;
        iov[1].iov_len = sizeof(infoBuffer);

        iov[2].iov_base = intBuffer;
        iov[2].iov_len = sizeof(intBuffer);

        struct msghdr message = {0};
        message.msg_iov = iov;
        message.msg_iovlen = 3;

        if (recvmsg(epctrl->events[n].data.fd, &message, MSG_WAITALL) == -1) {
            perror("epoll_event: recvmsg()");
            exit(EXIT_FAILURE);
        }

        if (infoBuffer == LISTEN) { // If we are just gonna listen as a server.
            packetIsExpected = LISTENING;
            debug_print("Now listening to incoming connections.\n");
        } else if (infoBuffer == RESET) {
            packetIsExpected = NOT_WAITING;
            debug_print("Daemon has been reset, no longer listening.\n");
        } else { // If we are gonna send a message.
            if (strlen(intBuffer) > MAX_PAYLOAD_SIZE) {
                infoBuffer = TOO_LONG_PAYLOAD;
                if (sendmsg(epctrl->events[n].data.fd, &message, 0) == -1) {
                    perror("epoll_event: sendmsg()");
                    exit(EXIT_FAILURE);
                }
                return;
            }
            
            char isMipKnown = 0; // Used to store whether the mip is known after checking the cache.
            int i;
            for (i = 0; i < 6; i++) {
                if (macCache[mip_addr][i] != 0) {
                    isMipKnown = 1;
                    break;
                }
            }

            if (isMipKnown) {
                if (infoBuffer != NO_RESPONSE) {
                    packetIsExpected = WAITING_DATA;
                }

                struct eth_interface * tmp_interface = interfaces;
                while (tmp_interface) {
                    struct ethernet_frame * eth_frame = (struct ethernet_frame*)&extBuffer;

                    memcpy(eth_frame->destination, macCache[mip_addr], 6);
                    memcpy(eth_frame->source, tmp_interface->mac, 6);
                    eth_frame->protocol = htons(ETH_P_MIP);

                    uint16_t payloadLength = mip_calc_payload_length(strlen(intBuffer));

                    mip_build_header(
                        1, 0, 0,
                        mip_addr,
                        tmp_interface->mip_addr,
                        payloadLength,
                        eth_frame->msg
                    );

                    memcpy((char*)(&eth_frame->msg[4]), &intBuffer, payloadLength * 4);

                    send(tmp_interface->sock, &extBuffer, payloadLength * 4, 0);

                    debug_print("Frame sent:\n");
                    debug_print_frame(eth_frame);
                    debug_print("MIP To: %u, From: %u.\n", mip_addr, tmp_interface->mip_addr);

                    tmp_interface = tmp_interface->next;
                }
            } else {
                debug_print("Unknown MIP. Running arp.\n");
                // Store the message we intend to send in the buffer.
                memset(arpBuffer, 0, MAX_PAYLOAD_SIZE);
                memcpy(arpBuffer, intBuffer, MAX_PAYLOAD_SIZE);
                destinationMip = mip_addr;
                if (infoBuffer == NO_RESPONSE && packetIsExpected != LISTENING) {
                    respBuffer = EXP_NO_RESP;
                } else if (infoBuffer == NO_RESPONSE && packetIsExpected == LISTENING) {
                    respBuffer = RESUME_LISTEN;
                } else {
                    respBuffer = EXP_DATA;
                }
                packetIsExpected = WAITING_ARP;

                // Make ethernet frame
                struct ethernet_frame * eth_frame = (struct ethernet_frame*)&extBuffer;
                memset(eth_frame->destination, 0xFF, 6);
                eth_frame->protocol = htons(ETH_P_MIP);

                // Send ARP requests.
                struct eth_interface * tmp_interface = interfaces;
                while (tmp_interface) {
                    memcpy(eth_frame->source, tmp_interface->mac, 6);

                    mip_build_header(0, 0, 1, destinationMip, tmp_interface->mip_addr, 0, eth_frame->msg);

                    if (send(tmp_interface->sock, &extBuffer, 18, 0) == -1) {
                        perror("epoll_event: send()");
                        exit(EXIT_FAILURE);
                    }

                    debug_print("ARP frame sent on %s from %u:\n", tmp_interface->name, tmp_interface->mip_addr);
                    debug_print_frame(eth_frame);
                    debug_print("MIP To: %u, From: %u.\n", destinationMip, tmp_interface->mip_addr);

                    tmp_interface = tmp_interface->next;
                }
            }
        }
    } else {
        // Get headers.
        if (recv(epctrl->events[n].data.fd, &extBuffer, 18, 0) == -1) {
            perror("epoll_event: recv()");
            exit(EXIT_FAILURE);
        }
        struct ethernet_frame *eth_frame = (struct ethernet_frame *)&extBuffer; // Create an eth frame pointer to the buffer.

        char * mip_header = eth_frame->msg; // Store a direct pointer to the MIP header.
        char * mip_content = &(eth_frame->msg[4]); // Store a pointer to the MIP payload.

        // If there is a payload: Get it.
        int tmp_payloadLength = mip_get_payload_length(mip_header) * 4;
        if (tmp_payloadLength > 0) {
            if (recv(epctrl->events[n].data.fd, mip_content, tmp_payloadLength, 0) == -1) {
                perror("epoll_event: recv()");
                exit(EXIT_FAILURE);
            }
        }

        // Store source MIP in cache.
        memcpy(macCache[mip_get_src(mip_header)], eth_frame->source, 6);

        // Dump incoming frame.
        debug_print("Incoming frame:\n");
        debug_print_frame(eth_frame);
        debug_print(
            "Transport: %u, Routing: %u, ARP: %u\n",
            mip_is_transport(mip_header),
            mip_is_routing(mip_header),
            mip_is_arp(mip_header)
        );

        if (
            packetIsExpected == WAITING_ARP
            && !mip_is_transport(mip_header)
            && !mip_is_routing(mip_header)
            && !mip_is_arp(mip_header)
        ) { // If ARP response packet is expected.
            struct eth_interface * tmp_interface = interfaces;
            while (tmp_interface) {
                memcpy(eth_frame->destination, macCache[mip_get_src(mip_header)], 6);
                memcpy(eth_frame->source, tmp_interface->mac, 6);
                eth_frame->protocol = htons(ETH_P_MIP);

                uint16_t payloadLength = mip_calc_payload_length(strlen(intBuffer));

                memcpy(mip_content, arpBuffer, payloadLength * 4);

                mip_build_header(
                    1, 0, 0,
                    destinationMip,
                    tmp_interface->mip_addr,
                    payloadLength,
                    mip_header
                );

                send(tmp_interface->sock, &extBuffer, payloadLength * 4, 0);

                debug_print("Frame sent after ARP received:\n");
                debug_print_frame(eth_frame);

                tmp_interface = tmp_interface->next;
            }

            // Update status
            if (respBuffer == EXP_NO_RESP) {
                packetIsExpected = NOT_WAITING;
            } else if (respBuffer == EXP_DATA) {
                packetIsExpected = WAITING_DATA;
            } else if (respBuffer == RESUME_LISTEN) {
                packetIsExpected = LISTENING;
            }
              
        } else if (
            (packetIsExpected == WAITING_DATA
            || packetIsExpected == LISTENING)
            && mip_is_transport(mip_header)
            && !mip_is_routing(mip_header)
            && !mip_is_arp(mip_header)
        ) { // If data packet is expected.

            // Is it actually ment for us?
            char mentForUs = 0;
            struct eth_interface * tmp_interface = interfaces;
            while (tmp_interface) {
                if (
                    tmp_interface->sock == epctrl->events[n].data.fd
                    && tmp_interface->mip_addr == mip_get_dest(mip_header)
                ) {
                    mentForUs = 1;
                    break;
                }
                tmp_interface = tmp_interface->next;
            }
            if (!mentForUs) {
                return;
            }

            // Creating the iov and msghdr structs for sending data back here.
            struct iovec iov[3];
            iov[0].iov_base = &mip_addr;
            iov[0].iov_len = sizeof(mip_addr);

            iov[1].iov_base = &infoBuffer;
            iov[1].iov_len = sizeof(infoBuffer);

            iov[2].iov_base = intBuffer;
            iov[2].iov_len = sizeof(intBuffer);

            struct msghdr message = {0};
            message.msg_iov = iov;
            message.msg_iovlen = 3;

            infoBuffer = NO_ERROR;
            mip_addr = mip_get_src(mip_header);
            memcpy(intBuffer, mip_content, MAX_PAYLOAD_SIZE);

            if (sendmsg(epctrl->unix_fd, &message, 0) == -1) {
                perror("epoll_event: senmsg()");
                exit(EXIT_FAILURE);
            }

            debug_print("Send to process.\n");

            // Update status
            if (packetIsExpected == WAITING_DATA) {
                packetIsExpected = NOT_WAITING;
            }
        } else { // No packet of this kind is expected.
            if (mip_is_arp(mip_header)) { // If ARP packet.
                char isMe = 0;
                struct eth_interface * tmp_interface = interfaces;
                while (tmp_interface) {
                    if (mip_get_dest(mip_header) == tmp_interface->mip_addr) {
                        isMe = 1;
                        break;
                    }
                    tmp_interface = tmp_interface->next;
                }
                printf("IsMe %d\n", isMe);
                if (isMe) {
                    mip_build_header(
                        0, 0, 0,
                        mip_get_src(mip_header),
                        tmp_interface->mip_addr,
                        0,
                        eth_frame->msg
                    );
                    
                    uint8_t tmp_mac[6] = {0};
                    memcpy(&tmp_mac, eth_frame->source, 6);
                    memcpy(eth_frame->source, eth_frame->destination, 6);
                    memcpy(eth_frame->destination, &tmp_mac, 6);

                    if (send(epctrl->events[n].data.fd, &extBuffer, 18, 0) == -1) {
                        perror("epoll_event: send()");
                        exit(EXIT_FAILURE);
                    }

                    debug_print("Sent ARP response:\n");
                    debug_print_frame(eth_frame);
                }
            } else { // If not ARP packet.
                debug_print("Unexpected packet received.\n");
            }
        }
    }
}

/**
 * Main method.
 * Affected by:
 *      packetIsExpected, myAddresses.
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
            printf("Syntax: %s [-h] [-d] <unix_socket> [MIP addresses]\n", argv[0]);
            printf("-h: Show help and exit.\n");
            printf("-d: Debug mode.\n");
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

    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, sockpath);

    // Unlink/delete old socket file before creating the new one.
    unlink(sockpath);

    if (bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        perror("main: bind()");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 100)) {
        perror("main: listen()");
        exit(EXIT_FAILURE);
    }

    // Create EPOLL.
    struct epoll_control epctrl;
    epctrl.sock_fd = sock;
    epctrl.sockaddrlen = sizeof(sockaddr);
    memcpy(&(epctrl.sockaddr), &sockaddr, sizeof(sockaddr));
    epctrl.epoll_fd = epoll_create(10);

    epoll_add(&epctrl, epctrl.sock_fd);

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

            struct sockaddr_ll sockaddr_net;
            sockaddr_net.sll_family = AF_PACKET;
            sockaddr_net.sll_protocol = htons(ETH_P_ALL);
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

            epoll_add(&epctrl, sock);

            tmp_addrNum++; // Increase by one.

            debug_print("%s added with MIP addr %u.\n", tmp_addr->ifa_name, tmp_interface->mip_addr);
        }

        tmp_addr = tmp_addr->ifa_next;
    }
    freeifaddrs(addrs);

    printf("Ready to serve.\n");

    // Serve. The epoll_wait timeout makes this a "pulse" loop, meaning all periodic updates in the daemon
    // can be done from here.
    while (1) {
        int nfds, n;
        nfds = epoll_wait(epctrl.epoll_fd, epctrl.events, MAX_EVENTS, 1000); // Max waiting time = 1 sec.
        if (nfds == -1) {
            perror("main: epoll_wait()");
            exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; n++) {
            // Handle event.
            epoll_event(&epctrl, n);
        }

        // Time out. If the daemon is waiting, not serving, and no events happened.
        if (
            (packetIsExpected == WAITING_ARP || packetIsExpected == WAITING_DATA)
            && nfds == 0
        ) {
            // Defining the necessary variables to send back to the UNIX socket client.
            char intBuffer = {0};
            char mip_addr = 0;
            enum info infoBuffer = TIMED_OUT;

            packetIsExpected = NOT_WAITING;

            struct iovec iov[3];
            iov[0].iov_base = &mip_addr;
            iov[0].iov_len = sizeof(mip_addr);

            iov[1].iov_base = &infoBuffer;
            iov[1].iov_len = sizeof(infoBuffer);

            iov[2].iov_base = &intBuffer;
            iov[2].iov_len = sizeof(intBuffer);

            struct msghdr message = {0};
            message.msg_iov = iov;
            message.msg_iovlen = 3;

            if (sendmsg(epctrl.unix_fd, &message, 0) == -1) {
                perror("main: sendmsg()");
                exit(EXIT_FAILURE);
            }

            debug_print("Connection timed out.\n");
        }
        debug_print("Epoll timed out, 1s pulse loop done, events: %d\n", nfds);
    }

    // Close unix socket.
    close(epctrl.unix_fd);
    close(epctrl.sock_fd);

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