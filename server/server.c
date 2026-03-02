#include "server.h"
#include "darray.h"

define_array(ClientTLS);
Array_ClientTLS clients;

static SSL_CTX *server_ctx = NULL;

// Initializes the TLS server context, loading certificates and keys
static SERVER_STATUS init_tls_server()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    server_ctx = SSL_CTX_new(TLS_server_method());
    if (!server_ctx)
        return TLS_BAD_CONTEXT;

    if (SSL_CTX_use_certificate_file(server_ctx,
        "server.crt", SSL_FILETYPE_PEM) <= 0)
        return TLS_BAD_CERT;

    if (SSL_CTX_use_PrivateKey_file(server_ctx,
        "server.key", SSL_FILETYPE_PEM) <= 0)
        return TLS_BAD_KEY;

    return OK;
}


static SERVER_STATUS add_client(int sock)
{
    ClientTLS c;
    memset(&c, 0, sizeof(c));

    c.socket = sock;
    c.state = STATE_HANDSHAKING;

    c.ssl = SSL_new(server_ctx);
    if (!c.ssl)
        return SSL_INIT_FAIL;

    if (SSL_set_fd(c.ssl, (int)sock) <= 0)
		return SSL_INIT_FAIL;

    set_nonblocking(sock);

    da_append(&clients, c);
    return OK;
}


// Removes a client, cleaning up resources
static SERVER_STATUS remove_client(ClientTLS *c)
{
    SSL_shutdown(c->ssl);
    SSL_free(c->ssl);
    close(c->socket);

    size_t index = c - clients.data;
    da_delete(&clients, index);
    da_handle_error(&clients);

    return OK;
}


static SERVER_STATUS handle_handshake(int epoll_fd, ClientTLS *c)
{
    int r = SSL_accept(c->ssl);

    if (r == 1)
    {
        c->state = STATE_CONNECTED;

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

    return SSL_ACCEPT_FAIL;
}

// Queues a packet to be sent to the client, adding length prefix and buffering
static SERVER_STATUS queue_packet(ClientTLS *c,
    const unsigned char *data,
    uint32_t len)
{
    if (len > MAX_MESSAGE_SIZE)
        return BUFFER_OVERFLOW;

    if (c->out_len + 4 + len > INPUT_BUFFER_SIZE)
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
    printf("sending message: %s\n", c->out_buffer);
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

            return SSL_SEND_FAIL;
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
    int r = SSL_read(
        c->ssl,
        c->in_buffer + c->in_len,
        INPUT_BUFFER_SIZE - c->in_len
    );

    printf("Received: %s\n", c->in_buffer + c->in_len);

    if (r <= 0)
    {
        int err = SSL_get_error(c->ssl, r);

        if (err == SSL_ERROR_WANT_READ ||
            err == SSL_ERROR_WANT_WRITE)
            return OK;

        return SSL_RECV_FAIL;
    }

    c->in_len += r;

    while (c->in_len >= 4)
    {
        uint32_t msg_len;
        memcpy(&msg_len, c->in_buffer, 4);
        msg_len = ntohl(msg_len);

        if (msg_len > MAX_MESSAGE_SIZE)
            return BUFFER_OVERFLOW;

        if (c->in_len < 4 + msg_len)
            break;

        unsigned char *payload =
            c->in_buffer + 4;

        // Bradcast
        for (size_t i = 0; i < clients.size; i++)
        {
            ClientTLS *dst = &clients.data[i];
            if (dst == c) continue;

            short err = queue_packet(dst, payload, msg_len);
            if (err != OK) {
                print_error_server(err);
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

    if (init_tls_server() < 0) {
        ERROR("TLS initialization failed");
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

    if (serverSocket < 0) {
        ERROR("Failed to create socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ERROR("setsockopt(SO_REUSEADDR) failed");
        close(serverSocket);
        return 1;
    }

    if (bind(serverSocket, result->ai_addr, result->ai_addrlen) < 0) {
        ERROR("Failed to bind socket");
        close(serverSocket);
		return 1;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
		ERROR("Failed to listen on socket");
		close(serverSocket);
        return 1;
    }

    set_nonblocking(serverSocket);
    freeaddrinfo(result);

    int epoll_fd = epoll_create1(0);

    struct epoll_event ev;
    struct epoll_event *events =
        malloc(sizeof(struct epoll_event) * MAX_EVENTS);

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

                    if (add_client(client_fd) != OK) {
                        close(client_fd);
                        ERROR("Failed to add client");
                        continue;
                    }

                    ClientTLS *c = &clients.data[clients.size - 1];

                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN | EPOLLET;
                    client_ev.data.ptr = c;

                    epoll_ctl(epoll_fd,
                        EPOLL_CTL_ADD,
                        client_fd,
                        &client_ev);

					printf("New client connected!\n");
                }
            }
            else {
                // CLIENT EVENT
                ClientTLS *c = events[i].data.ptr;

				// Check for hangup or error
                if (events[i].events & (EPOLLHUP | EPOLLERR))
                {
                    remove_client(c);
                    continue;
                }

                if (c->state == STATE_HANDSHAKING)
                {
                    if (handle_handshake(epoll_fd, c) != OK)
                        remove_client(c);

                    continue;
                }

                // READ
                if (events[i].events & EPOLLIN)
                {
                    while (1)
                    {
                        int err = handle_recv(c, epoll_fd);

                        if (err == OK) {
                            // If SSL_read inside returned WANT_* —
                            // handle_recv will return OK,
                            // but there is no more data.
                            // Exit the loop.
                            break;
                        }
                        else {
                            remove_client(c);
                            break;
                        }
                    }
                }

                // WRITE
                if (events[i].events & EPOLLOUT)
                {
                    SERVER_STATUS err = flush_send(c);

                    if (err != OK) {
                        remove_client(c);
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

                if (events[i].events &
                    (EPOLLHUP | EPOLLERR))
                {
                    remove_client(c);
                }
            }
        }
    }

    return 0;
}
