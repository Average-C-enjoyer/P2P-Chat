#ifndef UTILS_H
#define UTILS_H

#define NAME 16

void parse_usernames(const char *input, char usernames[][NAME], int *count) {
	*count = 0;
	const char *start = input;
	const char *end = input;

	while (*start) {
		// Skip spaces
		if (*start == ' ') {
			start++;
		}
		if (*start == '\0') break;
		// Find the end of the username
		end = start;
		while (*end != ' ' && *end != '\0')
			end++;
		// Copy the username into the array
		int length = end - start;
		if (length >= NAME)
			length = NAME - 1;

		memcpy(usernames[*count], start, length);
		usernames[*count][length] = '\0';
		usernames[*count][length] = '\0';
		(*count)++;
		start = end;
	}
}

//=============================
// Dynamic array implementation
//=============================
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

typedef enum {
	OK,
	ERR_NOMEM,
	ERR_BAD_INDEX,
} ErrorStatus;

#define define_array(type) \
typedef struct {           \
	type		*data;         \
	size_t	size;          \
	size_t   capacity;      \
	short err;              \
} Array_##type;

#define da_init(a, er)     \
	do {                    \
		(a)->data = NULL;    \
		(a)->size = 0;       \
		(a)->capacity = 0;   \
		(a)->err = OK;       \
	} while (0)

#define da_free(a)         \
	do {                    \
		if ((a)->data)       \
			free((a)->data);  \
		(a)->data = NULL;    \
		(a)->size = 0;       \
		(a)->capacity = 0;   \
	} while (0)

// Internal macro to handle realloc and error checking (do not use in code directly)
#define __GROW_ARRAY(a) do {                                                \
	size_t old_cap = (a)->capacity;                                          \
	size_t new_cap = old_cap ? old_cap + old_cap / 2 : 32;                   \
                                                                            \
	if (new_cap < old_cap) {                                                 \
		(a)->err = ERR_NOMEM;                                                 \
		break;                                                                \
	}                                                                        \
                                                                            \
	void *tmp = realloc((a)->data, new_cap * sizeof(*((a)->data)));          \
	if (!tmp) {                                                              \
		(a)->err = ERR_NOMEM;                                                 \
		break;                                                                \
	}                                                                        \
                                                                            \
	(a)->data = tmp;                                                         \
	(a)->capacity = new_cap;                                                 \
	(a)->err = OK;                                                           \
} while(0)

#define __REALLOC_ARRAY(a) do {                                             \
	void *tmp = realloc((a)->data, (a)->capacity * sizeof(*((a)->data)));    \
	if (!tmp) { (a)->err = ERR_NOMEM; break; }                               \
	(a)->data = tmp;                                                         \
	(a)->err = OK;                                                           \
} while(0)


// Append value to the end of the array, resizing if necessary
#define da_append(a, value) do {                       \
	if ((a)->size >= (a)->capacity) {                   \
		__GROW_ARRAY(a);                                 \
		if ((a)->err) break;                             \
	}                                                   \
	(a)->data[(a)->size++] = value;                     \
	(a)->err = OK;                                      \
} while(0)

// Shrink the array to fit its current size, freeing memory if size is 0
#define da_shrink_to_fit(a) do {                       \
	if ((a)->size == 0) {                               \
		free((a)->data);                                 \
		(a)->data = NULL;                                \
		(a)->capacity = 0;                               \
		break;                                           \
	}                                                   \
	(a)->capacity = (a)->size;                          \
	__REALLOC_ARRAY(a);                                 \
	if ((a)->err) break;		                            \
	(a)->err = OK;                                      \
} while(0)

// Delete element at index, shifting subsequent elements left
#define da_delete(a, index) do {                       \
	if (index < (a)->size) {                            \
		memmove(                                         \
			&(a)->data[index],                            \
			&(a)->data[index + 1],                        \
			((a)->size - index - 1) * sizeof(*(a)->data)  \
		);                                               \
		(a)->size--;                                     \
	} else {                                            \
		(a)->err = ERR_BAD_INDEX;                        \
		break;                                           \
	}                                                   \
	(a)->err = OK;                                      \
} while(0)

// Insert value at index, shifting subsequent elements right and resizing if necessary
#define da_insert_at(a, index, value) do {             \
	if ((index) > (a)->size) {                          \
		(a)->err = ERR_BAD_INDEX;                        \
		break;                                           \
	}                                                   \
	                                                    \
	if ((a)->size >= (a)->capacity) {                   \
		__GROW_ARRAY(a);                                 \
		if ((a)->err) break;                             \
	}                                                   \
	                                                    \
	memmove(                                            \
		&(a)->data[(index) + 1],                         \
		&(a)->data[(index)],                             \
		((a)->size - (index)) * sizeof(*(a)->data)       \
	);                                                  \
	                                                    \
	(a)->data[index] = (value);                         \
	(a)->size++;                                        \
	(a)->err = OK;                                      \
} while(0)

#define da_get_last_err(a) ((a)->err)

static inline void da_print_error(ErrorStatus err) {
	switch (err) {
		case OK: printf("OK\n"); break;
		case ERR_NOMEM: printf("Out of memory\n"); break;
		case ERR_BAD_INDEX: printf("Bad index\n"); break;
		default: printf("Unknown error\n");
	}
}

#define da_handle_error(a) if ((a)->err != OK) da_print_error((a)->err);



//==============================
// Dynamic string implementation
//==============================

typedef struct {
	char *data;
	size_t len;
	size_t cap;
} String;

inline void ds_init(String *s) {
	s->len = 0;
	s->cap = 16;
	s->data = malloc(s->cap);
	if (s->data) s->data[0] = '\0';
}

inline void ds_free(String *s) {
	if (s->data) 
		free(s->data);
	s->data = NULL;
	s->len = s->cap = 0;
}

// Internal function to grow the string's capacity, returns ERR_NOMEM on failure
static inline short ds_grow(String *s, size_t new_cap) {
	void *tmp = realloc(s->data, new_cap);
	if (!tmp) return ERR_NOMEM;

	s->data = tmp;
	s->cap = new_cap;
	return OK;
}

inline short ds_append_char(String *s, char c) {
	if (s->len + 2 > s->cap) {
		size_t new_cap = s->cap ? s->cap * 2 : 16;
		if (ds_grow(s, new_cap)) return ERR_NOMEM;
	}
	s->data[s->len++] = c;
	s->data[s->len] = '\0';
	return OK;
}

inline short ds_append_str(String *s, const char *str) {
	size_t add = strlen(str);
	if (s->len + add + 1 > s->cap)
		if (ds_grow(s, (s->len + add + 1) * 2)) return ERR_NOMEM;

	memcpy(s->data + s->len, str, add + 1);
	s->len += add;
	return OK;
}

#endif // UTILS_H