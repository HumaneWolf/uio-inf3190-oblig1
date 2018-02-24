#ifndef _shared_h
#define _shared_h

#include "ethernet.h"

#include <stdarg.h>

// Types of error we can send.
enum error {
    NO_ERROR = 0,
    TOO_LONG_PAYLOAD = 1
};

// Debug print functions
void enable_debug_print();
void debug_print(char *str, ...);
void debug_print_frame(struct ethernet_frame * frame);

#endif