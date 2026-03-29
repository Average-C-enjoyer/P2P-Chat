#pragma once
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define QUEUE_INIT_CAPACITY 1024

typedef struct {
    _Atomic size_t size;
    _Atomic size_t head;
    _Atomic size_t tail;
    _Atomic size_t capacity;
} QueueHeader;

// Don't use q_header, not part of api
#define q_header(q) ((QueueHeader*)(q) - 1)

#define q_front(q) ((q && q_header(q)->size > 0) ? (q)[q_header(q)->head] : 0)

#define q_size(q) ((q) ? q_header(q)->size : 0)


#define q_init(q, cap) do {                                             \
    size_t _cap = (cap);                                                \
    QueueHeader *h = malloc(sizeof(QueueHeader) + sizeof(*(q)) * _cap); \
    if (unlikely(!h)) { perror("malloc"); exit(1); }                    \
    h->size = 0;                                                        \
    h->head = 0;                                                        \
    h->tail = 0;                                                        \
    h->capacity = _cap;                                                 \
    (q) = (void*)(h + 1);                                               \
} while(0)



#define q_push(q, val) do {                                      \
    if (!(q)) q_init(q, QUEUE_INIT_CAPACITY);                    \
                                                                 \
    QueueHeader *h = q_header(q);                                \
                                                                 \
    (q)[h->tail] = (val);                                        \
                                                                 \
    if (h->size == h->capacity) {                                \
        /* overwrite oldest */                                   \
        h->head = (h->head + 1) % h->capacity;                   \
    } else {                                                     \
        h->size++;                                               \
    }                                                            \
                                                                 \
    h->tail = (h->tail + 1) % h->capacity;                       \
} while (0)



#define q_pop(q) do {                                            \
    if (q && q_header(q)->size > 0) {                            \
        QueueHeader *h = q_header(q);                            \
        h->head = (h->head + 1) % h->capacity;                   \
        h->size--;                                               \
    }                                                            \
} while(0)


#define q_clear(q) do {                          \
    if (q) {                                     \
        QueueHeader *h = q_header(q);            \
        h->size = 0;                             \
        h->head = 0;                             \
        h->tail = 0;                             \
    }                                            \
} while(0)


#define q_is_empty(q) ((q) ? (q_header(q)->size == 0) : 1)
