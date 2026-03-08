#include "server.h"

#define DEBUG

define_ptr_array(ClientTLS);

Array_ClientTLS clients;

static SSL_CTX *server_ctx = NULL;

// Initializes the TLS server context, loading certificates and keys
static SERVER_STATUS init_tls_server()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    server_ctx = SSL_CTX_new(TLS_server_method());
    if (unlikely(!server_ctx))
        return TLS_BAD_CONTEXT;

    if (unlikely(SSL_CTX_use_certificate_file(server_ctx,
        "server.crt", SSL_FILETYPE_PEM) <= 0))
        return TLS_BAD_CERT;

    if (unlikely(SSL_CTX_use_PrivateKey_file(server_ctx,
        "server.key", SSL_FILETYPE_PEM) <= 0))
        return TLS_BAD_KEY;

    return OK;
}


static SERVER_STATUS add_client(int sock)
{
    ClientTLS *c = malloc(sizeof(ClientTLS));
    if (unlikely(!c))
        return CLIENT_INIT_FAIL;
    memset(c, 0, sizeof(ClientTLS));

    c->socket = sock;
    c->flags.state = HANDSHAKING;

    c->ssl = SSL_new(server_ctx);
    if (unlikely(!c->ssl)) {
        free(c);
        return CLIENT_INIT_FAIL;
    }

    if (unlikely(SSL_set_fd(c->ssl, (int)sock) <= 0)) {
        free(c);
        return CLIENT_INIT_FAIL;
    }

    set_nonblocking(sock);

    da_append(&clients, c);
    if (unlikely(da_get_last_err(&clients) != DA_OK)) {
        free(c);
        da_print_error(clients.err);
        return CLIENT_INIT_FAIL;
    }
    c->index = clients.size - 1;
    return OK;
}


// Removes a client, cleaning up resources
static SERVER_STATUS remove_client(int epoll_fd, ClientTLS *c)
{
    if (!c) return OK;

    // Remove from epoll to avoid receiving further events for this socket
    if (epoll_fd >= 0) {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c->socket, NULL) < 0) {
            ERROR("epoll_ctl(DEL) failed");
        }
    }

    SSL_free(c->ssl);
    close(c->socket);

    da_swap_remove_ptr(&clients, c->index);

    if (unlikely(da_get_last_err(&clients) != DA_OK)) {
        da_print_error(clients.err);
        return REMOVE_CLIENT_FAIL;
    }
    free(c);

    return OK;
}


static SERVER_STATUS handle_handshake(int epoll_fd, ClientTLS *c)
{
    int r = SSL_accept(c->ssl);

    if (r == 1)
    {
        c->flags.state = CONNECTED;

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = c;

        epoll_ctl(epoll_fd,
            EPOLL_CTL_MOD,
            c->socket,
            &ev);

        return OK;
    }

    int err = SSL_get_error(c->ssl, r);

    if (err == SSL_ERROR_WANT_READ)
        return OK;

    if (err == SSL_ERROR_WANT_WRITE) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = c;

        epoll_ctl(epoll_fd,
            EPOLL_CTL_MOD,
            c->socket,
            &ev);

        return OK;
    }

    return CLIENT_HANDSHAKE_FAIL;
}

// Queues a packet to be sent to the client, adding length prefix and buffering
static SERVER_STATUS queue_packet(ClientTLS *c,
    const unsigned char *data,
    uint32_t len)
{
    if (unlikely(len > INPUT_BUFFER_SIZE))
        return BUFFER_OVERFLOW;

    if (unlikely(c->out_len + 4 + len > OUTPUT_BUFFER_SIZE))
        return BUFFER_OVERFLOW;

    uint32_t net_len = htonl(len);

    // Data is buffered as [4-byte length][payload]
    memcpy(c->out_buffer + c->out_len, &net_len, 4);
    memcpy(c->out_buffer + c->out_len + 4, data, len);

    c->out_len += 4 + len;

    return OK;
}


static SERVER_STATUS flush_send(ClientTLS *c)
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

            return SEND_FAIL;
        }

        c->out_sent += r;
    }

    c->out_len = 0;
    c->out_sent = 0;
    return OK;
}

