#include "ethernet.h"
#include "shared.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc <= 1) { //Not enough args for help.
        printf("Syntax: %s [-h] <Destination host> <Message> <Unix socket>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (!strcmp(argv[1], "-h")) { // Show help.
        printf("Syntax: %s [-h] <Destination host> <Message> <Unix socket>\n", argv[0]);
        printf("-h: Show help and exit.\n");
        return EXIT_SUCCESS;
    }

    if (argc < 4) { //Not enough args
        printf("Syntax: %s [-h] <Destination host> <Message> <Unix socket>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Message:
    char * msg = argv[2];

    // Socket path:
    char *sockpath = argv[3];

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
    char buffer[MAX_PACKET_SIZE] = {0};
    strcpy(buffer, msg);
    unsigned char mip_addr = (unsigned char)atoi(argv[1]);
    enum info infoBuffer = NO_ERROR;

    struct iovec iov[3];
    iov[0].iov_base = &mip_addr;
    iov[0].iov_len = sizeof(mip_addr);

    iov[1].iov_base = &infoBuffer;
    iov[1].iov_len = sizeof(infoBuffer);

    iov[2].iov_base = buffer;
    iov[2].iov_len = sizeof(buffer);

    struct msghdr message = {0};
    message.msg_iov = iov;
    message.msg_iovlen = 3;

    printf("Pinging %hhu..\n", mip_addr);

    if (sendmsg(sock, &message, 0) == -1) {
        perror("sendmsg()");
        exit(EXIT_FAILURE);
    }
    printf("Ping sent %s.\n", buffer);
    memset(&buffer, 0, sizeof(buffer));

    // Receive message.
    if (recvmsg(sock, &message, MSG_WAITALL) == -1) {
        perror("recvmsg()");
        exit(EXIT_FAILURE);
    }

    if (infoBuffer == NO_ERROR) { // If no error occured.
        printf("Ping received: %s\n", buffer);
    } else if (infoBuffer == TIMED_OUT) { // If the connection timed out.
        printf("Timed out.\n");
    } else if (infoBuffer == TOO_LONG_PAYLOAD) {
        printf("Payload too large.\n");
    } else {
        printf("Unknown error occured.\n");
    }

    close(sock);
}