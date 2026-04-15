#pragma once

#include "server.h"

#include "spsc_queue.h"

#include <stdatomic.h>
#include <unistd.h>

static inline void send_fd_to_worker(Worker *w, int client_fd)
{
    q_push(w->client_fd_queue, client_fd);

    uint64_t one = 1;
    write(w->event_fd, &one, sizeof(one));
}


static inline void send_msg_to_workers(Message *msg, int current_worker_id)
{
    for (int wi = 0; wi < workers_count; wi++)
    {
        q_push(msg_queues[current_worker_id][wi], msg);

		//DEBUG("Sent message to worker %d from worker %d\n", 
        //    workers[wi].id, current_worker_id);

        uint64_t one = 1;
        write(workers[wi].event_fd, &one, sizeof(one));
    }
}


void *run_worker(void *arg)
{
    Worker *current_worker = (Worker *)arg;

    struct epoll_event events[MAX_EVENTS];

    while (1)
    {
        int ready = epoll_wait(current_worker->epoll_fd, events, MAX_EVENTS, -1);

        for (int ev_idx = 0; ev_idx < ready; ev_idx++)
        {
            // Check if it's a notification from the main thread
            if (events[ev_idx].data.fd == current_worker->event_fd)
            {
                uint64_t val;
                read(current_worker->event_fd, &val, sizeof(val));

                while (!q_is_empty(current_worker->client_fd_queue))
                {
                    int fd = q_pop_val(current_worker->client_fd_queue);

                    if (add_client(current_worker, fd) != OK) {
                        ERROR("Failed to add client");
                    }

					DEBUG("Worker id: %d\n", current_worker->id);
                }

                for (int i = 0; i < workers_count; i++)
                {
                    if (i == current_worker->id) continue;

                    Message **q = msg_queues[i][current_worker->id];

                    while (!q_is_empty(q))
                    {
                        Message *msg = q_pop_val(q);

                        broadcast_message(&current_worker->clients, msg, current_worker->epoll_fd);

                        if (atomic_fetch_sub(&msg->refcount, 1) == 1)
                        {
                            free(msg->data);
                            free(msg);
                        }
                    }
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
                if (handle_handshake(current_worker->epoll_fd, c) != OK)
                    mark_client_for_close(c);
                continue;
            }

            // READ
            if (events[ev_idx].events & EPOLLIN)
            {
                SERVER_STATUS err = handle_recv(
                    &current_worker->clients,
                    c,
                    current_worker->epoll_fd,
                    current_worker->id
                );
                if (err < 0)
                {
					mark_client_for_close(c);
                }
            }

            // WRITE
            if (events[ev_idx].events & EPOLLOUT)
            {
                SERVER_STATUS err = flush_send(c, current_worker);

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

                    if (epoll_ctl(current_worker->epoll_fd,
                        EPOLL_CTL_MOD,
                        c->socket,
                        &mod) < 0)
                    {
                        ERROR("Disabling EPOLLOUT failed");
                    }
                }

            }
        }

        for (size_t ci = 0; ci < current_worker->clients.size; )
        {
            ClientTLS *c = current_worker->clients.data[ci];

            if (c->flags.closing)
            {
                if (unlikely(remove_client(&current_worker->clients, c, current_worker->epoll_fd) != OK)) {
                    ERROR("Failed to remove client");
                }
                continue;
            }

            ci++;
        }
    }

    return NULL;
}