// Handles incoming data from a client, 
// processing complete messages and broadcasting them
static int handle_recv(ClientTLS *c, int epoll_fd)
{
    int need_epoll_out = 0;

    while (1)
    {
        size_t avail = INPUT_BUFFER_SIZE - c->in_len;
        if (unlikely(avail == 0)) {
            // no space to read more
            return BUFFER_OVERFLOW;
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

        if (err == SSL_ERROR_WANT_READ) {
            // No more data available now
            break;
        }

        if (err == SSL_ERROR_WANT_WRITE) {
            need_epoll_out = 1;
            break;
        }

        return RECV_FAIL;
    }

    if (need_epoll_out) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = c;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->socket, &ev);
    }

    while (c->in_len >= 4)
    {
        uint32_t msg_len;
        memcpy(&msg_len, c->in_buffer, 4);
        msg_len = ntohl(msg_len);

        if (unlikely(msg_len > OUTPUT_BUFFER_SIZE))
            return BUFFER_OVERFLOW;

        if (c->in_len < 4 + msg_len)
            break;

        unsigned char *payload =
            c->in_buffer + 4;

        // Bradcast
        for (size_t i = 0; i < clients.size; i++)
        {
            ClientTLS *dst = clients.data[i];
            if (dst == c) continue;

            short err = queue_packet(dst, payload, msg_len);
            if (unlikely(err != OK)) {
				print_error_server(err);
				mark_client_for_close(dst);
                continue;
            }

            // Enable EPOLLOUT for the destination client
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.ptr = dst;
            epoll_ctl(epoll_fd,
                EPOLL_CTL_MOD,
                dst->socket,
                &ev);
        }

        memmove(
            c->in_buffer,
            c->in_buffer + 4 + msg_len,
            c->in_len - 4 - msg_len
        );

        c->in_len -= (4 + msg_len);
    }

    return OK;
}

// ============================
// MAIN
// ============================
int main()
{
    int serverSocket = -1;
    struct addrinfo hints, *result = NULL;

	short err = init_tls_server();
    if (err < 0) {
        ERROR("TLS initialization failed");
		print_error_server(err);
        return 1;
    }

    da_init(&clients);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, DEFAULT_PORT, &hints, &result) != 0) {
		ERROR("getaddrinfo failed");
        return 1;
    }

    serverSocket = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (unlikely(serverSocket < 0)) {
        ERROR("Failed to create socket");
        return 1;
    }

    int opt = 1;
    if (unlikely(setsockopt(serverSocket, 
                            SOL_SOCKET, SO_REUSEADDR, 
                            &opt, 
                            sizeof(opt)) < 0)) 
    {
        ERROR("setsockopt(SO_REUSEADDR) failed");
        close(serverSocket);
        return 1;
    }

    if (unlikely(bind(serverSocket, result->ai_addr, result->ai_addrlen) < 0)) {
        ERROR("Failed to bind socket");
        close(serverSocket);
		return 1;
    }

    if (unlikely(listen(serverSocket, SOMAXCONN) < 0)) {
		ERROR("Failed to listen on socket");
		close(serverSocket);
        return 1;
    }

    set_nonblocking(serverSocket);
    freeaddrinfo(result);

    int epoll_fd = epoll_create1(0);

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    // server socket already created, bound, listening, non-blocking

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = serverSocket;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverSocket, &ev);

	printf("Server is listening on port %s...\n", DEFAULT_PORT);
    while (1)
    {
        int ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < ready; i++)
        {
			// If the event is on the server socket -> new client connection
            if (events[i].data.fd == serverSocket)
            {
				// Accept loop for multiple connections in edge-triggered mode
                while (1)
                {
                    int client_fd =
                        accept(serverSocket, NULL, NULL);

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

                    if (unlikely(add_client(client_fd) != OK)) {
                        close(client_fd);
                        ERROR("Failed to add client");
                        continue;
                    }

					ClientTLS *c = clients.data[clients.size - 1];

                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN | EPOLLET;
                    client_ev.data.ptr = c;

                    epoll_ctl(epoll_fd,
                        EPOLL_CTL_ADD,
                        client_fd,
                        &client_ev);

#ifdef DEBUG
					printf("New client connected!\n");
#endif
                }
            }
            else {
                // CLIENT EVENT
                ClientTLS *c = events[i].data.ptr;

				// Check for hangup or error
                if (events[i].events & (EPOLLHUP | EPOLLERR))
                {
                    mark_client_for_close(c);
                    continue;
                }

                if (c->flags.state == HANDSHAKING)
                {
                    if (handle_handshake(epoll_fd, c) != OK)
                        mark_client_for_close(c);

                    continue;
                }

                // READ
                if (events[i].events & EPOLLIN)
                {
                    int err = handle_recv(c, epoll_fd);

                    if (err != OK) {
                        mark_client_for_close(c);
                    }
                }

                // WRITE
                if (events[i].events & EPOLLOUT)
                {
                    SERVER_STATUS err = flush_send(c);

                    if (err != OK) {
                        mark_client_for_close(c);
                        continue;
                    }

                    // If buffer is empty — disable EPOLLOUT
                    if (c->out_len == 0)
                    {
                        struct epoll_event mod;
                        mod.events = EPOLLIN | EPOLLET;
                        mod.data.ptr = c;

                        epoll_ctl(epoll_fd,
                            EPOLL_CTL_MOD,
                            c->socket,
                            &mod);
                    }
                }

                if (events[i].events & (EPOLLHUP | EPOLLERR))
                {
                    if (unlikely(remove_client(epoll_fd, c) != OK)) {
                        ERROR("Failed to remove client");
                    }
                }
            }

            for (size_t i = 0; i < clients.size; )
            {
                ClientTLS *c = clients.data[i];

                if (c->flags.closing)
                {
                    if (unlikely(remove_client(epoll_fd, c) != OK)) {
                        ERROR("Failed to remove client");
                    }
                    continue;
                }

                i++;
            }
        }
    }

    return 0;
}
