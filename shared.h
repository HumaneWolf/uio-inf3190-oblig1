#ifndef _shared_h
#define _shared_h

#include "ethernet.h"

#include <stdarg.h>

// Types of error we can send.
enum info {
    NO_ERROR = 0, // No error, no action, nothing special about this request.
    TOO_LONG_PAYLOAD = 1, // Error: The included payload is too long.
    TIMED_OUT = 2, // Error: The quest timed out.
    LISTEN = 3 // Action: Listen to any incoming packets and send them to me.
};

// Debug print functions
void enable_debug_print();
void debug_print(char *str, ...);
void debug_print_frame(struct ethernet_frame * frame);

#endif