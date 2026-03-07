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

#define DEFAULT_PORT "4444"
#define MAX_MESSAGE_SIZE 4096
#define INPUT_BUFFER_SIZE 4096

#define MAX_EVENTS 8192

#define set_nonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

#define ERROR(msg) fprintf(stderr, "%s: %s\n", msg, strerror(errno))

// Enums for error handling
typedef enum {
    STATE_HANDSHAKING,
    STATE_CONNECTED
} CLIENT_STATE;

typedef enum {
    OK = 0,
    TLS_BAD_CONTEXT = -1,
    TLS_BAD_CERT = -2,
    TLS_BAD_KEY = -3,
    SSL_INIT_FAIL = -4,
    SSL_ACCEPT_FAIL = -5,
    SSL_SEND_FAIL = -6,
    SSL_RECV_FAIL = -7,
    BUFFER_OVERFLOW = -8,
	REMOVE_CLIENT_FAIL = -9
} SERVER_STATUS;


typedef struct {
    int socket;
    char name[16];

    CLIENT_STATE state;

    SSL *ssl;

    unsigned char in_buffer[INPUT_BUFFER_SIZE];
    size_t in_len;

    unsigned char out_buffer[INPUT_BUFFER_SIZE];
    size_t out_len;
    size_t out_sent; // bytes already sent from out_buffer

    size_t index; // index in clients arr (for O(1) delete)
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
    case SSL_INIT_FAIL:
        ERROR("SSL initialization failed");
		break;
    case SSL_ACCEPT_FAIL:
        ERROR("TLS handshake failed");
		break;
    case SSL_SEND_FAIL:
        ERROR("SSL send failed");
        break;
    case SSL_RECV_FAIL:
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