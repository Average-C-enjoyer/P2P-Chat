#pragma once

#include <stdio.h>

#include "cross_platform_api.h"
#include "darray.h"
#include "String.h"
#include "TLS.h"

#define DEFAULT_PORT "4444"

#define MAX_MESSAGE_SIZE 4096

#define THREAD_COUNT 2
#define SEND_THREAD 0
#define RECV_THREAD 1

#define IP_LENGTH 15

typedef enum {
	OK = 0,
	ERR_INIT_CREATE_CTX = -1,
	ERR_INIT_GAI = -2,
	ERR_CONNECT = -3
} CLIENT_STATE;

static inline void client_print_error(CLIENT_STATE err) {
	switch (err) {
	case ERR_INIT_CREATE_CTX:
		fprintf(stderr, "Failed to create TLS context\n");
		break;
	case ERR_INIT_GAI:
		fprintf(stderr, "Failed to resolve server address\n");
		break;
	case ERR_CONNECT:
		fprintf(stderr, "Failed to connect to server\n");
		break;
	default:
		fprintf(stderr, "Unknown error\n");
	}
}

define_array(String);

/* Initialize TLS context, prepare hints and resolve server address */
CLIENT_STATE init_TLS_and_sock(ClientTLS *client, 
											 struct addrinfo *hints, 
											 struct addrinfo **result, 
											 char *ip);

/* Connect to the resolved address(es) */
CLIENT_STATE Connect(struct addrinfo *result, ClientTLS *client);

/* Perform TLS handshake */
TLS_STATE TLS_handshake(ClientTLS *client);

// Length-Prefixed Protocol
static TLS_STATE send_packet(ClientTLS *c, const unsigned char *data, uint32_t len);
static void process_message(unsigned char *payload, uint32_t len);
static TLS_STATE handle_recv(ClientTLS *c);

// Send and recv threads (macro expands to proper signature)
ClientSendMessage(clientPtr);
ClientRecieveMessage(lpParam);
