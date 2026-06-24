/*
 * app_desktop.h — C11 Desktop App for Slermes Agent (Enhanced)
 *
 * Cross-platform desktop application written in C11.
 * Replaces the Electron/TypeScript desktop app (apps/desktop/src/, 446 TS files).
 *
 * Architecture:
 *   - ncurses for terminal UI (panels, sidebar, chat, terminal, statusbar)
 *   - chat_render.c for markdown/syntax-highlighted message display
 *   - chat_composer.c for message input with slash commands
 *   - terminal.c + tty.c for embedded terminal panel
 *   - session management via desktop_app_common.c
 *   - settings via JSON persistence
 *   - notifications via status bar overlay
 *
 * Layout (ncurses):
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │ Titlebar (model, provider, conn status, tools)               │
 *   ├──────────┬───────────────────────────────────┬───────────────┤
 *   │          │                                   │               │
 *   │  Sidebar │     Chat Panel / Messages         │   Terminal    │
 *   │          │     (rendered via chat_render)     │   Panel PTY   │
 *   │ Sessions │                                   │               │
 *   │ Settings │     Composer (chat_composer)       │               │
 *   │ Profiles │                                   │               │
 *   │ Skills   │                                   │               │
 *   │ Artifacts│                                   │               │
 *   │ Cron     │                                   │               │
 *   │ Agents   │                                   │               │
 *   │ Messaging│                                   │               │
 *   ├──────────┴───────────────────────────────────┴───────────────┤
 *   │ Statusbar (gateway, model, tokens, tasks, subagents, yolo)   │
 *   └──────────────────────────────────────────────────────────────┘
 */

#ifndef APP_DESKTOP_H
#define APP_DESKTOP_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "desktop_app.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ─────────────────────────────────────────────────────── */
#define APP_MAX_TITLE       256
#define APP_MAX_SESSIONS    1024
#define APP_MAX_MESSAGES    4096
#define APP_MAX_NAV_ITEMS   32
#define APP_MAX_NOTIFICATIONS 10
#define APP_SIDEBAR_WIDTH   30
#define APP_TERMINAL_HEIGHT 12
#define APP_TITLEBAR_HEIGHT 1
#define APP_STATUSBAR_HEIGHT 1
#define APP_COMPOSER_HEIGHT 5
#define APP_SETTINGS_TABS   8
#define APP_MAX_PALETTE_CMDS 64
#define APP_MAX_PROFILES    32

/* ── Navigation / View IDs ────────────────────────────────────────── */
typedef enum {
    VIEW_CHAT = 0,
    VIEW_SETTINGS,
    VIEW_COMMAND_CENTER,
    VIEW_SKILLS,
    VIEW_ARTIFACTS,
    VIEW_CRON,
    VIEW_PROFILES,
    VIEW_AGENTS,
    VIEW_MESSAGING,
    VIEW_COUNT,
} app_view_t;

static const char * const APP_VIEW_NAMES[VIEW_COUNT] = {
    "chat", "settings", "command-center", "skills",
    "artifacts", "cron", "profiles", "agents", "messaging",
};

static const char * const APP_VIEW_LABELS[VIEW_COUNT] = {
    "Chat", "Settings", "Command Center", "Skills",
    "Artifacts", "Cron", "Profiles", "Agents", "Messaging",
};

static const char * const APP_VIEW_ICONS[VIEW_COUNT] = {
    "\xe2\x97\x8b",  /* ○ Chat */
    "\xe2\x99\xaf",  /* ♯ Settings */
    "\xe2\x96\xb6",  /* ▶ Command Center */
    "\xe2\x9c\xa6",  /* ✦ Skills */
    "\xe2\x9d\x90",  /* ❐ Artifacts */
    "\xe2\x8c\x9a",  /* ⌚ Cron */
    "\xe2\x99\xa0",  /* ♠ Profiles */
    "\xe2\x99\x9f",  /* ♟ Agents */
    "\xe2\x87\x84",  /* ⇄ Messaging */
};

