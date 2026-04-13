#include <unistd.h>
#include <netdb.h>

#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>

// To enable DEBUG() logs, define DEBUG_IMPL
#define DEBUG_IMPL
#include "server.h"
#include "workers.h"

SSL_CTX *server_ctx;

// Definition of extern symbols declared in workers.h
Worker *workers = NULL;
_Atomic int workers_count = 0;

SERVER_STATUS init_tls()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    server_ctx = SSL_CTX_new(TLS_server_method());
    if (unlikely(!server_ctx))
    {
        ERROR("TLS context initialization failed");
        return TLS_BAD_CONTEXT;
    }

    if (unlikely(SSL_CTX_use_certificate_file(
        server_ctx,
        "server.crt",
        SSL_FILETYPE_PEM) <= 0))
    {
        ERROR("Failed to load TLS certificate");
        return TLS_BAD_CERT;
    }

    if (unlikely(SSL_CTX_use_PrivateKey_file(
        server_ctx,
        "server.key",
        SSL_FILETYPE_PEM) <= 0))
    {
        ERROR("Failed to load TLS private key");
        return TLS_BAD_KEY;
    }

    return OK;
}


INIT_STATUS init_server(int *server_socket)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, DEFAULT_PORT, &hints, &result) != 0)
    {
        ERROR("getaddrinfo failed");
        return GETADDRINFO_FAIL;
    }

    *server_socket = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (unlikely(*server_socket < 0))
    {
        ERROR("Failed to create socket");
        return SOCKET_CREATE_FAIL;
    }

    int opt = 1;
    if (unlikely(setsockopt(
        *server_socket,
        SOL_SOCKET, SO_REUSEADDR,
        &opt,
        sizeof(opt)) < 0))
    {
        ERROR("setsockopt(SO_REUSEADDR) failed");
        close(*server_socket);
        return SETSOCKOPT_FAIL;
    }

    if (unlikely(bind(*server_socket, result->ai_addr, result->ai_addrlen) < 0))
    {
        ERROR("Failed to bind socket");
        close(*server_socket);
        return BIND_FAIL;
    }

    if (unlikely(listen(*server_socket, SOMAXCONN) < 0))
    {
        ERROR("Failed to listen on socket");
        close(*server_socket);
        return LISTEN_FAIL;
    }

    set_nonblocking(*server_socket);
    freeaddrinfo(result);

    return OK;
}


SERVER_STATUS add_client(Worker *w, int sock)
{
    char err[64];

    ClientTLS *c = malloc(sizeof(ClientTLS));
    if (unlikely(!c))
    {
        strcpy(err, "Client allocation failed");
        goto cleanup;
    }

    memset(c, 0, sizeof(ClientTLS));

    c->id = (uint32_t)sock; // Simple unique ID based on socket fd

    c->out_buffer = malloc(OUTPUT_BUFFER_SIZE);
    if (unlikely(!c->out_buffer))
    {
        strcpy(err, "Output buffer allocation failed");
        goto cleanup;
    }

    c->in_buffer = malloc(OUTPUT_BUFFER_SIZE);
    if (unlikely(!c->in_buffer))
    {
        strcpy(err, "Output buffer allocation failed");
        goto cleanup;
    }

    c->socket = sock;
    c->flags.state = HANDSHAKING;

    c->ssl = SSL_new(server_ctx);
    if (unlikely(!c->ssl))
    {
        strcpy(err, "SSL initialization failed");
        goto cleanup;
    }

    if (unlikely(SSL_set_fd(c->ssl, (int)sock) <= 0))
    {
        strcpy(err, "SSL initialization failed");
        goto cleanup;
    }

    set_nonblocking(sock);

    da_append(&w->clients, c);
    if (unlikely(da_get_last_err(&w->clients) != DA_OK))
    {
        da_print_error(w->clients.err);
        strcpy(err, "SSL initialization failed");
        goto cleanup;
    }

    c->index = w->clients.size - 1;

    // Add client to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = c;

    if (epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, sock, &ev) < 0) {
        strcpy(err, "epoll_ctl ADD failed");
        goto cleanup;
    }

    printf("Added client");
    return OK;

cleanup:
    if (c->out_buffer) free(c->out_buffer);
    if (c->ssl) SSL_free(c->ssl);
    if (c) free(c);
    ERROR(err);
    return CLIENT_INIT_FAIL;
}


SERVER_STATUS remove_client(Array_ClientTLS *clients, ClientTLS *c, int epoll_fd)
{
    if (!c) return OK;

    // Remove from epoll to avoid receiving further events for this socket
    if (epoll_fd >= 0)
    {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c->socket, NULL) < 0)
        {
            ERROR("epoll_ctl(DEL) failed");
        }
    }

    if (c->in_buffer)  free(c->in_buffer);
    if (c->out_buffer) free(c->out_buffer);

    SSL_free(c->ssl);
    close(c->socket);

    da_swap_remove_ptr(clients, c->index);

    if (unlikely(da_get_last_err(clients) != DA_OK))
    {
        da_print_error(clients->err);
        ERROR("Failed to remove client");
        return REMOVE_CLIENT_FAIL;
    }

    free(c);
    return OK;
}


