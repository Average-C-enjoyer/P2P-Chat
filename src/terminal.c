#ifdef _WIN32
#include <windows.h>

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

   SetConsoleOutputCP(CP_UTF8);                     \
   SetConsoleCP(CP_UTF8);
}

void terminal_restore(void) {
	system("cls");
   SetConsoleMode(hOut, originalMode);
   SetConsoleCursorInfo(hOut, &originalCursor);
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

void terminal_init(void) {
   printf("\033[?25l"); // hide cursor
   fflush(stdout);
}

void terminal_restore(void) {
	system("clear");
   printf("\033[?25h"); // show cursor
   fflush(stdout);
}

void terminal_clear_home(void) {
	printf("\033[H"); // move cursor to 0, 0 position
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
