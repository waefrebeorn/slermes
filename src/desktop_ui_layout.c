/* desktop_ui_layout.c -- extracted from src/app_desktop.c (angel-coder monolith split).
 * Self-contained desktop UI/PTY concern module. See app_desktop_internals.h.
 */

#include "app_desktop_internals.h"

void init_colors_for_theme(desktop_theme_t theme) {
    if (!has_colors()) return;
    start_color();

    if (theme == THEME_LIGHT) {
        /* ── LIGHT THEME ────────────────────────────────────────────── */
        init_pair(CP_TITLEBAR,         COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_TITLEBAR_HL,      COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_TITLEBAR_WARN,    COLOR_WHITE,   COLOR_RED);
        init_pair(CP_SIDEBAR,          COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_SIDEBAR_SEL,      COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_SIDEBAR_HL,       COLOR_BLUE,    COLOR_WHITE);
        init_pair(CP_SIDEBAR_HEADING,  COLOR_BLUE,    COLOR_WHITE);
        init_pair(CP_SIDEBAR_PINNED,   COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_SIDEBAR_DIM,      COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_SIDEBAR_META,     COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_BG,          COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_USER,        COLOR_BLUE,    COLOR_WHITE);
        init_pair(CP_CHAT_ASSISTANT,   COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_SYSTEM,      COLOR_MAGENTA, COLOR_WHITE);
        init_pair(CP_CHAT_TOOL,        COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_ERROR,       COLOR_RED,     COLOR_WHITE);
        init_pair(CP_CHAT_DIM,         COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_BOLD,        COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_CODE,        COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_CHAT_LINK,        COLOR_BLUE,    COLOR_WHITE);
        init_pair(CP_COMPOSER,         COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_STATUSBAR,        COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_STATUSBAR_HL,     COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_STATUSBAR_WARN,   COLOR_YELLOW,  COLOR_BLUE);
        init_pair(CP_STATUSBAR_ERROR,  COLOR_RED,     COLOR_BLUE);
        init_pair(CP_STATUSBAR_INFO,   COLOR_CYAN,    COLOR_BLUE);
        init_pair(CP_STATUSBAR_GREEN,  COLOR_GREEN,   COLOR_BLUE);
        init_pair(CP_NOTIFICATION,     COLOR_BLACK,   COLOR_YELLOW);
        init_pair(CP_OVERLAY_BG,       COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_OVERLAY_HEADING,  COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_OVERLAY_HL,       COLOR_BLUE,    COLOR_WHITE);
        init_pair(CP_OVERLAY_SEL,      COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_OVERLAY_TAB_ACTIVE,   COLOR_WHITE,  COLOR_BLUE);
        init_pair(CP_OVERLAY_TAB_INACTIVE, COLOR_BLACK,  COLOR_WHITE);
        init_pair(CP_OVERLAY_BORDER,   COLOR_BLUE,    COLOR_WHITE);
        init_pair(CP_DIALOG_BG,        COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_DIALOG_HL,        COLOR_WHITE,   COLOR_BLUE);
    } else {
        /* ── DARK THEME (Hermes Teal match) ────────────────────────────
         * Near-black background, muted teal accents matching #0c5c43.
         * Redefine COLOR_CYAN to muted teal if terminal supports it.
         */
        if (can_change_color()) {
            init_color(COLOR_CYAN, 47, 361, 263);
        }
        init_pair(CP_TITLEBAR,         COLOR_WHITE,   COLOR_CYAN);
        init_pair(CP_TITLEBAR_HL,      COLOR_WHITE,   COLOR_CYAN);
        init_pair(CP_TITLEBAR_WARN,    COLOR_WHITE,   COLOR_RED);
        init_pair(CP_SIDEBAR,          COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_SIDEBAR_SEL,      COLOR_WHITE,   COLOR_CYAN);
        init_pair(CP_SIDEBAR_HL,       COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_SIDEBAR_HEADING,  COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_SIDEBAR_PINNED,   COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_SIDEBAR_DIM,      COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_SIDEBAR_META,     COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_CHAT_BG,          COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_CHAT_USER,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_CHAT_ASSISTANT,   COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_CHAT_SYSTEM,      COLOR_MAGENTA, COLOR_BLACK);
        init_pair(CP_CHAT_TOOL,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_CHAT_ERROR,       COLOR_RED,     COLOR_BLACK);
        init_pair(CP_CHAT_DIM,         COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_CHAT_BOLD,        COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_CHAT_CODE,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_CHAT_LINK,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_COMPOSER,         COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_STATUSBAR,        COLOR_WHITE,   COLOR_CYAN);
        init_pair(CP_STATUSBAR_HL,     COLOR_WHITE,   COLOR_CYAN);
        init_pair(CP_STATUSBAR_WARN,   COLOR_YELLOW,  COLOR_CYAN);
        init_pair(CP_STATUSBAR_ERROR,  COLOR_RED,     COLOR_CYAN);
        init_pair(CP_STATUSBAR_INFO,   COLOR_WHITE,   COLOR_CYAN);
        init_pair(CP_STATUSBAR_GREEN,  COLOR_GREEN,   COLOR_CYAN);
        init_pair(CP_NOTIFICATION,     COLOR_BLACK,   COLOR_YELLOW);
        init_pair(CP_OVERLAY_BG,       COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_OVERLAY_HEADING,  COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_OVERLAY_HL,       COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_OVERLAY_SEL,      COLOR_BLACK,   COLOR_CYAN);
        init_pair(CP_OVERLAY_TAB_ACTIVE,   COLOR_BLACK,  COLOR_CYAN);
        init_pair(CP_OVERLAY_TAB_INACTIVE, COLOR_WHITE,  COLOR_BLACK);
        init_pair(CP_OVERLAY_BORDER,   COLOR_CYAN,    COLOR_BLACK);
        init_pair(CP_DIALOG_BG,        COLOR_WHITE,   COLOR_BLACK);
        init_pair(CP_DIALOG_HL,        COLOR_BLACK,   COLOR_CYAN);
    }
}

