#include "terminal.h"
#include "menu.h"

#include <stdlib.h>

#define BIG_LOGO_WIDTH 40
#define SMALL_LOGO_WIDTH 19

// Array of lines so i can print and center it
const char *logo_lines[] = {
GREEN
"████████  ███████ ██                ██",
"██▓      ▓██      ██        ████    ██",
"█▓▓▓░▓   ▓█▓      █▓▓▓██   █▓  ██ ███▓▓█▓",
"▓▓░      ░▓▓      █▓   ▓▓  █▓▓█▓▓   ▓▓░",
"░░▓░░▓░░  ░▓░░▓░░ ▓░   ░░  ▓░  ░░    ▓░░"
};


const char *small_logo_lines[] = {
GREEN
"██▀ ▄▀▀ █▄█ ▄▀▄ ▀█▀",
"█▄▄ ▀▄▄ █ █ █▀█  █ "
RESET_COLOR
};

const char *btn_reg_lines[] = {
"╔────────────────────╗",
"│ SIGN IN OR SIGN UP │",
"╚────────────────────╝"
};

const char *btn_reg_lines_choosed[] = {
GREEN
"╔────────────────────╗",
"│ SIGN IN OR SIGN UP │",
"╚────────────────────╝"
RESET_COLOR
};

const char *btn_guest_lines[] = {
"╔────────────────────╗",
"│ CONTINUE AS GUEST  │",
"╚────────────────────╝"
};

const char *btn_guest_lines_choosed[] = {
GREEN
"╔────────────────────╗",
"│ CONTINUE AS GUEST  │",
"╚────────────────────╝"
RESET_COLOR
};

// Main window assets

const char *group_chat_header =
GREEN
"██▀ ▄▀▀ █▄█ ▄▀▄ ▀█▀╒══════════════════════════════════════════════════════════════════════════════════════════════╗\n"
"█▄▄ ▀▄▄ █ █ █▀█  █ │                                                                                              ║\n"
"╓──────────────────┴──────────────────────────────────────────────────────────────────────────────────────────────║\n"
RESET_COLOR;

const char *p2p_chat_header =
GREEN
"██▀ ▄▀▀ █▄█ ▄▀▄ ▀█▀╒══════════════════════════════════════════════════════════════════════════════════════════════╗\n"
"█▄▄ ▀▄▄ █ █ █▀█  █ │                                                                                              ║\n"
"╓──────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────║\n"
RESET_COLOR;


// API
void init_menu(StartMenu *menu) {
    menu->btn_reg = btn_reg_lines_choosed;
    menu->btn_guest = btn_guest_lines;
    menu->type = BUTTON_REG;
}

void draw_frame(int w, int h) {
    // Top border
    printf(GREEN"╔");
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╗");

    // Middle
    for (int y = 1; y < h - 1; y++) {
        printf("\n║");
        for (int i = 0; i < w - 2; i++) printf(" ");
        printf("║");
    }

    // Bottom
    printf("\n╚");
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╝"RESET_COLOR);
}

void display_menu(StartMenu *menu) {
    TermSize ts = terminal_get_size();

    terminal_clear_home();

    draw_frame(ts.width, ts.height);

    int y = 3;

    // --- choose logo ---
    int use_tiny = ts.width < BIG_LOGO_WIDTH;

    const char **logo;
    int logo_h;
    int logo_w;

    if (use_tiny) {
        logo = small_logo_lines;
        logo_h = 2;
        logo_w = 22;
    }
    else {
        logo = logo_lines;
        logo_h = 5;
        logo_w = 48;
    }

    // --- print logo ---
    int start_x = center_x(ts.width, logo_w);

    for (int i = 0; i < logo_h; i++) {
        print_at(start_x, y + i, logo[i]);
    }

    y += logo_h + 3;

    // --- hint ---
    const char *hint_line =
        "Use " GREEN "UP" RESET_COLOR "/" GREEN "DOWN" RESET_COLOR " to navigate, " GREEN "ENTER" RESET_COLOR " to select";

    print_at(center_x(ts.width, 40), y, hint_line);

    y += 2;

    // --- buttons ---
    int btn_x = center_x(ts.width, 28);

    if (btn_x < 2) btn_x = 2;
    if (btn_x > ts.width - 28) btn_x = ts.width - 28;

    for (int i = 0; i < 3; i++) {
        print_at(btn_x, y + i, menu->btn_reg[i]);
    }
    y += 3;

    for (int i = 0; i < 3; i++) {
        print_at(btn_x, y + i, menu->btn_guest[i]);
    }
}

void switch_button(StartMenu *menu) {
    if (menu->type == BUTTON_REG) {
        menu->type = BUTTON_GUEST;
        menu->btn_guest = btn_guest_lines_choosed;
        menu->btn_reg = btn_reg_lines;
    }
    else if (menu->type == BUTTON_GUEST) {
        menu->type = BUTTON_REG;
        menu->btn_reg = btn_reg_lines_choosed;
        menu->btn_guest = btn_guest_lines;
    }
}

void reg_pressed(void) {
    printf("REG pressed\n");
}

#define guest_pressed() return

#ifdef _WIN32

void handle_menu_selection(StartMenu *menu)
{
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD record;
    DWORD events;

    // Disable line buffering & echo
    SetConsoleMode(hInput, ENABLE_EXTENDED_FLAGS);

    while (1) {
        ReadConsoleInput(hInput, &record, 1, &events);

        if (record.EventType == KEY_EVENT &&
            record.Event.KeyEvent.bKeyDown) {

            WORD key = record.Event.KeyEvent.wVirtualKeyCode;

            if (key == VK_UP || key == VK_DOWN) {
                switch_button(menu);
                terminal_clear_home();
                display_menu(menu);
            }
            else if (key == VK_RETURN) {
                if (menu->type == BUTTON_REG)
                    reg_pressed();
                else
                    guest_pressed();
            }
        }
    }
}
#else // Linux / POSIX

static struct termios global_orig;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &global_orig);
}

static void enable_raw_mode(void) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &global_orig);
    raw = global_orig;

    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    atexit(restore_terminal);
}

void restore_stdin_mode(void)
{
    struct termios t;

    tcgetattr(STDIN_FILENO, &t);

    t.c_lflag |= (ICANON | ECHO);  // вернуть нормальный режим

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

void handle_menu_selection(StartMenu *menu) {
    char c;

    enable_raw_mode();

    while (1) {
        if (read(STDIN_FILENO, &c, 1) != 1)
            continue;

        if (c == '\033') { // ESC
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[' &&
                (seq[1] == 'A' || seq[1] == 'B')) {

                switch_button(menu);
                terminal_clear_home();
                display_menu(menu);
            }
        }
        else if (c == '\n' || c == '\r') {
            if (menu->type == BUTTON_REG)
                reg_pressed();
            else
                guest_pressed();
            break;
        }
    }

    restore_stdin_mode();
}

#endif // _WIN32