#include "client_core.h"
#include <string.h>

CLIENT_STATE init_TLS_and_sock(ClientTLS *client,
    struct addrinfo *hints,
    struct addrinfo **result,
    char *ip)
{
    INIT_WINSOCK();
    init_openssl();

    client->ctx = create_ctx();
    if (!client->ctx) {
        printf("TLS context failed\n");
        return ERR_INIT_CREATE_CTX;
    }

    memset(hints, 0, sizeof(*hints));

    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_protocol = IPPROTO_TCP;

    int gai = getaddrinfo(ip, DEFAULT_PORT, hints, result);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(gai));
        return ERR_INIT_GAI;
    }

    return OK;
}


CLIENT_STATE Connect(struct addrinfo *result, ClientTLS *client) {
    struct addrinfo *ptr = NULL;
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        client->socket = socket(
            ptr->ai_family,
            ptr->ai_socktype,
            ptr->ai_protocol
        );

        if (client->socket < 0)
            continue;

        printf("Connecting...");

        if (connect(
            client->socket,
            ptr->ai_addr,
            (int)ptr->ai_addrlen) == 0) 
        {
            printf(" done.\n");
            break;
        }

        printf(" failed.\n");
        CLOSE(client->socket);
        client->socket = -1;
    }

    if (client->socket < 0) {
        fprintf(stderr, "Unable to connect!\n");
        return ERR_CONNECT;
    }

    freeaddrinfo(result);
    return OK;
}


TLS_STATE TLS_handshake(ClientTLS *client) {
    printf("TLS Handshake...\n");

    client->ssl = SSL_new(client->ctx);
    SSL_set_fd(client->ssl, (int)client->socket);

    printf("done\n");

    if (SSL_connect(client->ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        perror("SSL_connect failed");
        return SSL_ACCEPT_FAIL;
    }
    printf("Verifying certificate...\n");

    if (!verify_certificate(client->ssl))
    {
        printf("Certificate verification failed\n");
        return TLS_BAD_CERT;
    }

    printf("TLS connection established\n");
}


// ============================
// Length-Prefixed Protocol
// ============================
static TLS_STATE send_packet(ClientTLS *c, const unsigned char *data, uint32_t len)
{
    if (len > MAX_MESSAGE_SIZE)
        return BUFFER_OVERFLOW;

    uint32_t net_len = htonl(len);

    if (SSL_write(c->ssl, &net_len, sizeof(net_len)) <= 0)
        return SEND_FAIL;

    if (SSL_write(c->ssl, data, len) <= 0)
        return SEND_FAIL;

    return TLS_OK;
}

static void process_message(unsigned char *payload, uint32_t len)
{
    payload[len] = '\0';
    printf("%s\n", payload);
}

static TLS_STATE handle_recv(ClientTLS *c)
{
    int r = SSL_read(
        c->ssl,
        c->in_buffer + c->in_len,
        INPUT_BUFFER_SIZE - c->in_len
    );

    if (r <= 0)
        return RECV_FAIL;

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

        unsigned char *payload = c->in_buffer + 4;

        process_message(payload, msg_len);

        memmove(
            c->in_buffer,
            c->in_buffer + 4 + msg_len,
            c->in_len - 4 - msg_len
        );

        c->in_len -= (4 + msg_len);
    }

    return TLS_OK;
}


// ============================
// SEND THREAD
// ============================
ClientSendMessage(clientPtr)
{
    ClientTLS *client = (ClientTLS *)clientPtr;
    char buffer[MAX_MESSAGE_SIZE];

    while (1)
    {
        if (!fgets(buffer, sizeof(buffer), stdin))
            break;

        size_t len = strlen(buffer);

        if (len && buffer[len - 1] == '\n')
            buffer[--len] = '\0';

        if (strcmp(buffer, "exit") == 0)
            break;

        if (len == 0) continue;

        short err = send_packet(client, (unsigned char *)buffer, (uint32_t)len);
        if (err < 0) {
            tls_print_error(err);
            break;
        }
    }

    SSL_shutdown(client->ssl);
	SSL_free(client->ssl);
	CLOSE(client->socket);
    return NULL;
}


// ============================
// RECV THREAD
// ============================
ClientRecieveMessage(lpParam)
{
    ClientTLS *client = (ClientTLS *)lpParam;

    while (1) {
        short err = handle_recv(client);
        if (err < 0) {
            tls_print_error(err);
            break;
        }
    }

    return NULL;
}