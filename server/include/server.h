#pragma once

#include "d_array.h"

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

#define ERROR(...) error_impl(__VA_ARGS__, strerror(errno))

static inline void error_impl(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, ": %s\n", strerror(errno));
}

// Macro for debug logging, can be enabled by defining DEBUG
#ifdef DEBUG_IMPL
#define DEBUG(fmt, ...) do { fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__); } while(0)
#else
#define DEBUG(fmt, ...) do { } while(0)
#endif

// Macro to set a file descriptor to non-blocking mode
#define set_nonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

// Macro to mark a client for closing by setting the closing flag
#define mark_client_for_close(c) ((c)->flags.closing = TRUE)

#define likely(x)   __builtin_expect(!!(x), TRUE)
#define unlikely(x) __builtin_expect(!!(x), FALSE)

// Forward declarations
typedef struct ClientFlags_s ClientFlags;
typedef struct Worker_s Worker;
typedef struct ClientTLS_s ClientTLS;
typedef struct Message_s Message;

// ===============================
// Global variables
// ===============================

// Workers array and count
extern Worker *workers;
extern _Atomic(int)workers_count;

/*
Okay, this is complex...
Its a 2d array of queues of pointers to Message struct.
Each pair of worker threads has its own queue for sending messages to each other.
like N*N queues for N workers. Each queue is SPSC

Indexing like:
msg_queues[i][j]

Where
i = sender (producer)
j = receiver (consumer)

In memory:
     to →  0     1     2     3
from ↓  +-----+-----+-----+-----+
     0  |  -  | Q01 | Q02 | Q03 |
     1  | Q10 |  -  | Q12 | Q13 |
     2  | Q20 | Q21 |  -  | Q23 |
     3  | Q30 | Q31 | Q32 |  -  |
*/
extern Message ****msg_queues;


// ================================
// Enums for error handling
// ================================

// Client state: whether it's still handshaking or fully connected
typedef enum CLIENT_STATE_E {
    HANDSHAKING,
    CONNECTED
} CLIENT_STATE;

// Server status codes for various operations, 
// including TLS errors, client handling, and epoll control
typedef enum SERVER_STATUS_E {
    OK = 0,
    TLS_BAD_CONTEXT = -1,
    TLS_BAD_CERT = -2,
    TLS_BAD_KEY = -3,
    CLIENT_INIT_FAIL = -4,
    CLIENT_HANDSHAKE_FAIL = -5,
    SEND_FAIL = -6,
    RECV_FAIL = -7,
    BUFFER_OVERFLOW = -8,
    REMOVE_CLIENT_FAIL = -9,
    EPOLL_CTL_FAIL = -10
} SERVER_STATUS;

// Initialization status codes for server setup, 
// including socket creation and binding errors
typedef enum INIT_STATUS_E {
    GETADDRINFO_FAIL = -1,
    SOCKET_CREATE_FAIL = -2,
    SETSOCKOPT_FAIL = -3,
    BIND_FAIL = -4,
    LISTEN_FAIL = -5
} INIT_STATUS;

// Main server status codes for overall server startup
typedef enum SERVER_MAIN_STATUS_E {
    INIT_SERVER_FAIL = -1,
    INIT_TLS_FAIL = -2
} SERVER_MAIN_STATUS;


// ================================
// Structs and types
// ================================

// Flags for client state
// Field "closing" needs for not closing connection instantly on error
// but marking it for closing and removing after processing all events
struct ClientFlags_s {
    _Bool        state;       // HANDSHAKING or CONNECTED
    _Bool        closing;
};

// Struct representing a connected client, including SSL state, buffers, and flags
struct ClientTLS_s {
    SSL *ssl;

    uint8_t *in_buffer;
    size_t       in_len;

    uint8_t *out_buffer;
    size_t       out_len;
    size_t       out_sent;    // Bytes already sent from out_buffer

    size_t       index;       // index in the clients array
    int          socket;
    uint32_t     id;          // Unique client ID
    ClientFlags  flags;
};

// Define a dynamic array type for ClientTLS pointers
define_ptr_array(ClientTLS);

// Message struct for inter-worker communication 
// with reference counting
struct Message_s {
    uint8_t *data;
    _Atomic int  refcount;
    uint32_t     len;
    uint32_t     sender_id;   // For not echoing msg to sender
};

// Worker struct representing a worker thread
// Each worker has its own epoll instance and client list
struct Worker_s {
    pthread_t        thread;

    int *client_fd_queue;
    Array_ClientTLS  clients;

    int              epoll_fd;
    int 			 event_fd;  // For main notifications
    int              id;
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
    ClientTLS *c,
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
    ClientTLS *c,
    int              epoll_fd,
    int              current_worker_id
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
SERVER_MAIN_STATUS server_run();