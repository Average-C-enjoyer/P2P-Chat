#pragma once

#include <string.h>
#include <stdint.h>

typedef enum {
	S_OK,
	S_ERR_NOMEM,
	S_ERR_BAD_INDEX,
} STRING_STATUS;

//==============================
// Dynamic string implementation
//==============================

typedef struct {
	char *data;
	size_t len;
	size_t cap;
} String;

static inline void ds_init(String *s) {
	s->len = 0;
	s->cap = 16;
	s->data = malloc(s->cap);
	if (s->data) s->data[0] = '\0';
}

static inline void ds_free(String *s) {
	if (s->data)
		free(s->data);
	s->data = NULL;
	s->len = s->cap = 0;
}

// Internal function to grow the string's capacity, returns ERR_NOMEM on failure
static inline int8_t ds_grow(String *s, size_t new_cap) {
	void *tmp = realloc(s->data, new_cap);
	if (!tmp) return S_ERR_NOMEM;

	s->data = tmp;
	s->cap = new_cap;
	return S_OK;
}

static inline int8_t ds_append_char(String *s, char c) {
	if (s->len + 2 > s->cap) {
		size_t new_cap = s->cap ? s->cap * 2 : 16;
		if (ds_grow(s, new_cap)) return S_ERR_NOMEM;
	}
	s->data[s->len++] = c;
	s->data[s->len] = '\0';
	return S_OK;
}

static inline int8_t ds_append_str(String *s, const char *str) {
	size_t add = strlen(str);
	if (s->len + add + 1 > s->cap)
		if (ds_grow(s, (s->len + add + 1) * 2)) return S_ERR_NOMEM;

	memcpy(s->data + s->len, str, add + 1);
	s->len += add;
	return S_OK;
}

inline void ds_print_error(STRING_STATUS err) {
	switch (err) {
	case S_ERR_NOMEM:
		fprintf(stderr, "String memory allocation failed\n");
		break;
	case S_ERR_BAD_INDEX:
		fprintf(stderr, "String bad index error\n");
		break;
	default:
		fprintf(stderr, "Unknown error\n");
	}
}

#define handle_error(err) if (err) ds_print_error(err); 
