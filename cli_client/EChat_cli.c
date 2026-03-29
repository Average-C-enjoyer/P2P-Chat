#include "client_core.h"
#include "menu.h"
#include "terminal.h"

#include <string.h>

// TODO: Add users amount handling for personal chats

int main(int argc, char *argv[]) {
    char ip[IP_LENGTH];
    
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [ip]\n", argv[0]);
        printf("A simple TLS chat client.\n");
        printf("Options:\n");
        printf("  --help    Show this help message and exit\n");
        return 0;
    }

    if (argc > 1) {
        strncpy(ip, argv[1], IP_LENGTH - 1);
        ip[IP_LENGTH - 1] = '\0';
    }
    else {
        strcpy(ip, "127.0.0.1");
    }

    ClientTLS client;
    memset(&client, 0, sizeof(client));

    struct addrinfo *result = NULL, hints;
    THREAD threads[THREAD_COUNT];

    // START MENU
    terminal_init();

    StartMenu menu_button;

    init_menu(&menu_button);
    display_menu(&menu_button);
    handle_menu_selection(&menu_button);

    terminal_restore();

#ifndef _WIN32
    system("stty sane");
#endif

    short err = init_TLS_and_sock(&client, &hints, &result, ip);
    if (err < 0) {
        client_print_error(err);
        return 1;
    }

    err = Connect(result, &client);
    if (err < 0) {
        client_print_error(err);
        return 1;
    }

    err = TLS_handshake(&client);
    if (err < 0) {
        tls_print_error(err);
        return 1;
    }

    // Name input
    while (1)
    {
        printf("Enter name (16 characters max): ");
        scanf("%16s", client.name);

        size_t len = strlen(client.name);

        if (len && client.name[len - 1] == '\n')
            client.name[len - 1] = '\0';

        if (strchr(client.name, ' '))
        {
            printf("Invalid name: no spaces allowed\n");
            client.name[0] = '\0';
            continue;
        }
        break;
    }

    system("clear");
    CLEAR_SCREEN();

    // Send and receive threads
    THREAD_CREATE(threads[SEND_THREAD], ClientSendMessage, &client);
    THREAD_CREATE(threads[RECV_THREAD], ClientRecieveMessage, &client);

    for (int i = 0; i < THREAD_COUNT; i++) {
        THREAD_JOIN(threads[i]);
    }

    CLEANUP_CLIENT(&client);

    return 0;
}