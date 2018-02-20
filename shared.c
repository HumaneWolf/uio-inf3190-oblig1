#include "shared.h"

#include <stdio.h>

char setting_debug = 0;

void enable_debug_print() {
    setting_debug = 1;
}

void debug_print(char* str, ...) {
    if (setting_debug) {
        va_list args;
        va_start(args, str);
        vprintf(str, args);
    }
}