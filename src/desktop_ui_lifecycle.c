/* desktop_ui_lifecycle.c -- extracted from src/app_desktop.c (angel-coder monolith split).
 * Self-contained desktop UI/PTY concern module. See app_desktop_internals.h.
 */

#include "app_desktop_internals.h"

void ui_init(void) {
    memset(&ui, 0, sizeof(ui));
    memset(&app, 0, sizeof(app));

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(100);
    curs_set(0);
    init_colors_for_theme(app.theme);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(50);

    ui.sidebar_visible = true;
    ui.terminal_visible = true;
    ui.dirty = true;

    app.running = true;
    app.conn_state = CONN_CONNECTED;
    app.theme = THEME_DARK;
    app.settings_tab = SETTINGS_TAB_MODEL;
    app.sidebar_section = 0;
    app.iteration = 3;

    /* Probe gateway for real connection status */
    probe_result_t probe = gateway_probe_default("http://localhost:18789/health");
    if (probe.reachable) {
        app.conn_state = CONN_CONNECTED;
        snprintf(app.model, sizeof(app.model), "%s", probe.version);
    } else {
        app.conn_state = CONN_DISCONNECTED;
    }
    app.max_iterations = 42;
    app.yolo_active = false;
    app.tokens_in = 4520;
    app.tokens_out = 2180;
    app.context_usage_pct = 12;

    strncpy(app.model, "openrouter/owl-alpha", sizeof(app.model) - 1);
    strncpy(app.provider, "openrouter", sizeof(app.provider) - 1);
    strncpy(app.gateway_url, "http://localhost:18789", sizeof(app.gateway_url) - 1);
    strncpy(app.current_profile, "default", sizeof(app.current_profile) - 1);

    strncpy(app.gateway.state, "connected", sizeof(app.gateway.state) - 1);
    strncpy(app.gateway.url, "http://localhost:18789", sizeof(app.gateway.url) - 1);
    app.gateway.inference_ready = true;
    app.gateway.active_sessions = 1;
    app.gateway.messages_today = 47;

    /* Models */
    const char *default_models[] = {
        "claude-sonnet-4", "claude-opus-4", "gpt-4o", "gpt-4o-mini",
        "gemini-2.5-pro", "openrouter/owl-alpha", NULL
    };
    app.model_count = 0;
    for (int i = 0; default_models[i] && app.model_count < APP_MAX_SESSIONS; i++) {
        strncpy(app.model_names[app.model_count++], default_models[i], 127);
    }

    /* Profiles */
    const char *default_profiles[] = {"default", "dev", "research", "creative", NULL};
    app.profile_count = 0;
    for (int i = 0; default_profiles[i] && app.profile_count < APP_MAX_PROFILES; i++) {
        strncpy(app.profile_names[app.profile_count++], default_profiles[i], 127);
    }
    app.active_profile = 0;

    /* Demo sessions with metadata-like names */
    const struct { const char *id, *title; int msgs, last_active_hrs; } demos[] = {
        {"20260622_142300", "C Porting Blitz — Web/Desktop Parity",  42, 0},
        {"20260621_093000", "Desktop Parity Work — v470 Enhancements", 28, 3},
        {"20260620_161200", "Web Server API Schema Matching",         35, 8},
        {"20260619_110000", "Gateway WebSocket Integration",          18, 24},
        {"20260618_080000", "Session Persistence & CRUD",             12, 48},
        {"20260617_150000", "Build System / Makefile Audit",           7, 72},
    };
    app.session_count = sizeof(demos) / sizeof(demos[0]);
    for (int i = 0; i < app.session_count; i++) {
        strncpy(app.session_ids[i], demos[i].id, 63);
        snprintf(app.session_titles[i], 255, "%s", demos[i].title);
    }
    app.session_sel = 0;

    /* Background tasks */
    app.bg_task_count = 2;
    strncpy(app.bg_tasks[0].id, "task_1", 63);
    strncpy(app.bg_tasks[0].label, "parity-scan", 127);
    app.bg_tasks[0].running = true;
    strncpy(app.bg_tasks[1].id, "task_2", 63);
    strncpy(app.bg_tasks[1].label, "build-check", 127);
    app.bg_tasks[1].running = false;
    app.bg_tasks[1].has_error = false;

    /* Subagents */
    app.subagent_count = 1;
    strncpy(app.subagents[0].id, "sub_1", 63);
    strncpy(app.subagents[0].task, "audit-parity", 127);
    app.subagents[0].running = true;

    /* Update available */
    app.update_available = false;
    strncpy(app.update_version, "0.17.1", 31);

    calc_layout();

    /* Create windows */
    ui.wins[PANEL_TITLEBAR]  = newwin(APP_TITLEBAR_HEIGHT, ui.cols, 0, 0);
    ui.wins[PANEL_SIDEBAR]   = derwin(stdscr, ui.rows - APP_TITLEBAR_HEIGHT - APP_STATUSBAR_HEIGHT, ui.sidebar_width,
                                       APP_TITLEBAR_HEIGHT, 0);
    ui.wins[PANEL_CHAT]      = derwin(stdscr, ui.chat_rows, ui.chat_cols,
                                       APP_TITLEBAR_HEIGHT, ui.sidebar_visible ? ui.sidebar_width : 0);
    ui.wins[PANEL_TERMINAL]  = derwin(stdscr, ui.terminal_height, ui.chat_cols,
                                       APP_TITLEBAR_HEIGHT + ui.chat_rows, ui.sidebar_visible ? ui.sidebar_width : 0);
    ui.wins[PANEL_STATUSBAR] = newwin(APP_STATUSBAR_HEIGHT, ui.cols, ui.rows - 1, 0);
    ui.wins[PANEL_OVERLAY]   = NULL;
    ui.wins[PANEL_DIALOG]    = NULL;

    for (int i = 0; i < PANEL_COUNT; i++) {
        if (ui.wins[i]) ui.panels[i] = new_panel(ui.wins[i]);
    }

    ui.composer = composer_create();

    ui.rendered_capacity = APP_MAX_MESSAGES;
    ui.rendered_msgs = calloc(ui.rendered_capacity, sizeof(chat_rendered_msg_t*));
}