/* ── Connection state ──────────────────────────────────────────────── */
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_ERROR,
    CONN_REAUTH_REQUIRED,
} conn_state_t;

/* ── Settings tab IDs ──────────────────────────────────────────────── */
typedef enum {
    SETTINGS_TAB_MODEL = 0,
    SETTINGS_TAB_PROVIDERS,
    SETTINGS_TAB_GATEWAY,
    SETTINGS_TAB_NOTIFICATIONS,
    SETTINGS_TAB_PROFILES,
    SETTINGS_TAB_THEME,
    SETTINGS_TAB_KEYS,
    SETTINGS_TAB_ABOUT,
} settings_tab_t;

static const char * const SETTINGS_TAB_LABELS[APP_SETTINGS_TABS] = {
    "Model", "Providers", "Gateway", "Notifications",
    "Profiles", "Theme", "API Keys", "About",
};

static const char * const SETTINGS_TAB_ICONS[APP_SETTINGS_TABS] = {
    "\xe2\x9c\xb5",  /* ✵ Model */
    "\xe2\x9a\xa1",  /* ⚡ Providers */
    "\xe2\x87\x84",  /* ⇄ Gateway */
    "\xe2\x9f\xae",  /* 🔔 Notifications */
    "\xe2\x99\xa0",  /* ♠ Profiles */
    "\xe2\x9c\xa8",  /* ✨ Theme */
    "\xe2\x94\xbf",  /* ❋ Keys */
    "\xe2\x93\x98",  /* ⓘ About */
};

/* ── Theme ──────────────────────────────────────────────────────────── */
static const char * const THEME_NAMES[3] = {
    "System", "Dark", "Light",
};

/* ── Notification entry ───────────────────────────────────────────── */
typedef struct {
    char message[256];
    time_t timestamp;
    int duration_sec;
    bool urgent;
} desktop_notif_t;

/* ── Background task info ──────────────────────────────────────────── */
typedef struct {
    char id[64];
    char label[128];
    bool running;
    bool has_error;
    int progress_pct;
} desktop_bg_task_t;

#define APP_MAX_BG_TASKS 8

/* ── Subagent info ────────────────────────────────────────────────── */
typedef struct {
    char id[64];
    char session_id[64];
    char task[128];
    bool running;
} desktop_subagent_t;

#define APP_MAX_SUBAGENTS 8

/* ── Gateway detail ────────────────────────────────────────────────── */
typedef struct {
    char state[32];
    char url[256];
    char profile[64];
    bool inference_ready;
    int active_sessions;
    int messages_today;
    int restart_count;
} desktop_gateway_detail_t;

/* ── Desktop app state ────────────────────────────────────────────── */
typedef struct app_desktop_state {
    /* Window */
    int rows, cols;
    bool running;
    bool needs_redraw;

    /* Navigation */
    app_view_t active_view;
    bool sidebar_visible;
    bool terminal_visible;
    bool settings_overlay;
    bool command_palette;
    app_view_t overlay_view;
    settings_tab_t settings_tab;

    /* Sidebar selection */
    int sidebar_section;        /* 0=sessions, 1=nav */
    int sidebar_sel;            /* selected index within section */
    int session_sel;            /* selected session index */
    int session_count;
    char session_ids[APP_MAX_SESSIONS][64];
    char session_titles[APP_MAX_SESSIONS][256];

    /* Connection */
    conn_state_t conn_state;
    char model[128];
    char provider[64];
    char gateway_url[256];
    char current_profile[64];
    bool yolo_active;
    bool update_available;
    char update_version[32];

    /* Tokens / usage */
    int tokens_in;
    int tokens_out;
    int iteration;
    int max_iterations;
    int context_usage_pct;

    /* Notifications stack */
    desktop_notif_t notifications[APP_MAX_NOTIFICATIONS];
    int notif_count;

    /* Background tasks */
    desktop_bg_task_t bg_tasks[APP_MAX_BG_TASKS];
    int bg_task_count;

    /* Subagents */
    desktop_subagent_t subagents[APP_MAX_SUBAGENTS];
    int subagent_count;

    /* Gateway detail */
    desktop_gateway_detail_t gateway;

    /* Theme */
    desktop_theme_t theme;

    /* Profiles */
    int profile_count;
    char profile_names[APP_MAX_PROFILES][128];
    int active_profile;

    /* Models */
    int model_count;
    char model_names[APP_MAX_SESSIONS][128];  /* max = session max is generous */

    /* Command palette query */
    char palette_query[256];
    int palette_query_len;
    int palette_sel;
    int palette_cmd_count;
    int palette_filtered_count;
    int palette_filtered_indices[APP_MAX_PALETTE_CMDS];
    char palette_labels[APP_MAX_PALETTE_CMDS][64];
    char palette_actions[APP_MAX_PALETTE_CMDS][64];

    /* Session operations */
    char rename_buf[256];
    int rename_len;
    bool rename_active;
    bool delete_confirm;
    char confirm_session_id[64];
    bool create_new_session;

    /* Notifications */
    char notification[256];
    time_t notification_time;
    int notification_duration_sec;
} app_desktop_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────── */

