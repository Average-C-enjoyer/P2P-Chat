#ifndef TERMINAL_H
#define TERMINAL_H

#ifdef _WIN32
	#include <windows.h>
	#define CLEAR_SCREEN() system("cls")
#else
	#include <stdio.h>
	#define CLEAR_SCREEN() system("clear")
#endif

void terminal_init(void);
void terminal_restore(void);

void terminal_clear_home(void);
void terminal_hide_cursor(void);
void terminal_show_cursor(void);

#endif
