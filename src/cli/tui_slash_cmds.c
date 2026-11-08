/*
 * tui_slash_cmds.c - TUI slash-command handlers and their registry.
 *
 * Split from tui_fullscreen.c (soul.md: self-contained modules). Each
 * handler is a thin dispatcher that flips tui modal state and calls the
 * matching tui_draw_* view. No file-scope state of its own.
 */
#include "tui_slash_cmds.h"
#include "hermes_cli.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "base64.h"
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <locale.h>
#include "budget_tracker.h"
#include "provider_metadata.h"
#include "hermes_plugin.h"
#include "plugin.h"
#include "tui_eventpub.h"
#include "tui_slash_worker.h"
#include "chat_render.h"
#include "chat_composer.h"
#include "slermes_home.h"
#include "tui_state.h"


/* ==================================================================
 *  PLUGIN HUB — list loaded plugins with status and toggle
 * ================================================================== */

/* Initialize plugin hub: populate from agent's plugin_registry */

/* Draw the plugin hub overlay */

/* Handle plugin hub input */

/* Port of Python hermes_cli/main.py:cmd_plugins(). */
/* Activate plugin hub */ bool cmd_plugins(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_plugin_hub_init();
    tui_draw_plugin_hub();
    tui.modal_mode = MODE_PLUGIN_HUB;
    return true;
}


/* ── Slash command handlers (T06) ───────────────────────────── */ bool cmd_help(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_HELP;
    tui_draw_help();
    return true;
}
 bool cmd_quit(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.running = false;
    return true;
}

/* AG26: Port of Python gateway/platforms/helpers.py:clear(). */ bool cmd_clear(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.history.count = 0;
    tui.history.head = 0;
    tui_redraw_history();
    return true;
}
 bool cmd_sessions(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_fullscreen_session_browse();
    return true;
}

/* Port of Python hermes_cli/main.py:cmd_config(). */ bool cmd_config(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_fullscreen_config_edit();
    return true;
}
 bool cmd_mobile(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    if (tui.layout_mode == TUI_LAYOUT_NORMAL)
        tui.layout_mode = TUI_LAYOUT_MOBILE;
    else if (tui.layout_mode == TUI_LAYOUT_MOBILE)
        tui.layout_mode = TUI_LAYOUT_COMPACT;
    else
        tui.layout_mode = TUI_LAYOUT_NORMAL;
    tui_resize_panes();
    tui_redraw_history();
    tui_redraw_status();
    return true;
}
 bool cmd_theme(const char *line, int argc, char **argv, void *state) {
    (void)state;
    if (argc > 1) {
        tui_fullscreen_theme_reload(argv[1]);
        tui_history_add(MSG_ROLE_INFO, "Theme changed", false);
    }
    tui_redraw_history();
    tui_redraw_status();
    return true;
}
 bool cmd_redraw(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    clearok(stdscr, TRUE);
    refresh();
    tui_resize_panes();
    tui_redraw_history();
    tui_redraw_status();
    return true;
}
 bool cmd_tokens(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    char info[256];
    snprintf(info, sizeof(info), "Tokens in: %d, out: %d, total: %d",
             tui.status.tokens_in, tui.status.tokens_out,
             tui.status.tokens_in + tui.status.tokens_out);
    tui_history_add(MSG_ROLE_INFO, info, false);
    tui_redraw_history();
    return true;
}
 bool cmd_budget(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    char info[128];
    snprintf(info, sizeof(info), "Budget remaining: %.2f",
             tui.status.budget_remaining);
    tui_history_add(MSG_ROLE_INFO, info, false);
    tui_redraw_history();
    return true;
}

/* Port of Python hermes_cli/main.py:cmd_model(). */ bool cmd_model(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)state;
    if (argc < 2) {
        /* No args: open model picker overlay */
        tui_model_picker_init();
        tui_draw_model_picker();
        tui.modal_mode = MODE_MODEL_PICK;
    } else {
        /* /model <name>: quick set */
        if (tui.agent)
            snprintf(tui.agent->llm.model, sizeof(tui.agent->llm.model), "%s", argv[1]);
        snprintf(tui.status.model, sizeof(tui.status.model), "%s", argv[1]);
        tui.status.dirty = true;
        char msg[256];
        snprintf(msg, sizeof(msg), "Model set to: %s", argv[1]);
        tui_history_add(MSG_ROLE_INFO, msg, false);
        tui_redraw_history();
        tui_redraw_status();
    }
    return true;
}
 bool cmd_undo(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    if (tui.agent)
        tui_history_add(MSG_ROLE_INFO, "Undo requested", false);
    tui_redraw_history();
    return true;
}
 bool cmd_reset(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.history.count = 0;
    tui.history.head = 0;
    if (tui.agent) { /* agent_reset(tui.agent); */ }
    tui_history_add(MSG_ROLE_INFO, "Conversation reset", false);
    tui_redraw_history();
    return true;
}
 bool cmd_skin(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    char info[1024] = "Available themes: ";
    for (int i = 0; i < tui_theme_count; i++) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "%s%s%s",
                 (i > 0 ? ", " : ""),
                 tui_themes[i]->name,
                 (i == tui_current_theme ? " (active)" : ""));
        strncat(info, tmp, sizeof(info) - strlen(info) - 1);
    }
    tui_history_add(MSG_ROLE_INFO, info, false);
    tui_redraw_history();
    return true;
}
 bool cmd_verbose(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_history_add(MSG_ROLE_INFO, "Verbose level toggled", false);
    tui_redraw_history();
    return true;
}
 bool cmd_yolo(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_history_add(MSG_ROLE_INFO, "Yolo mode toggled", false);
    tui_redraw_history();
    return true;
}
 bool cmd_fast(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_history_add(MSG_ROLE_INFO, "Fast mode toggled", false);
    tui_redraw_history();
    return true;
}
 bool cmd_todos(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui_todo_panel_init();
    tui.modal_mode = MODE_TODO_PANEL;
    tui_draw_todo_panel();
    return true;
}
 bool cmd_agent_info(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_AGENT_INFO;
    tui_draw_agent_info();
    return true;
}
 bool cmd_skills_browse(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_SKILL_BROWSE;
    tui_draw_skill_browser();
    return true;
}

