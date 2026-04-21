#include "terminal.h"
#include <stdlib.h>

#ifdef _WIN32

static HANDLE hOut;
static DWORD originalMode;
static CONSOLE_CURSOR_INFO originalCursor;

void terminal_init(void) {
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleMode(hOut, &originalMode);
	SetConsoleMode(hOut, originalMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	GetConsoleCursorInfo(hOut, &originalCursor);

	CONSOLE_CURSOR_INFO ci = originalCursor;
	ci.bVisible = FALSE;
	SetConsoleCursorInfo(hOut, &ci);

	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
}

void terminal_restore(void) {
	system("cls");
	SetConsoleMode(hOut, originalMode);
	SetConsoleCursorInfo(hOut, &originalCursor);
}

TermSize terminal_get_size(void) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

	TermSize size = {
		csbi.srWindow.Right - csbi.srWindow.Left + 1,
		csbi.srWindow.Bottom - csbi.srWindow.Top + 1
	};
	return size;
}

void terminal_clear_home(void) {
	COORD c = { 0, 0 };
	SetConsoleCursorPosition(hOut, c);
}

void terminal_hide_cursor(void) {
	CONSOLE_CURSOR_INFO ci = originalCursor;
	ci.bVisible = FALSE;
	SetConsoleCursorInfo(hOut, &ci);
}

void terminal_show_cursor(void) {
	SetConsoleCursorInfo(hOut, &originalCursor);
}

#else // Linux / POSIX

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

void terminal_init(void) {
	printf("\033[?25l"); // hide cursor
	system("clear");
	fflush(stdout);
}

void terminal_restore(void) {
	system("clear");
	printf("\033[?25h"); // show cursor
	fflush(stdout);
}

TermSize terminal_get_size(void) {
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

	TermSize size = { ws.ws_col, ws.ws_row };
	return size;
}

void terminal_clear_home(void) {
	system("clear");
	printf("\033[H");  // move cursor home
	fflush(stdout);
}

void terminal_hide_cursor(void) {
	printf("\033[?25l"); // hide cursor
	fflush(stdout);
}

void terminal_show_cursor(void) {
	printf("\033[?25h"); // show cursor
	fflush(stdout);
}
#endif
