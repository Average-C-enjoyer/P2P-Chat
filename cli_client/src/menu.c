#include "menu.h"
#include "terminal.h"

// Start menu assets
const char hint[] = "\n\n\n\n\tUse \x1B[32mUP\x1B[0m and \x1B[32mDOWN\x1B[0m arrows to navigate, \x1B[32mENTER\x1B[0m to select\n\n";

const char *logo =
u8"\n\n"
u8"\t\t\x1B[32m████████  ███████ ██                ██   \033[0m\n"
u8"\t\t\x1B[32m██▓      ▓██      ██        ████    ██   \033[0m\n"
u8"\t\t\x1B[32m█▓▓▓░▓   ▓█▓      █▓▓▓██   █▓  ██ ███▓▓█▓\033[0m\n"
u8"\t\t\x1B[32m▓▓░      ░▓▓      █▓   ▓▓  █▓▓█▓▓   ▓▓░  \033[0m\n"
u8"\t\t\x1B[32m░░▓░░▓░░  ░▓░░▓░░ ▓░   ░░  ▓░  ░░    ▓░░ \033[0m\n";

const char *btn_reg =
"\t.--------------------.\n"
"\t| SIGN IN OR SIGN UP |\n"
"\t'--------------------'\n";

const char *btn_reg_choosed =
"\t\x1B[32m.--------------------.\033[0m\n"
"\t\x1B[32m| SIGN IN OR SIGN UP |\033[0m\n"
"\t\x1B[32m'--------------------'\033[0m\n";

const char *btn_guest =
"\t.--------------------.\n"
"\t| CONTINUE AS GUEST  |\n"
"\t'--------------------'\n";

const char *btn_guest_choosed =
"\t\x1B[32m.--------------------.\033[0m\n"
"\t\x1B[32m| CONTINUE AS GUEST  |\033[0m\n"
"\t\x1B[32m'--------------------'\033[0m\n";


// API
void init_menu(StartMenu *menu) {
    menu->hint = hint;
    menu->btn_reg = btn_reg_choosed;
    menu->btn_guest = btn_guest;
    menu->type = BUTTON_REG;
}

void display_menu(StartMenu *menu) {
    printf("%s", logo);
    printf("%s", menu->hint);
    printf("%s", menu->btn_reg);
    printf("%s", menu->btn_guest);
}

void switch_button(StartMenu *menu) {
    if (menu->type == BUTTON_REG) {
        menu->type = BUTTON_GUEST;
        menu->btn_guest = btn_guest_choosed;
        menu->btn_reg = btn_reg;
    }
    else if (menu->type == BUTTON_GUEST) {
        menu->type = BUTTON_REG;
        menu->btn_reg = btn_reg_choosed;
        menu->btn_guest = btn_guest;
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

static void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
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

    disable_raw_mode(&global_orig);
}

#endif // _WIN32