void ui_shutdown(void) {
    for (int i = 0; i < PANEL_COUNT; i++) {
        if (ui.panels[i]) del_panel(ui.panels[i]);
        if (ui.wins[i]) delwin(ui.wins[i]);
    }
    if (ui.composer) composer_dispose(ui.composer);
    for (int i = 0; i < ui.rendered_count; i++) {
        if (ui.rendered_msgs[i]) chat_render_free(ui.rendered_msgs[i]);
    }
    free(ui.rendered_msgs);
    endwin();
}

void ui_resize(void) {
    endwin();
    refresh();
    clear();
    calc_layout();
    for (int i = 0; i < PANEL_COUNT; i++) {
        if (ui.panels[i]) del_panel(ui.panels[i]);
        if (ui.wins[i]) delwin(ui.wins[i]);
    }

    ui.wins[PANEL_TITLEBAR]  = newwin(APP_TITLEBAR_HEIGHT, ui.cols, 0, 0);
    int sb_h = ui.rows - APP_TITLEBAR_HEIGHT - APP_STATUSBAR_HEIGHT;
    int chat_h = ui.terminal_visible ? ui.chat_rows : ui.rows - APP_TITLEBAR_HEIGHT - APP_STATUSBAR_HEIGHT;
    if (ui.sidebar_visible) {
        ui.wins[PANEL_SIDEBAR] = derwin(stdscr, sb_h, ui.sidebar_width, APP_TITLEBAR_HEIGHT, 0);
        ui.wins[PANEL_CHAT] = derwin(stdscr, chat_h, ui.chat_cols, APP_TITLEBAR_HEIGHT, ui.sidebar_width);
        if (ui.terminal_visible)
            ui.wins[PANEL_TERMINAL] = derwin(stdscr, ui.terminal_height, ui.chat_cols,
                                             APP_TITLEBAR_HEIGHT + chat_h, ui.sidebar_width);
        else
            ui.wins[PANEL_TERMINAL] = NULL;
    } else {
        ui.wins[PANEL_SIDEBAR] = NULL;
        ui.wins[PANEL_CHAT] = derwin(stdscr, chat_h, ui.cols, APP_TITLEBAR_HEIGHT, 0);
        if (ui.terminal_visible)
            ui.wins[PANEL_TERMINAL] = derwin(stdscr, ui.terminal_height, ui.cols,
                                             APP_TITLEBAR_HEIGHT + chat_h, 0);
        else
            ui.wins[PANEL_TERMINAL] = NULL;
    }
    ui.wins[PANEL_STATUSBAR] = newwin(APP_STATUSBAR_HEIGHT, ui.cols, ui.rows - 1, 0);
    ui.wins[PANEL_OVERLAY] = NULL;
    ui.wins[PANEL_DIALOG] = NULL;

    for (int i = 0; i < PANEL_COUNT; i++) {
        if (ui.wins[i]) ui.panels[i] = new_panel(ui.wins[i]);
    }
    ui.dirty = true;
}

void ui_create_overlay(int ov_h, int ov_w) {
    if (ui.wins[PANEL_OVERLAY]) {
        del_panel(ui.panels[PANEL_OVERLAY]);
        delwin(ui.wins[PANEL_OVERLAY]);
    }
    int ov_y = (ui.rows - ov_h) / 2;
    int ov_x = (ui.cols - ov_w) / 2;
    if (ov_y < 0) ov_y = 0;
    if (ov_x < 0) ov_x = 0;
    ui.wins[PANEL_OVERLAY] = newwin(ov_h, ov_w, ov_y, ov_x);
    box(ui.wins[PANEL_OVERLAY], 0, 0);
    ui.panels[PANEL_OVERLAY] = new_panel(ui.wins[PANEL_OVERLAY]);
    top_panel(ui.panels[PANEL_OVERLAY]);
}

void ui_destroy_overlay(void) {
    if (ui.wins[PANEL_OVERLAY]) {
        del_panel(ui.panels[PANEL_OVERLAY]);
        delwin(ui.wins[PANEL_OVERLAY]);
        ui.panels[PANEL_OVERLAY] = NULL;
        ui.wins[PANEL_OVERLAY] = NULL;
    }
}

void ui_destroy_dialog(void) {
    if (ui.wins[PANEL_DIALOG]) {
        del_panel(ui.panels[PANEL_DIALOG]);
        delwin(ui.wins[PANEL_DIALOG]);
        ui.panels[PANEL_DIALOG] = NULL;
        ui.wins[PANEL_DIALOG] = NULL;
    }
}

void ui_create_dialog(int d_h, int d_w) {
    if (ui.wins[PANEL_DIALOG]) {
        del_panel(ui.panels[PANEL_DIALOG]);
        delwin(ui.wins[PANEL_DIALOG]);
    }
    int d_y = (ui.rows - d_h) / 2;
    int d_x = (ui.cols - d_w) / 2;
    if (d_y < 0) d_y = 0;
    if (d_x < 0) d_x = 0;
    ui.wins[PANEL_DIALOG] = newwin(d_h, d_w, d_y, d_x);
    ui.panels[PANEL_DIALOG] = new_panel(ui.wins[PANEL_DIALOG]);
    top_panel(ui.panels[PANEL_DIALOG]);
}

void ui_refresh_panels(void) {
    update_panels();
    doupdate();
}