SERVER_STATUS handle_handshake(int epoll_fd, ClientTLS *c)
{
    int r = SSL_accept(c->ssl);

    if (r == 1)
    {
        c->flags.state = CONNECTED;

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = c;

        if (epoll_ctl(epoll_fd,
            EPOLL_CTL_MOD,
            c->socket,
            &ev) < 0)
        {
            ERROR("epoll_ctl MOD failed");
            return EPOLL_CTL_FAIL;
        }

		DEBUG("TLS handshake completed");
        return OK;
    }

    int err = SSL_get_error(c->ssl, r);

    if (err == SSL_ERROR_WANT_READ)
    {
		DEBUG("TLS handshake in progress, waiting for more data");
        return OK;
    }

    if (err == SSL_ERROR_WANT_WRITE)
    {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = c;

        if (epoll_ctl(epoll_fd,
            EPOLL_CTL_MOD,
            c->socket,
            &ev) < 0)
        {
            ERROR("epoll_ctl MOD failed after WANT_WRITE");
            return EPOLL_CTL_FAIL;
        }

		DEBUG("TLS handshake in progress, waiting for socket to be writable");
        return OK;
    }

    ERROR("TLS handshake failed");
    return CLIENT_HANDSHAKE_FAIL;
}


// =====================================
// Send funcs
// =====================================
SERVER_STATUS flush_send(ClientTLS *c, Worker *w)
{
    while (c->out_sent < c->out_len)
    {
        int r = SSL_write(
            c->ssl,
            c->out_buffer + c->out_sent,
            (int)(c->out_len - c->out_sent)
        );

        if (r <= 0)
        {
            int err = SSL_get_error(c->ssl, r);

            if (err == SSL_ERROR_WANT_WRITE ||
                err == SSL_ERROR_WANT_READ)
                return OK;

            ERROR("SSL send failed");
            return SEND_FAIL;
        }

        c->out_sent += r;
    }

    c->out_len = 0;
    c->out_sent = 0;

    return OK;
}


// ============================================
// Recieve funcs
// ============================================

// Queues a packet to be sent to the client, adding length prefix and buffering
static SERVER_STATUS queue_packet(
    uint8_t  *out_buffer, 
    size_t   *out_len, 
    uint8_t  *payload, 
    uint32_t  len)
{
    if (unlikely(len > INPUT_BUFFER_SIZE))
    {
        ERROR("Invalid argument: len greater than buffer size");
        return BUFFER_OVERFLOW;
    }

    if (unlikely(len > OUTPUT_BUFFER_SIZE - *out_len - LEN_PREFIX_SIZE))
    {
        ERROR("Buffer overflow: not enough space to queue packet");
        return BUFFER_OVERFLOW;
    }

    uint32_t net_len = htonl(len);

    // Data is buffered as [4-byte length][payload]
    memcpy(out_buffer + *out_len, &net_len, LEN_PREFIX_SIZE);
    memcpy(out_buffer + *out_len + LEN_PREFIX_SIZE, payload, len);

    *out_len += LEN_PREFIX_SIZE + len;

    return OK;
}


void broadcast_message(Array_ClientTLS *clients, Message *msg, int epoll_fd)
{
    for (size_t i = 0; i < clients->size; i++)
    {
        ClientTLS *dst = clients->data[i];
        if (dst->id == msg->sender_id) continue;

        if (unlikely(queue_packet(dst->out_buffer, &dst->out_len, msg->data, msg->len) != OK))
        {
            mark_client_for_close(dst);
            continue;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = dst;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, dst->socket, &ev) < 0)
        {
            ERROR("epoll_ctl MOD failed when broadcasting message");
            mark_client_for_close(dst);
            continue;
		}
    }
}


typedef enum PARSE_STATUS_S {
    MSG_BAD_SIZE            = -1,
	MSG_TOO_LARGE           = -2,
    MSG_BAD_LENGTH          = -3,
    MSG_BUFFER_ALLOC_FAILED = -4
} PARSE_STATUS;

// Extracting message from client's tcp buffer
static inline PARSE_STATUS extract_message(
    uint8_t *in_buffer, 
    size_t  *in_len, 
    Message *msg)
{
    // Need at least 4 bytes for length
    if (*in_len < LEN_PREFIX_SIZE)
    {
        return MSG_BAD_SIZE;
    }

    uint32_t msg_len;
    memcpy(&msg_len, in_buffer, LEN_PREFIX_SIZE);
    msg_len = ntohl(msg_len);

    // Sanity check
    if (msg_len > INPUT_BUFFER_SIZE)
    {
        ERROR("Message too large");
        return MSG_TOO_LARGE;
    }

    // Check if full message arrived
    if (*in_len < LEN_PREFIX_SIZE + msg_len)
    {
        return MSG_BAD_LENGTH;
    }

    Message *msg = malloc(sizeof(Message) + msg_len);
    if (unlikely(!msg->data))
    {
        ERROR("Message allocation failed");
        return MSG_BUFFER_ALLOC_FAILED;
    }

	// Data is stored right after the Message struct for cache efficiency and less malloc()
    msg->data = (uint8_t *)(msg + 1);

	// Copy data from buffer
    memcpy(msg->data, in_buffer + LEN_PREFIX_SIZE, msg_len);
    msg->len = msg_len;

    // Delete handled msg from buffer
    size_t total = LEN_PREFIX_SIZE + msg_len;
    size_t remaining = *in_len - total;

    if (remaining > 0)
    {
        memmove(
            in_buffer,
            in_buffer + total,
            remaining
        );
    }

    *in_len = remaining;

    return OK;
}