/* PoP: DesktopController @ apps/desktop/src/app/desktop-controller.tsx */
int app_desktop_run(int argc, char **argv);
void app_desktop_shutdown(void);

/* PoP: app init @ apps/desktop/src/app/desktop-controller.tsx */
bool app_desktop_init(void);

/* ── Rendering ──────────────────────────────────────────────────────── */

/* PoP: renderShell @ apps/desktop/src/app/shell/app-shell.tsx */
void app_desktop_draw(app_desktop_state_t *app);

/* PoP: titlebar @ apps/desktop/src/app/shell/titlebar.ts */
void app_desktop_draw_titlebar(app_desktop_state_t *app, int cols);

/* PoP: sidebar items → labels map @ apps/desktop/src/app/shell/sidebar-label.tsx */
void app_desktop_draw_sidebar(app_desktop_state_t *app, int rows, int cols);

/* PoP: ChatView @ apps/desktop/src/app/chat/index.tsx */
void app_desktop_draw_chat(app_desktop_state_t *app, int rows, int cols);

/* PoP: StatusbarControls @ apps/desktop/src/app/shell/statusbar-controls.tsx */
void app_desktop_draw_statusbar(app_desktop_state_t *app, int cols);

/* ── Input ──────────────────────────────────────────────────────────── */

/* PoP: keybinds @ apps/desktop/src/app/shell/keybind-panel.tsx */
void app_desktop_handle_input(app_desktop_state_t *app, int key);

/* PoP: command palette @ apps/desktop/src/app/command-palette/ */
void app_desktop_toggle_command_palette(app_desktop_state_t *app);

/* ── Settings overlay ───────────────────────────────────────────────── */

/* PoP: SettingsView @ apps/desktop/src/app/settings/ */
void app_desktop_toggle_settings(app_desktop_state_t *app);

/* ── Notifications ──────────────────────────────────────────────────── */
void app_desktop_notify(app_desktop_state_t *app, const char *msg, int duration_sec);
void app_desktop_push_notif(app_desktop_state_t *app, const char *msg, int duration_sec, bool urgent);

/* ── Model picker ───────────────────────────────────────────────────── */
void app_desktop_toggle_model_picker(app_desktop_state_t *app);

/* ── Session operations ────────────────────────────────────────────── */
void app_desktop_create_session(app_desktop_state_t *app);
void app_desktop_delete_session(app_desktop_state_t *app, const char *id);
void app_desktop_rename_session_open(app_desktop_state_t *app);
void app_desktop_pin_session(app_desktop_state_t *app, const char *id);

/* ── Theme ──────────────────────────────────────────────────────────── */
void app_desktop_set_theme(app_desktop_state_t *app, desktop_theme_t theme);

#ifdef __cplusplus
}
#endif

#endif /* APP_DESKTOP_H */
