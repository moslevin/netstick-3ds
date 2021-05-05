// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
/**
 * @brief net_util_connect Helper function; creates a TCP/IP connection to the
 * host.
 * @param serverAddr_ IP Address of the server (encoded as char string)
 * @param serverPort_ Port on the server to connect to
 * @return fd representing an active socket connection to the host, or -1 on error
 */
int net_util_connect(const char* serverAddr_, uint16_t serverPort_);

//---------------------------------------------------------------------------
/**
 * @brief net_util_encode_and_transmit Send a message to an active socket
 * connection using TLVC encoding.
 * @param sockFd_ fd representing the active socket connection
 * @param messageType_ Message ID associated with the data being sent
 * @param data_ Raw blob of data to send over the socket
 * @param dataLen_ Length of the data blob (in bytes)
 * @return true on success, false on socket error
 */
bool net_util_encode_and_transmit(int sockFd_, uint16_t messageType_, void* data_, size_t dataLen_);

#if defined(__cplusplus)
} // extern "C"
#endif
