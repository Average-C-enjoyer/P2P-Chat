#include "MySocket.h"
#include "utils.h"
#include "MsgEncrypt.h"

typedef enum {
   STATE_HANDSHAKING,
   STATE_CONNECTED
} ClientState;

typedef struct {
   Client base;
   SSL *ssl;

   ClientState state;

   unsigned char in_buffer[INPUT_BUFFER_SIZE];
   size_t in_len;

   unsigned char out_buffer[INPUT_BUFFER_SIZE];
   size_t out_len;
   size_t out_sent; // bytes already sent from out_buffer
} ClientTLS;


define_array(ClientTLS);
Array_ClientTLS clients;

static SSL_CTX *server_ctx = NULL;


// Initializes the TLS server context, loading certificates and keys
static int init_tls_server()
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



static int add_client(my_socket_t sock)
{
   ClientTLS c;
   memset(&c, 0, sizeof(c));

   c.base.socket = sock;
   c.state = STATE_HANDSHAKING;

   c.ssl = SSL_new(server_ctx);
   SSL_set_fd(c.ssl, (int)sock);

   SET_NONBLOCK(sock);

   da_append(&clients, c);
   return OK;
}



// Removes a client, cleaning up resources
static void remove_client(size_t i)
{
   ClientTLS *c = &clients.data[i];

   SSL_shutdown(c->ssl);
   SSL_free(c->ssl);
   my_close(c->base.socket);

   da_delete(&clients, i);
   da_handle_error(&clients);
}


// Queues a packet to be sent to the client, adding length prefix and buffering
static int queue_packet(ClientTLS *c,
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


// Flushes the send buffer by writing to the SSL connection, handling partial writes
static int flush_send(ClientTLS *c)
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
static int handle_recv(ClientTLS *c)
{
   int r = SSL_read(
      c->ssl,
      c->in_buffer + c->in_len,
      INPUT_BUFFER_SIZE - c->in_len
   );

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

         queue_packet(dst, payload, msg_len);
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
   my_socket_t serverSocket;
   struct addrinfo hints, *result;

   INIT_WINSOCK();

   short tls_status = init_tls_server();
   if (tls_status < 0)
   {
      print_error(tls_status);
      return 1;
   }

   da_init(&clients);

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   getaddrinfo(NULL, DEFAULT_PORT,
      &hints, &result);

   serverSocket = socket(
      result->ai_family,
      result->ai_socktype,
      result->ai_protocol);

   bind(serverSocket,
      result->ai_addr,
      (int)result->ai_addrlen);

   listen(serverSocket, SOMAXCONN);
   SET_NONBLOCK(serverSocket);

   freeaddrinfo(result);

   printf("TLS Event Loop Server started\n");

   while (1)
   {
      size_t nfds = clients.size + 1;

      struct pollfd *fds =
         malloc(sizeof(struct pollfd) * nfds);

      // First fd is the server socket
      fds[0].fd = serverSocket;

      // We only care about incoming connections on the server socket
      fds[0].events = POLLIN;

      for (size_t i = 0; i < clients.size; i++)
      {
         fds[i + 1].fd =
            clients.data[i].base.socket;

         fds[i + 1].events = POLLIN;

         if (clients.data[i].out_len > 0)
            fds[i + 1].events |= POLLOUT;
      }

      int ready = poll(fds,
         (unsigned long)nfds, -1);

      if (ready <= 0)
      {
         free(fds);
         continue;
      }

      // New client connection
      if (fds[0].revents & POLLIN)
      {
         my_socket_t sock =
            accept(serverSocket, NULL, NULL);

         if (sock >= 0)
            add_client(sock);
      }

      // Handle client events
      for (size_t i = 0; i < clients.size;)
      {
         ClientTLS *c = &clients.data[i];

         if (c->state == STATE_HANDSHAKING)
         {
            int ret = SSL_accept(c->ssl);

            if (ret == 1)
            {
               printf("New client connected\n");
               c->state = STATE_CONNECTED;
            }
            else
            {
               int err = SSL_get_error(c->ssl, ret);

               if (err == SSL_ERROR_WANT_READ ||
                  err == SSL_ERROR_WANT_WRITE)
               {
                  // Just wait for the next poll
                  i++;
                  continue;
               }

               printf("TLS handshake fatal error\n");
               ERR_print_errors_fp(stderr);
               remove_client(i);
               continue;
            }

            i++;
            continue;
         }

         // Handle incoming data
         if (fds[i + 1].revents & POLLIN)
         {
            if (handle_recv(c) < 0)
            {
               remove_client(i);
               continue;
            }
         }

         // Handle outgoing data
         if (fds[i + 1].revents & POLLOUT)
         {
            if (flush_send(c) < 0)
            {
               remove_client(i);
               continue;
            }
         }

         i++;
      }

      free(fds);
   }

   return 0;
}
