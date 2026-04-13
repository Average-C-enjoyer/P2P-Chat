#pragma once

#include "darray.h"

#include <openssl/ssl.h>
#include <errno.h>

#if __STDC_VERSION__ < 201112L || __STDC_NO_ATOMICS__ == 1
    #error "Atomics not supported. Cannot compile"
#endif

#define TRUE 1
#define FALSE 0

#define DEFAULT_PORT "4433"
#define INPUT_BUFFER_SIZE 4096
#define OUTPUT_BUFFER_SIZE 4096
#define LEN_PREFIX_SIZE 4

#define MAX_EVENTS 8192

// Macro for error logging with strerror
#define ERROR(msg) fprintf(stderr, "%s: %s\n", msg, strerror(errno))

// Macro for debug logging, can be enabled by defining DEBUG
#ifdef DEBUG_IMPL
    #define DEBUG(...)           \
        do {                     \
            printf(__VA_ARGS__); \
            printf("\n");        \
        } while (0)
#else
    #define DEBUG(...)
#endif

// Macro to set a file descriptor to non-blocking mode
#define set_nonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

// Macro to mark a client for closing by setting the closing flag
#define mark_client_for_close(c) ((c)->flags.closing = TRUE)

#define likely(x)   __builtin_expect(!!(x), TRUE)
#define unlikely(x) __builtin_expect(!!(x), FALSE)

typedef struct Worker_s Worker;

// ===============================
// Global variables
// ===============================

// Workers array and count
extern Worker *workers;
extern _Atomic int workers_count;


// ================================
// Enums for error handling
// ================================

// Client state: whether it's still handshaking or fully connected
typedef enum {
    HANDSHAKING,
    CONNECTED
} CLIENT_STATE;

// Server status codes for various operations, 
// including TLS errors, client handling, and epoll control
typedef enum SERVER_STATUS_S {
    OK                    =  0,
    TLS_BAD_CONTEXT       = -1,
    TLS_BAD_CERT          = -2,
    TLS_BAD_KEY           = -3,
    CLIENT_INIT_FAIL      = -4,
    CLIENT_HANDSHAKE_FAIL = -5,
    SEND_FAIL             = -6,
    RECV_FAIL             = -7,
    BUFFER_OVERFLOW       = -8,
    REMOVE_CLIENT_FAIL    = -9,
    EPOLL_CTL_FAIL        = -10
} SERVER_STATUS;

// Initialization status codes for server setup, 
// including socket creation and binding errors
typedef enum INIT_STATUS_S {
    GETADDRINFO_FAIL      = -1,
    SOCKET_CREATE_FAIL    = -2,
    SETSOCKOPT_FAIL       = -3,
    BIND_FAIL             = -4,
    LISTEN_FAIL           = -5
} INIT_STATUS;

// Main server status codes for overall server startup
typedef enum SERVER_MAIN_STATUS_S {
    INIT_SERVER_FAIL      = -1,
    INIT_TLS_FAIL         = -2
} SERVER_MAIN_STATUS;


// ================================
// Structs and types
// ================================

// Flags for client state
// Field "closing" needs for not closing connection instantly on error
// but marking it for closing and removing after processing all events
typedef struct ClientFlags_s {
    _Bool        state;       // HANDSHAKING or CONNECTED
    _Bool        closing;
} ClientFlags;

// Struct representing a connected client, including SSL state, buffers, and flags
typedef struct ClientTLS_s {
    SSL         *ssl;

	uint8_t     *in_buffer;
	size_t       in_len;

    uint8_t     *out_buffer;
    size_t       out_len;
    size_t       out_sent;    // Bytes already sent from out_buffer

    size_t       index;       // index in the clients array
    int          socket;
    uint32_t     id;          // Unique client ID
    ClientFlags  flags;
} ClientTLS;

// Define a dynamic array type for ClientTLS pointers
define_ptr_array(ClientTLS);

// Message struct for inter-worker communication 
// with reference counting
typedef struct Message_s {
    uint8_t     *data;
    _Atomic int  refcount;
    uint32_t     len;
    uint32_t     sender_id;   // For not echoing msg to sender
} Message;

// Worker struct representing a worker thread
// Each worker has its own epoll instance and client list
struct Worker_s {
    pthread_t        thread;

    int             *client_fd_queue;
    Array_ClientTLS  clients;

    int              epoll_fd;
    int 			 event_fd;  // For main notifications

    // Okay, this is complex...
    // Its an array of queues of messages.
	// Each pair of worker threads has its own queue for sending messages to each other.
	// like N*N queues for N workers. Each queue is SPSC
    Message       **msg_queue;
};


// ================================
// Function declarations
// ================================

// Initializes the TLS server context, loading certificates and keys
SERVER_STATUS init_tls();

// Initialize socets, TCP and stuff
INIT_STATUS init_server(int *server_socket);

// Adds a new client, creating SSL object and setting up non-blocking socket
SERVER_STATUS add_client(Worker *w, int sock);

// Removes a client, cleaning up resources
// Returns OK on success, REMOVE_CLIENT_FAIL on failure
SERVER_STATUS remove_client(
    Array_ClientTLS *clients,
    ClientTLS       *c,
    int              epoll_fd
);

// Func for TLS handshake
// Returns OK if handshake is complete or still in progress, 
// CLIENT_HANDSHAKE_FAIL on failure
SERVER_STATUS handle_handshake(int epoll_fd, ClientTLS *c);

// Func for broadcasting the message to all other clients
void broadcast_message(
    Array_ClientTLS *clients, 
    Message *msg, 
    int epoll_fd
);

// Handles incoming data from a client, 
// processing complete messages and returns it
SERVER_STATUS handle_recv(
    Array_ClientTLS *clients,
    ClientTLS       *c,
    int              epoll_fd
);

// Flushes the send buffer for a client, handling partial writes and SSL errors
SERVER_STATUS flush_send(ClientTLS *c, Worker *w);


// Function to pass into a thread for running a worker
void *run_worker(void *arg);


static inline void init_worker(Worker *worker)
{
    worker->client_fd_queue = NULL;
    da_init(&worker->clients);
    worker->epoll_fd = -1;
}


static inline void run_worker_thread(Worker *worker)
{
    if (pthread_create(&worker->thread, NULL, run_worker, worker) != 0) {
        ERROR("Failed to create worker thread");
    }
}


// Main fanc to start server
short server_run();