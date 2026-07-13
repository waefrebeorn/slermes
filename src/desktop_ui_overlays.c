/* desktop_ui_overlays.c -- extracted from src/app_desktop.c (angel-coder monolith split).
 * Self-contained desktop UI/PTY concern module. See app_desktop_internals.h.
 */

#include "app_desktop_internals.h"

void ui_draw_settings_tabs(WINDOW *win, int y, int w) {
    int cx = 1;
    for (int i = 0; i < APP_SETTINGS_TABS; i++) {
        const char *label = SETTINGS_TAB_LABELS[i];
        int tab_w = (int)strlen(label) + 3;
        if (cx + tab_w > w - 2) break;
        bool active = (app.settings_tab == i);
        int cp = active ? CP_OVERLAY_TAB_ACTIVE : CP_OVERLAY_TAB_INACTIVE;
        wattron(win, COLOR_PAIR(cp) | (active ? A_BOLD : 0));
        mvwprintw(win, y, cx, active ? " %s " : " %s ", label);
        wattroff(win, COLOR_PAIR(cp) | (active ? A_BOLD : 0));
        cx += tab_w + 1;
    }
}

void ui_draw_settings_overlay(void) {
    if (!app.settings_overlay || !ui.wins[PANEL_OVERLAY]) return;
    WINDOW *win = ui.wins[PANEL_OVERLAY];
    werase(win);
    wbkgd(win, COLOR_PAIR(CP_OVERLAY_BG));
    int rows = getmaxy(win);
    int cols = getmaxx(win);

    /* Title */
    wattron(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);
    mvwprintw(win, 0, 0, "%c Settings ", ACS_PLUS);
    wattroff(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);

    /* Tab bar */
    ui_draw_settings_tabs(win, 1, cols);

    /* Separator */
    mvwhline(win, 2, 0, ACS_HLINE, cols - 1);

    /* Content */
    int cy = 4, max_cy = rows - 2;
    switch (app.settings_tab) {
    case SETTINGS_TAB_MODEL: {
        mvwprintw(win, cy++, 1, "Active Model Settings");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        mvwprintw(win, cy++, 3, "Model:      %s", app.model);
        mvwprintw(win, cy++, 3, "Provider:   %s", app.provider);
        mvwprintw(win, cy++, 3, "Context:    128,000 tokens");
        mvwprintw(win, cy++, 3, "Max Output: 8,192 tokens");
        mvwprintw(win, cy++, 3, "Iteration:  %d / %d", app.iteration, app.max_iterations);
        cy++;
        if (cy < max_cy) {
            mvwprintw(win, cy++, 1, "Available Models:");
            for (int i = 0; i < app.model_count && cy < max_cy; i++) {
                bool is_cur = (strcmp(app.model_names[i], app.model) == 0);
                mvwprintw(win, cy++, 3, "%s %s", is_cur ? "●" : "○", app.model_names[i]);
            }
        }
        break;
    }
    case SETTINGS_TAB_PROVIDERS: {
        mvwprintw(win, cy++, 1, "Provider Accounts");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        mvwprintw(win, cy++, 3, "Active:  %s", app.provider);
        mvwprintw(win, cy++, 3, "Gateway: %s", app.gateway_url);
        cy++;
        mvwprintw(win, cy++, 1, "Connected Providers:");
        const char *providers[] = {"openrouter", "anthropic", "openai", "google", NULL};
        for (int i = 0; providers[i] && cy < max_cy; i++) {
            bool act = (strcmp(app.provider, providers[i]) == 0);
            mvwprintw(win, cy++, 3, "%s %s", act ? "●" : "○", providers[i]);
        }
        break;
    }
    case SETTINGS_TAB_GATEWAY: {
        mvwprintw(win, cy++, 1, "Gateway Connection");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        mvwprintw(win, cy++, 3, "URL:      %s", app.gateway.url);
        mvwprintw(win, cy++, 3, "State:    %s", app.gateway.state);
        mvwprintw(win, cy++, 3, "Profile:  %s", app.gateway.profile[0] ? app.gateway.profile : app.current_profile);
        mvwprintw(win, cy++, 3, "Ready:    %s", app.gateway.inference_ready ? "Yes" : "No");
        mvwprintw(win, cy++, 3, "Sessions: %d active", app.gateway.active_sessions);
        mvwprintw(win, cy++, 3, "Msgs:     %d today", app.gateway.messages_today);
        break;
    }
    case SETTINGS_TAB_NOTIFICATIONS: {
        mvwprintw(win, cy++, 1, "Notifications");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        if (app.notif_count == 0) {
            mvwprintw(win, cy++, 3, "No notifications");
        } else {
            for (int i = 0; i < app.notif_count && cy < max_cy; i++) {
                mvwprintw(win, cy++, 3, "[%s] %s",
                          app.notifications[i].urgent ? "!" : "i",
                          app.notifications[i].message);
            }
        }
        break;
    }
    case SETTINGS_TAB_PROFILES: {
        mvwprintw(win, cy++, 1, "Profile Management");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        for (int i = 0; i < app.profile_count && cy < max_cy; i++) {
            bool act = (i == app.active_profile);
            mvwprintw(win, cy++, 3, "%s %s", act ? "●" : "○", app.profile_names[i]);
        }
        break;
    }
    case SETTINGS_TAB_THEME: {
        mvwprintw(win, cy++, 1, "Theme Settings");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        for (int i = 0; i < 3 && cy < max_cy; i++) {
            desktop_theme_t t = (desktop_theme_t)i;
            bool act = (app.theme == t);
            mvwprintw(win, cy++, 3, "%s %s", act ? "●" : "○", THEME_NAMES[i]);
        }
        cy++;
        mvwprintw(win, cy++, 3, "Press 1-3 to switch theme");
        break;
    }
    case SETTINGS_TAB_KEYS: {
        mvwprintw(win, cy++, 1, "API Keys & Credentials");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        mvwprintw(win, cy++, 3, "OpenRouter:  configured");
        mvwprintw(win, cy++, 3, "Anthropic:   configured");
        mvwprintw(win, cy++, 3, "OpenAI:      not configured");
        mvwprintw(win, cy++, 3, "Google:      configured");
        mvwprintw(win, cy++, 3, "Firecrawl:   not configured");
        break;
    }
    case SETTINGS_TAB_ABOUT: {
        mvwprintw(win, cy++, 1, "About Slermes Desktop");
        mvwhline(win, cy++, 1, ACS_HLINE, cols - 3);
        cy++;
        mvwprintw(win, cy++, 3, "Version:  %s", "1.0.0-slermes");
        mvwprintw(win, cy++, 3, "Source:   Slermes C11 port");
        mvwprintw(win, cy++, 3, "Replaces: Electron/TS (446 files)");
        cy++;
        mvwprintw(win, cy++, 3, "Full-featured ncurses desktop app with:");
        mvwprintw(win, cy++, 5, "• Settings (8 tabs)");
        mvwprintw(win, cy++, 5, "• Command Palette (25 commands)");
        mvwprintw(win, cy++, 5, "• Model Picker");
        mvwprintw(win, cy++, 5, "• Session CRUD");
        mvwprintw(win, cy++, 5, "• Profiles & Themes");
        mvwprintw(win, cy++, 5, "• Notifications & Background Tasks");
        mvwprintw(win, cy++, 5, "• Subagents & Gateway Status");
        break;
    }
    }

    /* Footer */
    mvwprintw(win, rows - 1, 0, "%c", ACS_PLUS);
    wattron(win, COLOR_PAIR(CP_CHAT_DIM));
    mvwprintw(win, rows - 1, 2, " Tab/Arrows: navigate  q: close  Enter: select");
    wattroff(win, COLOR_PAIR(CP_CHAT_DIM));
    wnoutrefresh(win);
}