void draw_bar(WINDOW *win, int cp, int cols, const char *left,
                     const char *center, const char *right) {
    wbkgd(win, COLOR_PAIR(cp));
    werase(win);
    wattron(win, COLOR_PAIR(cp) | A_BOLD);
    if (left)   mvwprintw(win, 0, 0, "%s", left);
    if (center) {
        int cl = (int)strlen(center);
        int cx = (cols - cl) / 2;
        if (cx < 0) cx = 0;
        mvwprintw(win, 0, cx, "%s", center);
    }
    if (right) {
        int rl = (int)strlen(right);
        int rx = cols - rl - 1;
        if (rx < 0) rx = 0;
        mvwprintw(win, 0, rx, "%s", right);
    }
    wattroff(win, COLOR_PAIR(cp) | A_BOLD);
    wnoutrefresh(win);
}

void draw_section_header(WINDOW *win, int y, int w, const char *label) {
    wattron(win, COLOR_PAIR(CP_SIDEBAR_HEADING) | A_BOLD);
    mvwprintw(win, y, 0, " %s", label);
    wattroff(win, COLOR_PAIR(CP_SIDEBAR_HEADING) | A_BOLD);
    int llen = (int)strlen(label) + 2;
    if (llen < w - 1) {
        whline(win, ACS_HLINE, w - llen - 1);
    }
}

void calc_layout(void) {
    getmaxyx(stdscr, ui.rows, ui.cols);
    ui.sidebar_width = ui.cols < 100 ? 22 : APP_SIDEBAR_WIDTH;
    ui.terminal_height = ui.rows < 40 ? 8 : APP_TERMINAL_HEIGHT;
    ui.chat_top = APP_TITLEBAR_HEIGHT;
    ui.chat_rows = ui.rows - APP_TITLEBAR_HEIGHT - APP_STATUSBAR_HEIGHT;
    ui.chat_cols = ui.cols;
    if (ui.sidebar_visible) ui.chat_cols -= ui.sidebar_width;
    if (ui.terminal_visible) ui.chat_rows -= ui.terminal_height;
    if (ui.chat_rows < 10) ui.chat_rows = 10;
    ui.composer_y = APP_TITLEBAR_HEIGHT + ui.chat_rows;
}
