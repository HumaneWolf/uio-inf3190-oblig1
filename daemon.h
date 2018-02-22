#ifndef _daemon_h
#define _daemon_h

#include <sys/epoll.h>

struct eth_interface {
    struct eth_interface *next;
    char* name;
    uint8_t mac[6];
    char mip_addr;
    int sock;
};

#define MAX_EVENTS 15
/**
 * EPOLL control structure.
 */
struct epoll_control {
    int epoll_fd;
    int unix_fd;
    struct epoll_event events[MAX_EVENTS];
};

#endif