#ifndef MENU_H
#define MENU_H

#include <stdio.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <termios.h>
	#include <unistd.h>
#endif

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