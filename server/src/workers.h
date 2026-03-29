#pragma once

#include "server.h"


void send_fd_to_worker(Worker *w, int client_fd)
{
    if (w->client_fd_queue == NULL) {
        q_init(w->client_fd_queue, QUEUE_INIT_CAPACITY);
    }
    q_push(w->client_fd_queue, client_fd);

    uint64_t one = 1;
    write(w->event_fd, &one, sizeof(one));
}


void send_msg_to_worker(Worker *w, char *msg)
{
    if (w->msg_queue == NULL) {
		q_init(w->msg_queue, QUEUE_INIT_CAPACITY);
    }
    q_push(w->msg_queue, msg);
}


void *run_worker(void *arg)
{
    Worker *w = (Worker *)arg;

    struct epoll_event events[MAX_EVENTS];

    while (1) 
    {
        int ready = epoll_wait(w->epoll_fd, events, MAX_EVENTS, -1);
        printf("worker woke up\n");

        for (int i = 0; i < ready; i++)
        {
            if (events[i].data.fd == w->event_fd)
            {
                uint64_t val;
                read(w->event_fd, &val, sizeof(val));

                while (!q_is_empty(w->client_fd_queue))
                {
                    int fd = q_front(w->client_fd_queue);

                    if (add_client(w, fd) != OK) {
                        ERROR("Failed to add client");
                    }

                    q_pop(w->client_fd_queue);
                }

                continue;
            }

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
                if (handle_handshake(w->epoll_fd, c) != OK)
                    mark_client_for_close(c);
                continue;
            }

            // READ
            if (events[i].events & EPOLLIN)
            {
                char *message = handle_recv(&w->clients, c, w->epoll_fd);

                if (message == NULL) {
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

                    epoll_ctl(w->epoll_fd,
                        EPOLL_CTL_MOD,
                        c->socket,
                        &mod);
                }
            }
        }

        for (size_t i = 0; i < w->clients.size; )
        {
            ClientTLS *c = w->clients.data[i];

            if (c->flags.closing)
            {
                if (unlikely(remove_client(&w->clients, c, w->epoll_fd) != OK)) {
                    ERROR("Failed to remove client");
                }
                continue;
            }

            i++;
        }
    }
}