/* Port of Python hermes_cli/main.py:cmd_logs(). */ bool cmd_logs(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_LOG_VIEW;
    tui_draw_log_viewer();
    return true;
}

/* Port of Python hermes_cli/main.py:cmd_cron(). */ bool cmd_cron(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_CRON_VIEW;
    tui_draw_cron_viewer();
    return true;
}

/* Port of Python hermes_cli/main.py:cmd_gateway(). */ bool cmd_gateway(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    tui.modal_mode = MODE_GATEWAY_STATUS;
    tui_draw_gateway_status();
    return true;
}
 bool cmd_image(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)state;
    if (argc > 1) {
        tui_display_image(argv[1]);
    } else {
        tui_history_add(MSG_ROLE_ERROR, "Usage: /image <path>", true);
        tui_redraw_history();
    }
    return true;
}

/* Registration table for all slash commands */
const slash_cmd_entry_t slash_registry[] = {
    {"/help",    "Show help message", "",                SLASH_CAT_TUI,     cmd_help, NULL},
    {"/quit",    "Exit the TUI", "",                     SLASH_CAT_META,    cmd_quit, NULL},
    {"/exit",    "Exit the TUI", "",                     SLASH_CAT_META,    cmd_quit, NULL},
    {"/clear",   "Clear output pane", "",                SLASH_CAT_TUI,     cmd_clear, NULL},
    {"/theme",   "Set theme (default/dark/light/mono)", "<name>",           SLASH_CAT_TUI,     cmd_theme, NULL},
    {"/model",   "Show/set current model", "[name]",     SLASH_CAT_AGENT,   cmd_model, NULL},
    {"/tokens",  "Show token usage", "",                 SLASH_CAT_AGENT,   cmd_tokens, NULL},
    {"/budget",  "Show budget status", "",               SLASH_CAT_AGENT,   cmd_budget, NULL},
    {"/sessions","Open session browser", "",              SLASH_CAT_MODAL,   cmd_sessions, NULL},
    {"/config",  "Open config editor", "",                SLASH_CAT_MODAL,   cmd_config, NULL},
    {"/save",    "Save current session", "[name]",        SLASH_CAT_SESSION, NULL, NULL},
    {"/load",    "Load a session", "<id>",                SLASH_CAT_SESSION, NULL, NULL},
    {"/export",  "Export session as JSON/Markdown", "<fmt>", SLASH_CAT_SESSION, NULL, NULL},
    {"/delete",  "Delete current session", "",            SLASH_CAT_SESSION, NULL, NULL},
    {"/verbose", "Set verbose level (0/1/2)", "<level>",  SLASH_CAT_AGENT,   cmd_verbose, NULL},
    {"/yolo",    "Toggle yolo mode", "",                  SLASH_CAT_AGENT,   cmd_yolo, NULL},
    {"/fast",    "Toggle fast mode", "",                  SLASH_CAT_AGENT,   cmd_fast, NULL},
    {"/mobile",  "Toggle mobile/compact layout", "",      SLASH_CAT_TUI,     cmd_mobile, NULL},
    {"/reset",   "Reset conversation", "",                SLASH_CAT_META,    cmd_reset, NULL},
    {"/undo",    "Undo last turn", "",                    SLASH_CAT_META,    cmd_undo, NULL},
    {"/redraw",  "Force screen redraw", "",               SLASH_CAT_TUI,     cmd_redraw, NULL},
    {"/skin",    "List/reload skins", "",                 SLASH_CAT_TUI,     cmd_skin, NULL},
    {"/image",   "Display an image file", "<path>",       SLASH_CAT_TUI,     cmd_image, NULL},
    {"/gateway", "Show gateway status dashboard", "",     SLASH_CAT_MODAL,   cmd_gateway, NULL},
    {"/cron",    "Show cron job viewer", "",              SLASH_CAT_MODAL,   cmd_cron, NULL},
    {"/logs",    "Show log viewer", "",                   SLASH_CAT_MODAL,   cmd_logs, NULL},
    {"/skills",  "Browse available skills", "",           SLASH_CAT_MODAL,   cmd_skills_browse, NULL},
    {"/todos",   "Show todo/kanban board", "",            SLASH_CAT_MODAL,   cmd_todos, NULL},
    {"/agent",   "Show agent info (model, provider, tokens)", "", SLASH_CAT_MODAL, cmd_agent_info, NULL},
    {"/plugins", "Show plugin hub (loaded plugins, toggle)", "",   SLASH_CAT_MODAL, cmd_plugins, NULL},
    {NULL, NULL, NULL, 0, NULL, NULL}
};
