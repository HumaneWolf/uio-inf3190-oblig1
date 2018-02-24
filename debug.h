#ifndef _debug_h
#define _debug_h

#include "ethernet.h"

#include <stdarg.h>

// Debug print functions
void enable_debug_print();
void debug_print(char *str, ...);
void debug_print_frame(struct ethernet_frame *frame);

#endif