#ifndef _daemon_h
#define _daemon_h

struct eth_interface {
    struct eth_interface *next;
    char* name;
};

#endif