void ui_draw_command_palette(void) {
    if (!app.command_palette || !ui.wins[PANEL_OVERLAY]) return;
    WINDOW *win = ui.wins[PANEL_OVERLAY];
    werase(win);
    wbkgd(win, COLOR_PAIR(CP_OVERLAY_BG));
    int rows = getmaxy(win);
    int cols = getmaxx(win);

    wattron(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);
    mvwprintw(win, 0, 0, "%c Command Palette ", ACS_PLUS);
    wattroff(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);

    /* Query input */
    wattron(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
    mvwprintw(win, 1, 1, "> %-*.*s", cols - 4, cols - 4, app.palette_query);
    wattroff(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);

    int y = 3;
    const struct { const char *cat, *label, *action; } cmds[] = {
        /* Navigate */
        {"Navigate", "\xe2\x97\x8b Chat",           "view:chat"},
        {"Navigate", "\xe2\x99\xaf Settings",       "view:settings"},
        {"Navigate", "\xe2\x96\xb6 Command Center",  "view:command-center"},
        {"Navigate", "\xe2\x9c\xa6 Skills",         "view:skills"},
        {"Navigate", "\xe2\x9d\x90 Artifacts",      "view:artifacts"},
        {"Navigate", "\xe2\x8c\x9a Cron",           "view:cron"},
        {"Navigate", "\xe2\x99\xa0 Profiles",       "view:profiles"},
        {"Navigate", "\xe2\x99\x9f Agents",         "view:agents"},
        {"Navigate", "\xe2\x87\x84 Messaging",      "view:messaging"},
        /* Sessions */
        {"Sessions", "+ New Chat",    "session:new"},
        {"Sessions", "Delete Chat",   "session:delete"},
        {"Sessions", "Rename Chat",   "session:rename"},
        /* Settings */
        {"Settings", "Model",         "settings:model"},
        {"Settings", "Providers",     "settings:providers"},
        {"Settings", "Gateway",       "settings:gateway"},
        {"Settings", "Notifications", "settings:notifications"},
        {"Settings", "Profiles",      "settings:profiles"},
        {"Settings", "Theme",         "settings:theme"},
        {"Settings", "API Keys",      "settings:keys"},
        {"Settings", "About",         "settings:about"},
        /* Actions */
        {"Actions",  "Export Chat",    "action:export"},
        {"Actions",  "Clear Chat",     "action:clear"},
        {"Actions",  "Reset Config",   "action:reset"},
        {"Actions",  "Check Updates",  "action:check-update"},
        {"Actions",  "Keyboard Help",  "action:keyboard-help"},
        {"Actions",  "Quit",           "action:quit"},
    };
    int ncmds = sizeof(cmds) / sizeof(cmds[0]);

    /* Build query */
    char q_lower[256];
    for (int i = 0; app.palette_query[i]; i++)
        q_lower[i] = tolower((unsigned char)app.palette_query[i]);
    q_lower[app.palette_query_len] = '\0';

    int filtered = 0;
    int indices[APP_MAX_PALETTE_CMDS];
    for (int i = 0; i < ncmds && filtered < APP_MAX_PALETTE_CMDS; i++) {
        if (app.palette_query_len == 0) {
            indices[filtered++] = i;
        } else {
            char h[256];
            snprintf(h, sizeof(h), "%s %s %s", cmds[i].cat, cmds[i].label, cmds[i].action);
            for (int c = 0; h[c]; c++) h[c] = tolower((unsigned char)h[c]);
            if (strstr(h, q_lower)) indices[filtered++] = i;
        }
    }

    /* Sync to app state */
    app.palette_filtered_count = filtered;
    for (int i = 0; i < filtered; i++) {
        app.palette_filtered_indices[i] = indices[i];
        strncpy(app.palette_labels[i], cmds[indices[i]].label, 63);
        strncpy(app.palette_actions[i], cmds[indices[i]].action, 63);
    }
    if (app.palette_sel >= filtered) app.palette_sel = filtered - 1;
    if (app.palette_sel < 0) app.palette_sel = 0;

    const char *cur_cat = NULL;
    for (int i = 0; i < filtered && y < rows - 1; i++) {
        int ci = indices[i];
        if (cur_cat != cmds[ci].cat) {
            cur_cat = cmds[ci].cat;
            if (y < rows - 1) {
                wattron(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
                mvwprintw(win, y++, 1, "%s", cur_cat);
                wattroff(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
            }
        }
        if (y < rows - 1) {
            bool sel = (i == app.palette_sel);
            if (sel) wattron(win, COLOR_PAIR(CP_OVERLAY_SEL) | A_BOLD | A_REVERSE);
            mvwprintw(win, y, 3, "%s", cmds[ci].label);
            if (sel) wattroff(win, COLOR_PAIR(CP_OVERLAY_SEL) | A_BOLD | A_REVERSE);
            y++;
        }
    }
    if (filtered == 0 && y < rows - 1) {
        mvwprintw(win, y, 1, "No matching commands");
    }
    wnoutrefresh(win);
}

void ui_draw_model_picker(void) {
    if (!ui.model_picker_active || !ui.wins[PANEL_OVERLAY]) return;
    WINDOW *win = ui.wins[PANEL_OVERLAY];
    werase(win);
    wbkgd(win, COLOR_PAIR(CP_OVERLAY_BG));
    int rows = getmaxy(win);
    (void)getmaxx(win);

    wattron(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);
    mvwprintw(win, 0, 0, "%c Select Model ", ACS_PLUS);
    wattroff(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);

    wattron(win, COLOR_PAIR(CP_CHAT_DIM));
    mvwprintw(win, 1, 1, "Current: %s", app.model);
    wattroff(win, COLOR_PAIR(CP_CHAT_DIM));

    int y = 3;
    for (int i = 0; i < app.model_count && y < rows - 1; i++) {
        bool active = (strcmp(app.model_names[i], app.model) == 0);
        bool sel = (i == app.palette_sel);
        if (sel) wattron(win, COLOR_PAIR(CP_OVERLAY_SEL) | A_BOLD | A_REVERSE);
        else if (active) wattron(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
        mvwprintw(win, y++, 3, "%s %s", active ? "●" : "○", app.model_names[i]);
        if (sel) wattroff(win, COLOR_PAIR(CP_OVERLAY_SEL) | A_BOLD | A_REVERSE);
        else if (active) wattroff(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
    }
    mvwprintw(win, rows - 1, 2, " Arrows: navigate  Enter: select  q: close");
    wnoutrefresh(win);
}

void ui_draw_keyboard_shortcuts(void) {
    if (!ui.keyboard_shortcuts || !ui.wins[PANEL_OVERLAY]) return;
    WINDOW *win = ui.wins[PANEL_OVERLAY];
    werase(win);
    wbkgd(win, COLOR_PAIR(CP_OVERLAY_BG));
    int rows = getmaxy(win);
    int cols = getmaxx(win);

    wattron(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);
    mvwprintw(win, 0, 0, "%c Keyboard Shortcuts ", ACS_PLUS);
    wattroff(win, COLOR_PAIR(CP_OVERLAY_HEADING) | A_BOLD);

    const struct { const char *key, *action; } shortcuts[] = {
        {":",     "Command Palette"},
        {"s",     "Settings Overlay (8 tabs)"},
        {"p",     "Model Picker"},
        {"Tab",   "Toggle Sidebar"},
        {"t",     "Toggle Terminal Panel"},
        {"n",     "New Session"},
        {"r",     "Rename Session"},
        {"d",     "Delete Session"},
        {"y",     "Copy Last Response to Clipboard"},
        {"i",     "Paste from Clipboard"},
        {"q",     "Quit (or into composer)"},
        {"F1/?",  "Keyboard Shortcuts"},
        {"ESC/q", "Close Overlay/Dialog"},
        {"↑↓",    "Navigate List"},
        {"←→",    "Switch Sidebar Section"},
        {"PgUp/Dn","Scroll Chat"},
        {"1-3",   "Switch Theme (in Theme tab)"},
        {"Enter", "Execute Palette / Select Model"},
    };
    int n = sizeof(shortcuts) / sizeof(shortcuts[0]);
    int half = (n + 1) / 2;
    int col_w = cols / 2;

    int y = 2;
    for (int i = 0; i < half && y < rows - 2; i++) {
        /* Left column */
        wattron(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
        mvwprintw(win, y, 2, "%-8s", shortcuts[i].key);
        wattroff(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
        mvwprintw(win, y, 12, "%s", shortcuts[i].action);

        /* Right column */
        int ri = i + half;
        if (ri < n) {
            wattron(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
            mvwprintw(win, y, col_w + 2, "%-8s", shortcuts[ri].key);
            wattroff(win, COLOR_PAIR(CP_OVERLAY_HL) | A_BOLD);
            mvwprintw(win, y, col_w + 12, "%s", shortcuts[ri].action);
        }
        y++;
    }

    mvwprintw(win, rows - 1, 2, "Press any key to close");
    wnoutrefresh(win);
}

void ui_draw_dialog(void) {
    if (!ui.wins[PANEL_DIALOG]) return;
    WINDOW *win = ui.wins[PANEL_DIALOG];
    werase(win);
    wbkgd(win, COLOR_PAIR(CP_DIALOG_BG));
    box(win, 0, 0);
    int cols = getmaxx(win);

    if (app.delete_confirm) {
        wattron(win, A_BOLD | COLOR_PAIR(CP_OVERLAY_HEADING));
        mvwprintw(win, 1, 2, "Delete Session?");
        wattroff(win, A_BOLD | COLOR_PAIR(CP_OVERLAY_HEADING));
        wattron(win, COLOR_PAIR(CP_CHAT_DIM));
        mvwprintw(win, 2, 2, "Session: %s", app.confirm_session_id);
        wattroff(win, COLOR_PAIR(CP_CHAT_DIM));
        mvwprintw(win, 4, 2, "y/N to confirm");
    } else if (app.rename_active) {
        wattron(win, A_BOLD | COLOR_PAIR(CP_OVERLAY_HEADING));
        mvwprintw(win, 1, 2, "Rename Session");
        wattroff(win, A_BOLD | COLOR_PAIR(CP_OVERLAY_HEADING));
        wattron(win, COLOR_PAIR(CP_OVERLAY_SEL));
        mvwprintw(win, 3, 2, "> %-*.*s", cols - 6, cols - 6, app.rename_buf);
        wattroff(win, COLOR_PAIR(CP_OVERLAY_SEL));
        mvwprintw(win, 5, 2, "Enter: save  Esc: cancel");
    }
    wnoutrefresh(win);
}

void ui_draw_overlay(void) {
    if (!ui.wins[PANEL_OVERLAY]) return;
    if (ui.keyboard_shortcuts)           { ui_draw_keyboard_shortcuts(); }
    else if (ui.model_picker_active)     { ui_draw_model_picker(); }
    else if (app.command_palette)        { ui_draw_command_palette(); }
    else if (app.settings_overlay)       { ui_draw_settings_overlay(); }
    else {
        /* Generic fallback */
        WINDOW *win = ui.wins[PANEL_OVERLAY];
        werase(win);
        mvwprintw(win, 2, 2, "%s view", APP_VIEW_LABELS[app.overlay_view]);
        mvwprintw(win, 4, 2, "Press 'q' to close");
        wnoutrefresh(win);
    }
}
