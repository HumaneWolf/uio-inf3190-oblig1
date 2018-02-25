#ifndef _daemon_h
#define _daemon_h

#include <sys/epoll.h>
#include <sys/socket.h>

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
 * An enum object storing how the daemon can restore it's previous status.
 */
enum arp_restore_status {
    EXP_NO_RESP = 0, // Don't expect a response.
    EXP_DATA = 1, // Wait for data response.
    RESUME_LISTEN = 2 // Return to listening.
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
    int epoll_fd; // The file descriptor for the epoll.
    int sock_fd; // The file descriptor for the socket the ping server/client connects to.
    int unix_fd; // The file descriptor for the socket created when the ping server/client connects.
    socklen_t sockaddrlen; // Address length for sockaddr struct.
    struct sockaddr sockaddr; // Socket address struct for unix socket.
    struct epoll_event events[MAX_EVENTS];
};

#endif