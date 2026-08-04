/*
 * app_desktop.c — C11 Desktop App for Slermes Agent (v472 Polished)
 *
 * Full ncurses desktop application replacing the Electron/TypeScript shell.
 * Polished to match Hermes web dashboard visual quality.
 *
 * Features:
 *   - Dark theme with precise color scheme matching web dashboard
 *   - Titlebar with model, provider, connection state, YOLO indicator
 *   - Sidebar with session list (showing metadata), +New Chat, nav items
 *   - Chat panel with rendered messages, role labels, model pill
 *   - Terminal panel with PTY placeholder
 *   - Statusbar with gateway state, model, tokens, context bar, tasks
 *   - Settings overlay (8 tabs: Model, Providers, Gateway, Notifications,
 *     Profiles, Theme, API Keys, About)
 *   - Command palette (25 categorized commands with live search)
 *   - Model picker overlay
 *   - Keyboard shortcut overlay (F1/?)
 *   - Notification stack
 *   - Theme switching (dark/light/system)
 *   - Session create/delete/rename with confirmation dialogs
 *   - Sidebar search/filter for sessions
 *   - Working navigation views (8 pages with different content)
 */

#define _GNU_SOURCE
#include "desktop_app.h"
#include "app_desktop.h"
#include "chat_render.h"
#include "chat_composer.h"

#include <ncurses.h>
#include <panel.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include "slermes_pty.h"
#include "clipboard.h"

#include "app_desktop_internals.h"

/* Shared state definitions (types declared in app_desktop_internals.h) */
ui_state_t ui;
app_desktop_state_t app;

void app_desktop_handle_input(app_desktop_state_t *app_state, int key) {
    (void)app_state;
    if (app.delete_confirm || app.rename_active) {
        ui_handle_dialog(key);
    } else if (app.command_palette || app.settings_overlay || ui.model_picker_active || ui.keyboard_shortcuts) {
        ui_handle_overlay(key);
    } else {
        ui_handle_normal(key);
    }
    ui.dirty = true;
}

bool app_desktop_init(void) {
    ui_init();

    /* Add demo messages */
    chat_rendered_msg_t *m1 = chat_render_message(
        "Welcome to **Slermes Desktop** \xe2\x80\x94 C11 Edition\n"
        "*Full parity with the Electron/TypeScript shell*", "system");
    if (m1) ui.rendered_msgs[ui.rendered_count++] = m1;

    chat_rendered_msg_t *m2 = chat_render_message(
        "This native C11 desktop app has been polished to match\n"
        "the Hermes web dashboard visual quality:\n\n"
        "  \xe2\x80\xa2 Dark theme with precise color scheme\n"
        "  \xe2\x80\xa2 Sidebar with session metadata & search\n"
        "  \xe2\x80\xa2 Multi-tab settings (8 tabs)\n"
        "  \xe2\x80\xa2 Full command palette (26 commands)\n"
        "  \xe2\x80\xa2 Enhanced statusbar (gateway, tasks, YOLO)\n"
        "  \xe2\x80\xa2 Model picker, theme switching, keyboard help\n"
        "  \xe2\x80\xa2 Session create/delete/rename with dialogs", "assistant");
    if (m2) ui.rendered_msgs[ui.rendered_count++] = m2;

    chat_rendered_msg_t *m3 = chat_render_message(
        "```c\n"
        "/* Slermes C11 Desktop — Parity with Hermes Electron */\n"
        "// All 446 TS files → one C11 binary\n"
        "#define POLISH_LEVEL (DARK_THEME | META_SIDEBAR | SEARCH \\\n"
        "    | SHORTCUTS | NAV_VIEWS | NOTIFS | TASKS)\n"
        "struct desktop { int rows, cols, dirty; };\n"
        "int main() { return app_desktop_run(0, NULL); }\n"
        "```", "assistant");
    if (m3) ui.rendered_msgs[ui.rendered_count++] = m3;

    chat_rendered_msg_t *m4 = chat_render_message(
        "Keybinds: `s` Settings | `p` Model Picker | `:` Command Palette\\n"
        "`Tab` Sidebar | `t` Terminal | `n` New Chat | `r` Rename | `d` Delete\\n"
        "`y` Copy | `i` Paste | `F1` or `?` for keyboard shortcuts | `/` for sidebar search\\n"
        "Arrow keys navigate sidebar. `q` quits (or type into composer).", "system");
    if (m4) ui.rendered_msgs[ui.rendered_count++] = m4;

    /* Terminal PTY init */
    app.term_pty = NULL;
    app.term_buf_len = 0;
    app.term_scroll = 0;

    return true;
}

void app_desktop_shutdown(void) { term_shutdown_pty(); ui_shutdown(); }

void app_desktop_draw(app_desktop_state_t *app_state) {
    (void)app_state;
    if (ui.dirty) ui_draw_all();
}

void app_desktop_draw_titlebar(app_desktop_state_t *app_state, int cols) {
    (void)app_state; (void)cols; ui_draw_titlebar();
}

void app_desktop_draw_sidebar(app_desktop_state_t *app_state, int rows, int cols) {
    (void)app_state; (void)rows; (void)cols; ui_draw_sidebar();
}

void app_desktop_draw_chat(app_desktop_state_t *app_state, int rows, int cols) {
    (void)app_state; (void)rows; (void)cols; ui_draw_chat();
}

