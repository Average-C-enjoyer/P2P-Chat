#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdio.h>

#ifdef _WIN32
	#include <windows.h>
	#define CLEAR_SCREEN() system("cls")
#else
	#define CLEAR_SCREEN() system("clear")
#endif

typedef struct {
	int width;
	int height;
} TermSize;

static inline void print_at(int x, int y, const char *str) {
	printf("\033[%d;%dH", y, x);
	printf("%s", str);
	fflush(stdout);
}

static inline int center_x(int term_w, int content_w) {
	return (term_w - content_w) / 2;
}

void terminal_init(void);
void terminal_restore(void);

TermSize terminal_get_size(void);

void terminal_clear_home(void);
void terminal_hide_cursor(void);
void terminal_show_cursor(void);

#endif
