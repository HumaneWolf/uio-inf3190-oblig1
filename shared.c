#include "shared.h"
#include "mac_utils.h"

#include <stdio.h>

char setting_debug = 0;

/**
 * Enables the debug printing for the application.
 */
void enable_debug_print() {
    setting_debug = 1;
}

/**
 * Formats and prints a line of text if, and only if, debug printing is enabled.
 * For arguments, see the printf documentation.
 */
void debug_print(char * str, ...) {
    if (!setting_debug) return;
    
    va_list args;
    va_start(args, str);
    vprintf(str, args);
}

/**
 * If debug printing is enabled; format and print a single ethernet frame.
 * frame - The ethernet frame struct to print.
 */
void debug_print_frame(struct ethernet_frame * frame) {
    if (!setting_debug) return;

    printf("Source MAC:\n");
    print_mac(frame->source);
    printf("Destination:\n");
    print_mac(frame->destination);
    printf("Protocol: %d\n", frame->protocol);
    printf("%s\n", frame->msg);
}

