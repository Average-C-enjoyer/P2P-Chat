#pragma once

#include "server.h"
#include "queue.h"
#include <stdatomic.h>
#include <unistd.h>

static inline void send_fd_to_worker(Worker *w, int client_fd)
{
    q_push(w->client_fd_queue, client_fd);

    uint64_t one = 1;
    write(w->event_fd, &one, sizeof(one));
}


static inline void send_msg_to_worker(Worker *w, Message *msg)
{
    q_push(w->msg_queue, msg);

    uint64_t one = 1;
    write(w->event_fd, &one, sizeof(one));
}


void *run_worker(void *arg)
{
    Worker *w = (Worker *)arg;

    struct epoll_event events[MAX_EVENTS];

    while (1)
    {
        int ready = epoll_wait(w->epoll_fd, events, MAX_EVENTS, -1);

        for (int ev_idx = 0; ev_idx < ready; ev_idx++)
        {
            // Check if it's a notification from the main thread
            if (events[ev_idx].data.fd == w->event_fd)
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

                while (!q_is_empty(w->msg_queue))
                {
                    Message *msg = q_front(w->msg_queue);

                    broadcast_message(&w->clients, msg, w->epoll_fd);

                    if (atomic_fetch_sub(&msg->refcount, 1) == 1)
                    {
                        if (msg->data) free(msg->data);
                        if (msg) free(msg);
                    }

                    q_pop(w->msg_queue);
                }

                continue;
            }

            // CLIENT EVENT
            ClientTLS *c = events[ev_idx].data.ptr;

            // Check for hangup or error
            if (events[ev_idx].events & (EPOLLHUP | EPOLLERR))
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
            if (events[ev_idx].events & EPOLLIN)
            {
                SERVER_STATUS err = handle_recv(&w->clients, c, w->epoll_fd);
                if (err < 0)
                {
					mark_client_for_close(c);
                }
            }

            // WRITE
            if (events[ev_idx].events & EPOLLOUT)
            {
                SERVER_STATUS err = flush_send(c, w);

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

                    if (epoll_ctl(w->epoll_fd,
                        EPOLL_CTL_MOD,
                        c->socket,
                        &mod) < 0)
                    {
                        ERROR("Disabling EPOLLOUT failed");
                    }
                }

            }
        }

        for (size_t ci = 0; ci < w->clients.size; )
        {
            ClientTLS *c = w->clients.data[ci];

            if (c->flags.closing)
            {
                if (unlikely(remove_client(&w->clients, c, w->epoll_fd) != OK)) {
                    ERROR("Failed to remove client");
                }
                continue;
            }

            ci++;
        }
    }

    return NULL;
}