void app_desktop_draw_statusbar(app_desktop_state_t *app_state, int cols) {
    (void)app_state; (void)cols; ui_draw_statusbar();
}

void app_desktop_toggle_settings(app_desktop_state_t *app_state) {
    (void)app_state;
    if (app.settings_overlay && ui.wins[PANEL_OVERLAY]) {
        app.settings_overlay = false;
        ui_destroy_overlay();
    } else {
        app.settings_overlay = true;
        app.command_palette = false;
        ui.model_picker_active = false;
        ui.keyboard_shortcuts = false;
        ui_create_overlay(ui.rows - 2, ui.cols - 6);
    }
    ui.dirty = true;
}

/* PoP: _palette @ hermes_cli/journey.py:_palette */
void app_desktop_toggle_command_palette(app_desktop_state_t *app_state) {
    (void)app_state;
    if (app.command_palette) { app.command_palette = false; ui_destroy_overlay(); }
    else {
        app.command_palette = true;
        app.palette_query[0] = '\0';
        app.palette_query_len = 0;
        app.palette_sel = 0;
        filter_palette_commands();
        ui_create_overlay(ui.rows - 2, 52);
    }
    ui.dirty = true;
}

void app_desktop_toggle_model_picker(app_desktop_state_t *app_state) {
    (void)app_state;
    if (ui.model_picker_active) { ui.model_picker_active = false; ui_destroy_overlay(); }
    else { ui.model_picker_active = true; app.palette_sel = 0; ui_create_overlay(ui.rows - 4, 40); }
    ui.dirty = true;
}

void app_desktop_notify(app_desktop_state_t *app_state, const char *msg, int duration_sec) {
    strncpy(app_state->notification, msg, sizeof(app_state->notification) - 1);
    app_state->notification[sizeof(app_state->notification) - 1] = '\0';
    app_state->notification_time = time(NULL);
    app_state->notification_duration_sec = duration_sec;
    app_desktop_push_notif(app_state, msg, duration_sec, false);
    ui.dirty = true;
}

void app_desktop_push_notif(app_desktop_state_t *app_state, const char *msg, int duration_sec, bool urgent) {
    if (app_state->notif_count >= APP_MAX_NOTIFICATIONS) {
        for (int i = 1; i < app_state->notif_count; i++)
            app_state->notifications[i - 1] = app_state->notifications[i];
        app_state->notif_count--;
    }
    int idx = app_state->notif_count++;
    strncpy(app_state->notifications[idx].message, msg, sizeof(app_state->notifications[idx].message) - 1);
    app_state->notifications[idx].message[sizeof(app_state->notifications[idx].message) - 1] = '\0';
    app_state->notifications[idx].timestamp = time(NULL);
    app_state->notifications[idx].duration_sec = duration_sec;
    app_state->notifications[idx].urgent = urgent;
    ui.dirty = true;
}

void app_desktop_create_session(app_desktop_state_t *app_state) {
    if (app_state->session_count >= APP_MAX_SESSIONS) return;
    int idx = app_state->session_count;
    snprintf(app_state->session_ids[idx], 63, "session_%ld", (long)time(NULL));
    snprintf(app_state->session_titles[idx], 255, "Chat %d", idx + 1);
    app_state->session_count++;
    app_state->session_sel = idx;
    app_state->sidebar_section = 0;
    ui.rendered_count = 0;
    app_desktop_notify(app_state, "New session created", 2);
    ui.dirty = true;
}

void app_desktop_delete_session(app_desktop_state_t *app_state, const char *id) {
    if (app_state->session_count <= 0) return;
    strncpy(app_state->confirm_session_id, id, sizeof(app_state->confirm_session_id) - 1);
    app_state->delete_confirm = true;
    ui_create_dialog(7, 40);
    ui.dirty = true;
}

void app_desktop_rename_session_open(app_desktop_state_t *app_state) {
    if (app_state->session_count <= 0 || app_state->session_sel < 0 ||
        app_state->session_sel >= app_state->session_count) return;
    strncpy(app_state->rename_buf, app_state->session_titles[app_state->session_sel],
            sizeof(app_state->rename_buf) - 1);
    app_state->rename_buf[sizeof(app_state->rename_buf) - 1] = '\0';
    app_state->rename_len = (int)strlen(app_state->rename_buf);
    app_state->rename_active = true;
    ui_create_dialog(7, 50);
    ui.dirty = true;
}

void app_desktop_pin_session(app_desktop_state_t *app_state, const char *id) {
    (void)app_state; (void)id;
}

void app_desktop_set_theme(app_desktop_state_t *app_state, desktop_theme_t theme) {
    app_state->theme = theme;
    char notif[128];
    snprintf(notif, sizeof(notif), "Theme: %s", THEME_NAMES[theme]);
    app_desktop_notify(app_state, notif, 2);
    ui.dirty = true;
}

int app_desktop_run(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!app_desktop_init()) { fprintf(stderr, "Failed to initialize desktop app\n"); return 1; }
    ui_draw_all();
    while (app.running) {
        /* Read PTY output if terminal is active */
        term_read_pty();
        int key = getch();
        if (key != ERR) app_desktop_handle_input(&app, key);
        if (ui.dirty) ui_draw_all();
        usleep(10000);
    }
    app_desktop_shutdown();
    return 0;
}
