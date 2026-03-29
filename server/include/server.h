#pragma once

#include "darray.h"
#include "queue.h"

#include <stdint.h>
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

#define MAX_EVENTS 8192

#define ERROR(msg) fprintf(stderr, "%s: %s\n", msg, strerror(errno))

#define set_nonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)
#define mark_client_for_close(c) ((c)->flags.closing = TRUE)

#define likely(x)   __builtin_expect(!!(x), TRUE)
#define unlikely(x) __builtin_expect(!!(x), FALSE)

// Enums for error handling
typedef enum {
    HANDSHAKING,
    CONNECTED
} CLIENT_STATE;

typedef enum {
    OK                    =  0,
    TLS_BAD_CONTEXT       = -1,
    TLS_BAD_CERT          = -2,
    TLS_BAD_KEY           = -3,
    CLIENT_INIT_FAIL      = -4,
    CLIENT_HANDSHAKE_FAIL = -5,
    SEND_FAIL             = -6,
    RECV_FAIL             = -7,
    BUFFER_OVERFLOW       = -8,
    REMOVE_CLIENT_FAIL    = -9
} SERVER_STATUS;

typedef enum {
    INIT_OK               =  0,
    GETADDRINFO_FAIL      = -1,
    SOCKET_CREATE_FAIL    = -2,
    SETSOCKOPT_FAIL       = -3,
    BIND_FAIL             = -4,
    LISTEN_FAIL           = -5
} INIT_STATUS;

typedef enum {
    INIT_SERVER_FAIL = -1,
	INIT_TLS_FAIL    = -2,
	INIT_WORKER_FAIL = -3
} SERVER_MAIN_STATUS;


typedef struct {
    _Bool        state;
    _Bool        closing;
} ClientFlags;

typedef struct {
    uint8_t *in_buffer;
    uint8_t *out_buffer;

    size_t       index;      // index in the clients array
    size_t       in_len;
    size_t       out_len;
    size_t       out_sent;   // bytes already sent from out_buffer

    SSL         *ssl;

    int          socket;
    ClientFlags  flags;
} ClientTLS;

define_ptr_array(ClientTLS);

typedef struct {
    pthread_t        thread;
    int             *client_fd_queue;
    Array_ClientTLS  clients;
    int              epoll_fd;
	int 			 event_fd;  // For waking up the worker when new clients are added
    char           **msg_queue;
} Worker;


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

// Func for broadcasting the message to other clients
SERVER_STATUS broadcast_message(
    Array_ClientTLS *clients, 
    ClientTLS       *c, 
    int              epoll_fd
);

// Handles incoming data from a client, 
// processing complete messages and returns it
char *handle_recv( 
    Array_ClientTLS *clients, 
    ClientTLS       *c, 
    int              epoll_fd
);

// Flushes the send buffer for a client, handling partial writes and SSL errors
SERVER_STATUS flush_send(ClientTLS *c);

// Queues a packet to be sent to the client, adding length prefix and buffering
static SERVER_STATUS queue_packet(
    ClientTLS           *c,
    const unsigned char *data,
    uint32_t             len
);

// Function to pass into a thread for running a worker
void *run_worker(void *arg);

// Function for balancing clients to worker threads
void send_fd_to_worker(Worker *w, int client_fd);


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