SERVER_STATUS handle_recv(
    Array_ClientTLS *clients,
    ClientTLS       *c,
    int              epoll_fd)
{
    _Bool need_epoll_out = FALSE;

    while (1)
    {
        size_t avail = INPUT_BUFFER_SIZE - c->in_len;
        if (unlikely(avail == 0))
        {
            // no space to read more
            ERROR("Buffer overflow: message too large");
            break;
        }

        int r = SSL_read(
            c->ssl,
            c->in_buffer + c->in_len,
            (int)avail
        );

        if (r > 0)
        {
            c->in_len += r;
            // try to read more until WANT_READ or buffer full
            if (c->in_len == INPUT_BUFFER_SIZE) {
                break;
            }
            else {
                continue;
            }
        }

        // r <= 0
        int err = SSL_get_error(c->ssl, r);

        if (err == SSL_ERROR_WANT_READ)
        {
            // No more data available now
            break;
        }

        if (err == SSL_ERROR_WANT_WRITE)
        {
            need_epoll_out = TRUE;
            break;
        }

        ERROR("SSL receive failed");
        return RECV_FAIL;
    }

    if (need_epoll_out)
    {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = c;
        
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->socket, &ev) < 0)
        {
            ERROR("epoll_ctl MOD failed after WANT_WRITE in recv");
            return EPOLL_CTL_FAIL;
		}
    }
    
    Message *msg;

    int processed = 0;
    while (1)
    {
		if (++processed > 1000) {
            DEBUG("Processed 1000 messages, yielding to avoid starvation");
            break;
        }

        if (extract_message(c->in_buffer, &c->in_len, msg) != OK)
        {
            free(msg);
            break;
        }

        msg->sender_id = c->id;
        msg->refcount = workers_count;

        for (int wi = 0; wi < workers_count; wi++)
        {
            send_msg_to_worker(&workers[wi], msg);
        }
    }

	DEBUG("Received message\n");
	return OK;
}


short server_run()
{
    // Define amount of logical cores for creating same amount
    // of worker threads
    int logical_cores_amount = sysconf(_SC_NPROCESSORS_ONLN);
    if (logical_cores_amount <= 0) logical_cores_amount = 4;

    int server_socket;

    // Initialize server socket and TLS context
    if (init_server(&server_socket) < 0) {
        ERROR("Initializing server failed");
        return INIT_SERVER_FAIL;
    }

    if (init_tls() < 0) {
        ERROR("Initializing TLS connection failed");
        return INIT_TLS_FAIL;
    }


    // Create worker threads and their epoll instances
    workers = malloc(sizeof(Worker) * logical_cores_amount);
    int next_worker = 0;
    int workers_failed = 0;

    for (int i = 0; i < logical_cores_amount; i++)
    {
        init_worker(&workers[i]);

        workers[i].epoll_fd = epoll_create1(0);
        workers[i].event_fd = eventfd(0, EFD_NONBLOCK);

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = workers[i].event_fd;

        if (workers[i].epoll_fd < 0)
        {
            ERROR("Failed to create epoll instance for worker");
            workers_failed++;
            continue;
        }

        if (epoll_ctl(
            workers[i].epoll_fd,
            EPOLL_CTL_ADD,
            workers[i].event_fd,
            &ev) < 0)
        {
            ERROR("epoll_ctl ADD event_fd failed");
        }

        run_worker_thread(&workers[i]);
    }

    workers_count = logical_cores_amount - workers_failed;

    // Initialize epoll instance for accept thread (main)
    int epoll_fd = epoll_create1(0);

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_socket;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &ev);

    printf("Server is listening on port %s...\n", DEFAULT_PORT);

    // Accept loop
    while (1)
    {
        int ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < ready; i++) {
            if (events[i].data.fd == server_socket) {
                while (1)
                {
                    int client_fd = accept(server_socket, NULL, NULL);

                    // If no more clients to accept, break the loop
                    if (client_fd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // No more clients to accept
                        }
                        else {
                            ERROR("Accept failed");
                            break;
                        }
                    }

                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN | EPOLLET;

                    printf("New client connected!\n");

                    // Sending client to the worker
                    Worker *w = &workers[next_worker];
                    next_worker = (next_worker + 1) % logical_cores_amount;

                    send_fd_to_worker(w, client_fd);
                }
            }
        }
    }

    // Cleanup (unreachable, but good practice)
    if (workers) free(workers);
    if (server_ctx) SSL_CTX_free(server_ctx);
    if (server_socket >= 0) close(server_socket);
    if (epoll_fd >= 0) close(epoll_fd);

    return 0;
}