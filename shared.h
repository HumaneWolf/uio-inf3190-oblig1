#ifndef _shared_h
#define _shared_h

// Types of info/error/actions we can send.
enum info {
    NO_ERROR = 0, // No error, no action, nothing special about this request.
    TOO_LONG_PAYLOAD = 1, // Error: The included payload is too long.
    TIMED_OUT = 2, // Error: The quest timed out.
    LISTEN = 3, // Action: Listen to any incoming packets and send them to me.
    RESET = 4 // Action: Reset, stop listening.
};

#endif