#pragma once

// =====================
// UNIVERSAL DEFINITIONS
// =====================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_MESSAGE_SIZE 4096
#define INPUT_BUFFER_SIZE 4096

#ifdef _WIN32
   #define WIN32_LEAN_AND_MEAN
   #define _CRT_SECURE_NO_WARNINGS
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <windows.h>
   #pragma comment(lib, "Ws2_32.lib")
   // Socket macros
   typedef SOCKET my_socket_t;
   #define my_close(s) closesocket(s)

   // Threading macros
   #define THREAD_TYPE HANDLE
   #define THREAD_CREATE(thr, func, param) thr = CreateThread(NULL, 0, func, param, 0, NULL)
   #define THREAD_JOIN(thr) WaitForSingleObject(thr, INFINITE)

#else // Linux / POSIX
   #include <unistd.h>
   #include <pthread.h>
   #include <sys/socket.h>
   #include <arpa/inet.h>
   #include <netdb.h>

   // Socket macros
   typedef int my_socket_t;
   #define my_close(s) close(s)

   // Threading macros
   #define THREAD_TYPE pthread_t
   #define THREAD_CREATE(thr, func, param) pthread_create(&thr, NULL, func, param)
   #define THREAD_JOIN(thr) pthread_join(thr, NULL)

#endif // _WIN32 Universal

// ========================
// SERVER SIDE DEFINITIONS
// ========================
#ifdef _WIN32
   #define MUTEX_TYPE CRITICAL_SECTION
   #define MUTEX_INIT(m) InitializeCriticalSection(&m)
   #define MUTEX_LOCK(m) EnterCriticalSection(&m)
   #define MUTEX_UNLOCK(m) LeaveCriticalSection(&m)
   #define MUTEX_DESTROY(m) DeleteCriticalSection(&m)

   // Winsock macros (Windows only)
   #define INIT_WINSOCK() do { \
      WSADATA wsaData; \
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { \
         printf("WSAStartup failed.\n"); \
         return 1; \
      } \
   } while (0)

   #define WSA_CLEANUP() WSACleanup()

   #define ClientHandler(lpParam) DWORD __stdcall ClientHandler(void *lpParam)
   #define R_NULL 0 // For returning 0 in Windows and void* in Linux

	// Macros for event loop (polling)
   #define poll WSAPoll
   #define SET_NONBLOCK(s) { u_long mode = 1; ioctlsocket(s, FIONBIO, &mode); }

#else // Linux / POSIX
   #define MUTEX_TYPE pthread_mutex_t
   #define MUTEX_INIT(m) pthread_mutex_init(&m, NULL)
   #define MUTEX_LOCK(m) pthread_mutex_lock(&m)
   #define MUTEX_UNLOCK(m) pthread_mutex_unlock(&m)
   #define MUTEX_DESTROY(m) pthread_mutex_destroy(&m)

   // Winsock macros (Windows only)
   #define INIT_WINSOCK() ((void)0)
   #define WSA_CLEANUP() ((void)0)

   #define ClientHandler(lpParam) void *ClientHandler(void *lpParam)
   #define R_NULL NULL

   // Macros for event loop (polling)
   #include <poll.h>
   #include <fcntl.h>
   #define SET_NONBLOCK(s) fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK)

#endif // _WIN32 Server

#define DEFAULT_PORT "4444"
#define SERVER_BUFFER_SIZE 4096
#define MAX_CLIENTS 50
#define MAX_NAME_LEN 16
#define CODE_LENGTH 6

typedef struct {
   my_socket_t socket;
   char name[MAX_NAME_LEN];
} Client;


// =======================
// CLIENT SIDE DEFINITIONS
// =======================
#ifdef _WIN32
   #pragma comment(lib, "Mswsock.lib")
   #pragma comment(lib, "AdvApi32.lib")

   #define my_shutdown_send(s) shutdown(s, SD_SEND)
   #define my_get_last_error() WSAGetLastError()
   #define MY_SEND_ERROR SOCKET_ERROR

   #define THREAD_SLEEP(ms) Sleep(ms)

   #define ClientSendMessage(client) DWORD __stdcall ClientSendMessage(Client *client)
   #define ClientRecieveMessage(lpParam) DWORD __stdcall ClientRecieveMessage(LPVOID lpParam)

#else // Linux / POSIX
   #define my_shutdown_send(s) shutdown(s, SHUT_WR)
   #define my_get_last_error() errno
   #define MY_SEND_ERROR -1

   #define THREAD_SLEEP(ms) usleep((ms)*1000)

   // VT colors needed in Linux
   #define EnableVTMode() ((void)0)

   #define ClientSendMessage(client) void *ClientSendMessage(Client *client)
   #define ClientRecieveMessage(lpParam) void __stdcall *ClientRecieveMessage(void lpParam)
#endif // _WIN32 Client

#define THREAD_COUNT 2
#define SEND_THREAD 0
#define RECV_THREAD 1

#define IV_LEN 12
#define TAG_LEN 16

#define IP_LENGTH 15

typedef enum {
   SUCCESS = 0,
	TLS_BAD_CONTEXT = -1,
	TLS_BAD_CERT = -2,
	TLS_BAD_KEY = -3,
	SSL_ACCEPT_FAIL = -4,
	SSL_SEND_FAIL = -5,
	SSL_RECV_FAIL = -6,
	BUFFER_OVERFLOW = -7,
} state;

inline void print_error(state err) {
   switch (err) {
      case SUCCESS: printf("Success\n"); break;
      case TLS_BAD_CONTEXT: printf("TLS context initialization failed\n"); break;
      case TLS_BAD_CERT: printf("TLS certificate loading failed\n"); break;
      case TLS_BAD_KEY: printf("TLS private key loading failed\n"); break;
      case SSL_ACCEPT_FAIL: printf("SSL handshake failed\n"); break;
      case SSL_SEND_FAIL: printf("SSL send failed\n"); break;
      case SSL_RECV_FAIL: printf("SSL receive failed\n"); break;
      case BUFFER_OVERFLOW: printf("Buffer overflow detected\n"); break;
      default: printf("Unknown error\n");
   }
}