#pragma once

#define NAME 16

//=============================
// Dynamic array implementation
//=============================
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

typedef enum {
	DA_OK,
	DA_ERR_NOMEM,
	DA_ERR_BAD_INDEX,
} DARRAY_STATUS;

#define define_array(type)   \
typedef struct {             \
	type		*data;       \
	size_t	     size;       \
	size_t       capacity;   \
	short        err;        \
} Array_##type;

#define da_init(a)           \
	do {                     \
		(a)->data = NULL;    \
		(a)->size = 0;       \
		(a)->capacity = 0;   \
		(a)->err = DA_OK;    \
	} while (0)

#define da_free(a)           \
	do {                     \
		if ((a)->data)       \
			free((a)->data); \
		(a)->data = NULL;    \
		(a)->size = 0;       \
		(a)->capacity = 0;   \
	} while (0)

// Internal macro to handle realloc and error checking (do not use in code directly)
#define __GROW_ARRAY(a) do {                                         \
	size_t old_cap = (a)->capacity;                                  \
	size_t new_cap = old_cap ? old_cap + old_cap / 2 : 32;           \
                                                                     \
	if (new_cap < old_cap) {                                         \
		(a)->err = DA_ERR_NOMEM;                                     \
		break;                                                       \
	}                                                                \
                                                                     \
	void *tmp = realloc((a)->data, new_cap * sizeof(*((a)->data)));  \
	if (!tmp) {                                                      \
		(a)->err = DA_ERR_NOMEM;                                     \
		break;                                                       \
	}                                                                \
                                                                     \
	(a)->data = tmp;                                                 \
	(a)->capacity = new_cap;                                         \
	(a)->err = DA_OK;                                                \
} while(0)

#define __REALLOC_ARRAY(a) do {                                             \
	void *tmp = realloc((a)->data, (a)->capacity * sizeof(*((a)->data)));   \
	if (!tmp) { (a)->err = DA_ERR_NOMEM; break; }                           \
	(a)->data = tmp;                                                        \
	(a)->err = DA_OK;                                                       \
} while(0)


// Append value to the end of the array, resizing if necessary
#define da_append(a, value) do {                         \
	if ((a)->size >= (a)->capacity) {                    \
		__GROW_ARRAY(a);                                 \
		if ((a)->err) break;                             \
	}                                                    \
	(a)->data[(a)->size++] = value;                      \
	(a)->err = DA_OK;                                    \
} while(0)

// Shrink the array to fit its current size, freeing memory if size is 0
#define da_shrink_to_fit(a) do {                         \
	if ((a)->size == 0) {                                \
		free((a)->data);                                 \
		(a)->data = NULL;                                \
		(a)->capacity = 0;                               \
		break;                                           \
	}                                                    \
	(a)->capacity = (a)->size;                           \
	__REALLOC_ARRAY(a);                                  \
	if ((a)->err) break;		                         \
	(a)->err = DA_OK;                                    \
} while(0)

// Delete element at index, shifting subsequent elements left
#define da_delete(a, index) do {                         \
	if (index < (a)->size) {                             \
		memmove(                                         \
			&(a)->data[index],                           \
			&(a)->data[index + 1],                       \
			((a)->size - index - 1) * sizeof(*(a)->data) \
		);                                               \
		(a)->size--;                                     \
	} else {                                             \
		(a)->err = DA_ERR_BAD_INDEX;                     \
		break;                                           \
	}                                                    \
	(a)->err = DA_OK;                                    \
} while(0)

// Insert value at index, shifting subsequent elements right and resizing if necessary
#define da_insert_at(a, index, value) do {               \
	if ((index) > (a)->size) {                           \
		(a)->err = DA_ERR_BAD_INDEX;                     \
		break;                                           \
	}                                                    \
	                                                     \
	if ((a)->size >= (a)->capacity) {                    \
		__GROW_ARRAY(a);                                 \
		if ((a)->err) break;                             \
	}                                                    \
	                                                     \
	memmove(                                             \
		&(a)->data[(index) + 1],                         \
		&(a)->data[(index)],                             \
		((a)->size - (index)) * sizeof(*(a)->data)       \
	);                                                   \
	                                                     \
	(a)->data[index] = (value);                          \
	(a)->size++;                                         \
	(a)->err = DA_OK;                                    \
} while(0)

#define da_get_last_err(a) ((a)->err)

static inline void da_print_error(DARRAY_STATUS err) {
	switch (err) {
		case DA_OK: printf("OK\n"); break;
		case DA_ERR_NOMEM: printf("Out of memory\n"); break;
		case DA_ERR_BAD_INDEX: printf("Bad index\n"); break;
		default: printf("Unknown error\n");
	}
}

#define da_handle_error(a) if ((a)->err != DA_OK) da_print_error((a)->err);
