#ifndef MENU_H
#define MENU_H

#include <stdio.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <termios.h>
	#include <unistd.h>
#endif

#define RESET_COLOR  "\033[0m"

#define RED          "\x1B[31m"
#define GREEN        "\x1B[32m"
#define YELLOW       "\x1B[33m"
#define BLUE         "\x1B[34m"
#define MAGENTA      "\x1B[35m"
#define CYAN         "\x1B[36m"
#define WHITE        "\x1B[37m"
#define LIGHT_YELLOW "\x1B[93m"

#define BG_GREEN                "\033[3;42;30m"
#define BG_YELLOW               "\033[3;43;30m"
#define BG_BLUE                 "\033[3;44;30m"
#define BG_LIGHT_BLUE           "\033[3;104;30m"
#define BG_DARK_GRAY            "\033[3;100;30m"
#define BG_LIGHT_MAGENTA        "\033[3;47;35m"
#define BG_LIGHT_MAGENTA_DIM    "\033[2;47;35m"
#define BG_LIGHT_MAGENTA_BRIGHT "\033[1;47;35m"

typedef enum {
   BUTTON_REG,
   BUTTON_GUEST
} ActiveButtonType;

typedef struct {
   const char *hint;
   const char *btn_reg;
   const char *btn_guest;
   ActiveButtonType type;
} StartMenu;

// API
void init_menu(StartMenu *menu);
void display_menu(StartMenu *menu);
void handle_menu_selection(StartMenu *menu);

#endif // MENU_H