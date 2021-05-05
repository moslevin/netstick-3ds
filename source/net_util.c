// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "net_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "slip.h"
#include "tlvc.h"

//---------------------------------------------------------------------------
bool net_util_encode_and_transmit(int sockFd_, uint16_t messageType_, void* data_, size_t dataLen_)
{
    tlvc_data_t tlvc = {};
    tlvc_encode_data(&tlvc, messageType_, dataLen_, data_);

    slip_encode_message_t* encode = slip_encode_message_create(dataLen_);
    slip_encode_begin(encode);

    uint8_t* raw = (uint8_t*)&tlvc.header;
    for (size_t i = 0; i < sizeof(tlvc.header); i++) { slip_encode_byte(encode, *raw++); }

    raw = (uint8_t*)tlvc.data;
    for (size_t i = 0; i < tlvc.dataLen; i++) { slip_encode_byte(encode, *raw++); }

    raw = (uint8_t*)&tlvc.footer;
    for (size_t i = 0; i < sizeof(tlvc.footer); i++) { slip_encode_byte(encode, *raw++); }

    slip_encode_finish(encode);

    int toWrite = encode->index;
    raw         = encode->encoded;

    bool died = false;

    int nWritten = send(sockFd_, raw, toWrite, MSG_DONTWAIT);
    if (nWritten == -1) {
        printf("socket error: %d\n", errno);
        died = true;
    }

    slip_encode_message_destroy(encode);

    if (died) {
        printf("socket died during write\n");
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
// Attempt to connect to the server, return open socket fd on success.
int net_util_connect(const char* serverAddr_, uint16_t serverPort_)
{
    printf("connecting to %s:%d\n", serverAddr_, serverPort_);

    // Create the client socket address
    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        printf("error connecting socket: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    // Connect to the server
    struct sockaddr_in addr = {};

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, serverAddr_, &(addr.sin_addr));
    addr.sin_port = htons(serverPort_);

    int rc = connect(sockFd, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        printf("error connecting to server: %d (%s)\n", errno, strerror(errno));
        close(sockFd);
        return -1;
    }
    return sockFd;
}

