#pragma once

#include <stdlib.h>
#include <stdint.h>

#define INPUT_BUFFER_SIZE 4096

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
typedef SOCKET socket_t;

#define CLOSE(sock) closesocket(sock)
#define SHUTDOWN_SEND(sock) shutdown(sock, SD_SEND)
#define GET_LAST_ERROR() WSAGetLastError()
#define SEND_ERROR SOCKET_ERROR

#define INIT_WINSOCK() ({                          \
    int _exit_code = 0;                            \
    WSADATA wsaData;                               \
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) \
    {                                              \
            _exit_code = 1;                        \
    }                                              \
    _exit_code;                                    \
})

#define WSA_CLEANUP() WSACleanup()

// Threading macros
typedef HANDLE thread_t;

#define THREAD_CREATE(thr, func, param) thr = CreateThread(NULL, 0, func, param, 0, NULL)

#define THREAD_JOIN(thr)   WaitForSingleObject(thr, INFINITE)
#define THREAD_DETACH(thr) ((void)0)
#define THREAD_SLEEP(ms)   Sleep(ms)

#define client_do_read_decl(handle) \
    DWORD WINAPI client_do_read(ClientTLS handle)

#else // Linux / POSIX
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// Socket macros
typedef int socket_t;

#define CLOSE(sock) close(sock)
#define SHUTDOWN_SEND(sock) shutdown(sock, SHUT_WR)
#define GET_LAST_ERROR() errno
#define SEND_ERROR -1

#define INIT_WINSOCK() 0
#define WSA_CLEANUP()

// Threading macros
typedef pthread_t thread_t;

#define THREAD_CREATE(thr, func, param) \
        pthread_create(&(thr), NULL, (void *(*)(void *))(func), (void *)(param))

#define THREAD_JOIN(thr)   pthread_join(thr, NULL)
#define THREAD_DETACH(thr) pthread_detach(thr)
#define THREAD_SLEEP(ms)   usleep((ms)*1000)

// No special terminal handling needed on POSIX
#define EnableVTMode()

#define client_do_read_decl(handle) \
        void* client_do_read(ClientTLS *handle)

#endif // _WIN32
