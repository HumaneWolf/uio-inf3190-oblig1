#include "ethernet.h"
#include "shared.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc <= 1) { //Not enough args
        printf("Syntax: %s [-h] <Unix socket>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (!strcmp(argv[1], "-h")) { // Show help.
        printf("Syntax: %s [-h] <Unix socket>\n", argv[0]);
        printf("-h: Show help and exit.\n");
        return EXIT_SUCCESS;
    }

    // Socket path:
    char *sockpath = argv[1];

    // Create socket.
    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock == -1) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    // Connect it.
    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, sockpath);
    
    if (connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1) {
        perror("connect()");
        exit(EXIT_FAILURE);
    }

    // Variables for sendmsg and recvmsg.
    char buffer[MAX_PAYLOAD_SIZE] = {0};
    char mip_addr = 0;
    enum info infoBuffer = LISTEN;

    struct iovec iov[3];
    iov[0].iov_base = &buffer;
    iov[0].iov_len = MAX_PACKET_SIZE;

    iov[1].iov_base = &mip_addr;
    iov[1].iov_len = sizeof(mip_addr);

    iov[2].iov_base = &infoBuffer;
    iov[2].iov_len = sizeof(infoBuffer);

    struct msghdr message = {0};
    message.msg_iov = iov;
    message.msg_iovlen = 3;

    if (sendmsg(sock, &message, 0) == -1) {
        perror("sendmsg()");
        exit(EXIT_FAILURE);
    }
    printf("Now listening to connections.\n");

    while (1) {
        // Receive message.
        if (recvmsg(sock, &message, 0) == -1) {
            perror("recvmsg()");
            exit(EXIT_FAILURE);
        }

        // Print it.
        printf("Ping received: %s \n", buffer);

        // Prepare to send a pong back.
        memset(&buffer, 0, MAX_PAYLOAD_SIZE);
        buffer[0] = 'P';
        buffer[1] = 'o';
        buffer[2] = 'n';
        buffer[3] = 'g';
        buffer[4] = '!';
        buffer[5] = '\0';
        infoBuffer = NO_ERROR;

        // Send it.
        if (sendmsg(sock, &message, 0) == -1) {
            perror("sendmsg()");
            exit(EXIT_FAILURE);
        }

        // Tell the daemon to listen again.
        infoBuffer = LISTEN;
        memset(&buffer, 0, MAX_PAYLOAD_SIZE);
        if (sendmsg(sock, &message, 0) == -1) {
            perror("sendmsg()");
            exit(EXIT_FAILURE);
        }
    }

    close(sock);
}