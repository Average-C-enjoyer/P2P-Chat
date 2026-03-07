#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "TLS.h"

#define INPUT_BUFFER_SIZE 4096

// Error codes for TLS operations
typedef enum {
    TLS_OK = 0,
    TLS_BAD_CONTEXT = -1,
    TLS_BAD_CERT = -2,
    TLS_BAD_KEY = -3,
    SSL_ACCEPT_FAIL = -4,
    SEND_FAIL = -5,
    RECV_FAIL = -6,
    BUFFER_OVERFLOW = -7,
} TLS_STATE;

static inline void tls_print_error(TLS_STATE err) {
    switch (err) {
    case TLS_BAD_CONTEXT:
        fprintf(stderr, "TLS context initialization failed\n");
        break;
    case TLS_BAD_CERT:
        fprintf(stderr, "Failed to load TLS certificate\n");
        break;
    case TLS_BAD_KEY:
        fprintf(stderr, "Failed to load TLS private key\n");
        break;
    case SSL_ACCEPT_FAIL:
        fprintf(stderr, "TLS handshake failed\n");
        break;
    case SEND_FAIL:
        fprintf(stderr, "Failed to send data over TLS\n");
        break;
    case RECV_FAIL:
        fprintf(stderr, "Failed to receive data over TLS\n");
        break;
    case BUFFER_OVERFLOW:
        fprintf(stderr, "Message size exceeds maximum allowed\n");
        break;
    default:
        fprintf(stderr, "Unknown error\n");
    }
}

// ==================================================
// UNIVERSAL DEFINITIONS (work on Linux and Windows)
// ==================================================
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Initialize Winsock
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

// Socket macros
typedef SOCKET SOCKET_T;
#define CLOSE(s) closesocket(s)
#define SHUTDOWN_SEND(s) shutdown(s, SD_SEND)
#define GET_LAST_ERROR() WSAGetLastError()
#define SEND_ERROR SOCKET_ERROR

#define INIT_WINSOCK() do {                               \
        WSADATA wsaData;                                      \
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {  \
            printf("WSAStartup failed.\n");                   \
            return 1;                                         \
        }                                                     \
    } while (0)

#define WSA_CLEANUP() WSACleanup()

// Threading macros
typedef HANDLE THREAD;
#define THREAD_CREATE(thr, func, param) thr = CreateThread(NULL, 0, func, param, 0, NULL)
#define THREAD_JOIN(thr) WaitForSingleObject(thr, INFINITE)
#define THREAD_SLEEP(ms) Sleep(ms)

#define ClientSendMessage(client) DWORD WINAPI ClientSendMessage(ClientTLS *client)
#define ClientRecieveMessage(lpParam) DWORD WINAPI ClientRecieveMessage(LPVOID lpParam)

#else // Linux / POSIX
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// Socket macros
typedef int SOCKET_T;
#define CLOSE(s) close(s)
#define SHUTDOWN_SEND(s) shutdown(s, SHUT_WR)
#define GET_LAST_ERROR() errno
#define SEND_ERROR -1

#define INIT_WINSOCK() ((void)0)
#define WSA_CLEANUP() ((void)0)

// Threading macros
typedef pthread_t THREAD;
#define THREAD_CREATE(thr, func, param) \
        pthread_create(&(thr), NULL, (void *(*)(void *))(func), (void *)(param))
#define THREAD_JOIN(thr) pthread_join(thr, NULL)
#define THREAD_SLEEP(ms) usleep((ms)*1000)

// No special terminal handling needed on POSIX
#define EnableVTMode() ((void)0)

#define ClientSendMessage(client) \
        void* ClientSendMessage(ClientTLS *client)
#define ClientRecieveMessage(lpParam) \
        void* ClientRecieveMessage(void *lpParam)

#endif // _WIN32


// Client structure for TLS connections
typedef struct {
    SOCKET_T socket;
    char name[16];

    SSL *ssl;
    SSL_CTX *ctx;

    unsigned char in_buffer[INPUT_BUFFER_SIZE];
    size_t in_len;
} ClientTLS;

#define CLEANUP_CLIENT(client) do {  \
	SSL_free((client)->ssl);         \
	SSL_CTX_free((client)->ctx);     \
	CLOSE((client)->socket);         \
	WSA_CLEANUP();                   \
} while(0)
