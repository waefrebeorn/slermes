/* app_desktop_internals.h -- cross-module desktop UI and PTY helpers
 * extracted from src/app_desktop.c. Every moved ui/term helper function and the
 * shared ncurses panel state (ui) plus controller state (app) live here.
 * app_desktop.c (the faithful PoP controller port) and each extracted module
 * include this header.
 */

#ifndef APP_DESKTOP_INTERNALS_H
#define APP_DESKTOP_INTERNALS_H

#include "desktop_app.h"
#include "app_desktop.h"
#include "chat_render.h"
#include "chat_composer.h"
#include "slermes_pty.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <ncurses.h>
#include <panel.h>

/* ── PANEL IDs ─────────────────────────────────────────────────────── */
typedef enum {
    PANEL_TITLEBAR = 0,
    PANEL_SIDEBAR,
    PANEL_CHAT,
    PANEL_TERMINAL,
    PANEL_STATUSBAR,
    PANEL_OVERLAY,
    PANEL_DIALOG,
    PANEL_COUNT,
} panel_id_t;

/* ── COLOUR PAIRS ─────────────────────────────────────────────────── */
/* Accessible, high-contrast colour schemes (WCAG AA friendly).
 * Dark theme: dark backgrounds, bright text — for dim environments.
 * Light theme: light backgrounds, dark text — for bright environments.
 * No -1 backgrounds (always explicit) to avoid terminal default bleed.
 * Never use red-green as sole differentiators — use symbols + bold too.
 */
enum {
    CP_DEFAULT = 0,
    /* Titlebar: solid bg, high-contrast fg */
    CP_TITLEBAR,         /* bright fg on dark accent bg */
    CP_TITLEBAR_HL,      /* highlight fg on accent bg (warning/connected) */
    CP_TITLEBAR_WARN,    /* error fg on accent bg */
    /* Sidebar: distinct from main content area */
    CP_SIDEBAR,          /* default text on sidebar bg */
    CP_SIDEBAR_SEL,      /* selected item (reverse) */
    CP_SIDEBAR_HL,       /* hover/highlight accent */
    CP_SIDEBAR_HEADING,  /* section header accent */
    CP_SIDEBAR_PINNED,   /* special item accent */
    CP_SIDEBAR_DIM,      /* muted/secondary text */
    CP_SIDEBAR_META,     /* metadata (dates, counts) */
    /* Chat: main content area */
    CP_CHAT_BG,          /* base text on chat bg */
    CP_CHAT_USER,        /* user role label */
    CP_CHAT_ASSISTANT,   /* assistant role label */
    CP_CHAT_SYSTEM,      /* system role label */
    CP_CHAT_TOOL,        /* tool call label */
    CP_CHAT_ERROR,       /* error text */
    CP_CHAT_DIM,         /* secondary/dim text */
    CP_CHAT_BOLD,        /* emphasized text */
    CP_CHAT_CODE,        /* inline code */
    CP_CHAT_LINK,        /* link/hyperlink */
    /* Composer input area */
    CP_COMPOSER,         /* input text on input bg */
    /* Statusbar: solid bar across bottom */
    CP_STATUSBAR,        /* default text on statusbar bg */
    CP_STATUSBAR_HL,     /* highlight on statusbar bg */
    CP_STATUSBAR_WARN,   /* warning on statusbar bg */
    CP_STATUSBAR_ERROR,  /* error on statusbar bg */
    CP_STATUSBAR_INFO,   /* info on statusbar bg */
    CP_STATUSBAR_GREEN,  /* success on statusbar bg */
    /* Notification toast */
    CP_NOTIFICATION,     /* text on notification bg */
    /* Overlay dialogs */
    CP_OVERLAY_BG,       /* base text on overlay bg */
    CP_OVERLAY_HEADING,  /* overlay title */
    CP_OVERLAY_HL,       /* overlay highlight item */
    CP_OVERLAY_SEL,      /* overlay selected item */
    CP_OVERLAY_TAB_ACTIVE,   /* active tab */
    CP_OVERLAY_TAB_INACTIVE, /* inactive tab */
    CP_OVERLAY_BORDER,   /* overlay border */
    CP_DIALOG_BG,        /* dialog base text */
    CP_DIALOG_HL,        /* dialog highlight */
    CP_COUNT,
};

/* ── UI State (was anonymous static struct in app_desktop.c) ────────── */
typedef struct {
    WINDOW *wins[PANEL_COUNT];
    PANEL  *panels[PANEL_COUNT];
    int rows, cols;
    int sidebar_width;
    int terminal_height;
    int chat_top, chat_rows, chat_cols;
    int composer_y;
    bool sidebar_visible;
    bool terminal_visible;
    chat_rendered_msg_t **rendered_msgs;
    int rendered_count, rendered_capacity;
    int scroll_offset;
    composer_t *composer;
    bool model_picker_active;
    bool keyboard_shortcuts;
    char sidebar_search[128];
    int sidebar_search_len;
    bool sidebar_search_active;
    bool dirty;
} ui_state_t;

/* Shared state: defined in app_desktop.c, referenced by extracted modules. */
extern ui_state_t ui;
extern app_desktop_state_t app;

/* ── Layout helpers ─────────────────────────────────────────────────── */
void init_colors_for_theme(desktop_theme_t theme);
void draw_bar(WINDOW *win, int cp, int cols, const char *left, const char *center, const char *right);
void draw_section_header(WINDOW *win, int y, int w, const char *label);
void calc_layout(void);

/* ── UI lifecycle ───────────────────────────────────────────────────── */
void ui_init(void);
void ui_shutdown(void);
void ui_resize(void);
void ui_create_overlay(int ov_h, int ov_w);
void ui_destroy_overlay(void);
void ui_destroy_dialog(void);
void ui_create_dialog(int d_h, int d_w);
void ui_refresh_panels(void);

/* ── UI chrome draw ─────────────────────────────────────────────────── */
void ui_draw_titlebar(void);
void ui_draw_sidebar(void);
void ui_draw_chat(void);
void ui_draw_terminal(void);
void ui_draw_statusbar(void);
void ui_draw_all(void);

/* ── UI overlays draw ───────────────────────────────────────────────── */
void ui_draw_settings_tabs(WINDOW *win, int y, int w);
void ui_draw_settings_overlay(void);
void ui_draw_command_palette(void);
void ui_draw_model_picker(void);
void ui_draw_keyboard_shortcuts(void);
void ui_draw_dialog(void);
void ui_draw_overlay(void);

/* ── Input handlers ─────────────────────────────────────────────────── */
void filter_palette_commands(void);
void execute_palette_command(const char *action);
void ui_handle_dialog(int key);
void ui_handle_overlay(int key);
void ui_handle_normal(int key);

/* ── PTY ────────────────────────────────────────────────────────────── */
void term_launch_pty(void);
void term_read_pty(void);
void term_shutdown_pty(void);
void term_write_pty(const char *data, int len);

#endif /* APP_DESKTOP_INTERNALS_H */
