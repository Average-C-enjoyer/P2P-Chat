#include "MySocket.h"
#include "MsgEncrypt.h"
#include "utils.h"
#include "menu.h"
#include "terminal.h"

//#define DEBUG

// Extended client for TLS
typedef struct {
   Client base; // original struct
   SSL *ssl;
   SSL_CTX *ctx;
   unsigned char in_buffer[INPUT_BUFFER_SIZE];
   size_t in_len;
} ClientTLS;

define_array(String);
Array_String usernames;
// TODO: Add users amount handling for personal chats


// ============================
// Length-Prefixed Protocol
// ============================
static int send_packet(ClientTLS *c, const unsigned char *data, uint32_t len)
{
   if (len > MAX_MESSAGE_SIZE)
      return -1;

   uint32_t net_len = htonl(len);

   if (SSL_write(c->ssl, &net_len, sizeof(net_len)) <= 0)
      return -1;

   if (SSL_write(c->ssl, data, len) <= 0)
      return -1;

   return 0;
}

static int process_message(unsigned char *payload, uint32_t len)
{
   payload[len] = '\0';
   printf("%s\n", payload);
   return 0;
}

static int handle_recv(ClientTLS *c)
{
   int r = SSL_read(
      c->ssl,
      c->in_buffer + c->in_len,
      INPUT_BUFFER_SIZE - c->in_len
   );

   if (r <= 0)
      return -1;

   c->in_len += r;

   while (c->in_len >= 4)
   {
      uint32_t msg_len;
      memcpy(&msg_len, c->in_buffer, 4);
      msg_len = ntohl(msg_len);

      if (msg_len > MAX_MESSAGE_SIZE)
         return -1;

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

   return 0;
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

      if (len == 0)
         continue;

      if (send_packet(client, (unsigned char *)buffer, (uint32_t)len) < 0)
      {
         printf("Send failed\n");
         break;
      }
   }

   SSL_shutdown(client->ssl);
   return R_NULL;
}


// ============================
// RECV THREAD
// ============================
ClientRecieveMessage(lpParam)
{
   ClientTLS *client = (ClientTLS *)lpParam;

   while (1)
   {
      if (handle_recv(client) < 0)
      {
         printf("\nConnection closed\n");
         break;
      }
   }

   return R_NULL;
}


// ============================
// MAIN
// ============================
int main()
{
   ClientTLS client;
   memset(&client, 0, sizeof(client));

   struct addrinfo *result = NULL, *ptr = NULL, hints;
   THREAD_TYPE threads[THREAD_COUNT];
   char ip[IP_LENGTH];

   // START MENU
   terminal_init();

   StartMenu menu_button;
   init_menu(&menu_button);
   display_menu(&menu_button);
   handle_menu_selection(&menu_button);

   terminal_restore();

   INIT_WINSOCK();
   init_openssl();

   client.ctx = create_ctx();
   if (!client.ctx)
   {
      printf("TLS context failed\n");
      return 1;
   }

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;

#ifdef DEBUG
   printf("Enter server IP address: ");
   fgets(ip, sizeof(ip), stdin);
   ip[strcspn(ip, "\r\n")] = '\0';
#else
   strcpy(ip, "127.0.0.1");
#endif

   if (getaddrinfo(ip, DEFAULT_PORT, &hints, &result) != 0)
   {
      perror("getaddrinfo failed");
      return 1;
   }

   for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
   {
      client.base.socket = socket(
         ptr->ai_family,
         ptr->ai_socktype,
         ptr->ai_protocol
      );

      if (client.base.socket < 0)
         continue;

      printf("Connecting...");

      if (connect(
         client.base.socket,
         ptr->ai_addr,
         (int)ptr->ai_addrlen) == 0) {
         printf(" done.\n");
         break;
      }

		printf(" failed.\n");
      my_close(client.base.socket);
      client.base.socket = -1;
   }

   if (client.base.socket < 0)
   {
      printf("Unable to connect!\n");
      return 1;
   }

   freeaddrinfo(result);

	printf("TLS Handshake...\n");
   // =============TLS HANDSHAKE=============
   client.ssl = SSL_new(client.ctx);
   SSL_set_fd(client.ssl, (int)client.base.socket);

   if (SSL_connect(client.ssl) <= 0)
   {
		printf("TLS handshake failed\n");
      ERR_print_errors_fp(stderr);
      printf("After ERR_print_errors_fp\n");
      return 1;
   }
	printf("Verifying certificate...\n");

   if (!verify_certificate(client.ssl))
   {
      printf("Certificate verification failed\n");
      return 1;
   }

   printf("TLS connection established\n");

   // =============NAME INPUT=============
   while (1)
   {
      printf("Enter name (16 characters max): ");

      if (fgets(client.base.name, MAX_NAME_LEN, stdin))
      {
         size_t len = strlen(client.base.name);

         if (len && client.base.name[len - 1] == '\n')
            client.base.name[len - 1] = '\0';

         if (strchr(client.base.name, ' '))
         {
            printf("Invalid name: no spaces allowed\n");
            client.base.name[0] = '\0';
            continue;
         }
         break;
      }
   }

   // Send the client's name to the server
   /*send_packet(
      &client,
      (unsigned char *)client.base.name,
      (uint32_t)strlen(client.base.name)
   );*/

   // =============THREADS=============
   THREAD_CREATE(threads[SEND_THREAD], ClientSendMessage, &client);
   THREAD_CREATE(threads[RECV_THREAD], ClientRecieveMessage, &client);

   for (int i = 0; i < THREAD_COUNT; i++)
   {
#ifdef _WIN32
      if (threads[i] != NULL)
         THREAD_JOIN(threads[i]);
#else
      THREAD_JOIN(threads[i]);
#endif
   }

   // =============CLEANUP=============
   SSL_free(client.ssl);
   SSL_CTX_free(client.ctx);
   my_close(client.base.socket);
   WSA_CLEANUP();

   return 0;
}