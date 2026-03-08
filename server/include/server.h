#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Platform-specific includes and definitions
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// For event-driven I/O
#include <sys/epoll.h>
#include <fcntl.h>

// OpenSSL headers for TLS
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "darray.h"

#define DEFAULT_PORT "4433"
#define INPUT_BUFFER_SIZE 4096
#define OUTPUT_BUFFER_SIZE 4096

#define MAX_EVENTS 8192

#define ERROR(msg) fprintf(stderr, "%s: %s\n", msg, strerror(errno))

#define set_nonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)
#define mark_client_for_close(c) ((c)->flags.closing = 1)

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Enums for error handling
typedef enum {
    HANDSHAKING,
    CONNECTED
} CLIENT_STATE;

typedef enum {
    OK = 0,
    TLS_BAD_CONTEXT = -1,
    TLS_BAD_CERT = -2,
    TLS_BAD_KEY = -3,
    CLIENT_INIT_FAIL = -4,
    CLIENT_HANDSHAKE_FAIL = -5,
    SEND_FAIL = -6,
    RECV_FAIL = -7,
    BUFFER_OVERFLOW = -8,
	REMOVE_CLIENT_FAIL = -9
} SERVER_STATUS;

typedef struct {
    uint8_t        state   : 1;
    uint8_t        closing : 1;
} ClientFlags;

typedef struct {
    int            socket;
    ClientFlags    flags;

    SSL           *ssl;

    size_t         index;      // index in the clients array
    size_t         in_len;

    size_t         out_len;
    size_t         out_sent;   // bytes already sent from out_buffer

    char           name[16];

    uint8_t        in_buffer[INPUT_BUFFER_SIZE];
    uint8_t        out_buffer[OUTPUT_BUFFER_SIZE];
} ClientTLS;


static inline void print_error_server(SERVER_STATUS err) {
    switch (err) {
    case TLS_BAD_CONTEXT:
        ERROR("TLS context initialization failed");
        break;
    case TLS_BAD_CERT:
        ERROR("Failed to load TLS certificate");
        break;
    case TLS_BAD_KEY:
        ERROR("Failed to load TLS private key");
        break;
    case CLIENT_INIT_FAIL:
        ERROR("SSL initialization failed");
		break;
    case CLIENT_HANDSHAKE_FAIL:
        ERROR("TLS handshake failed");
		break;
    case SEND_FAIL:
        ERROR("SSL send failed");
        break;
    case RECV_FAIL:
        ERROR("SSL receive failed");
        break;
    case BUFFER_OVERFLOW:
        ERROR("Buffer overflow: message too large");
        break;
    case REMOVE_CLIENT_FAIL:
        ERROR("Failed to remove client");
		break;
    default:
        ERROR("Unknown error");
    }
}