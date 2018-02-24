#ifndef _daemon_h
#define _daemon_h

#include <sys/epoll.h>

/**
 * An enum object to store the status of the daemon at this moment.
 */
enum packet_waiting_status {
    NOT_WAITING = 0, // Not awaiting any packet.
    WAITING_ARP = 1, // Awaiting a single ARP response packet.
    WAITING_DATA = 2, // Awaiting a single data response packet.
    LISTENING = 3 // Listening to packets as a server.
};


/**
 * A linked list structure to store all the network interfaces in, with associated information.
 */
struct eth_interface {
    struct eth_interface *next;
    char* name;
    uint8_t mac[6];
    char mip_addr;
    int sock;
};

#define MAX_EVENTS 20
/**
 * EPOLL control structure.
 */
struct epoll_control {
    int epoll_fd;
    int unix_fd;
    struct epoll_event events[MAX_EVENTS];
};

#endif