/* desktop_ui_chrome.c -- extracted from src/app_desktop.c (angel-coder monolith split).
 * Self-contained desktop UI/PTY concern module. See app_desktop_internals.h.
 */

#include "app_desktop_internals.h"

void ui_draw_titlebar(void) {
    if (!ui.wins[PANEL_TITLEBAR]) return;
    char left[256], right[256], center[256];

    snprintf(left, sizeof(left), "  Slermes ");

    const char *conn_str = "";
    int conn_cp = CP_TITLEBAR;
    switch (app.conn_state) {
        case CONN_CONNECTED:      conn_str = "● Online"; conn_cp = CP_TITLEBAR_HL; break;
        case CONN_CONNECTING:     conn_str = "◎ Connecting"; conn_cp = CP_TITLEBAR_HL; break;
        case CONN_DISCONNECTED:   conn_str = "○ Offline"; conn_cp = CP_TITLEBAR_WARN; break;
        case CONN_ERROR:          conn_str = "✗ Error"; conn_cp = CP_TITLEBAR_WARN; break;
        case CONN_REAUTH_REQUIRED:conn_str = "\xf0\x9f\x94\x91 Locked"; conn_cp = CP_TITLEBAR_WARN; break;
    }

    snprintf(center, sizeof(center), " %s ", app.model);
    snprintf(right, sizeof(right), " %s %s%s  ",
             conn_str,
             app.current_profile,
             app.update_available ? " \xe2\x86\x91" : "");
    draw_bar(ui.wins[PANEL_TITLEBAR], conn_cp, ui.cols, left, center, right);
}

void ui_draw_sidebar(void) {
    if (!ui.wins[PANEL_SIDEBAR]) return;
    werase(ui.wins[PANEL_SIDEBAR]);
    wbkgd(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR));

    int y = 0;
    int w = getmaxx(ui.wins[PANEL_SIDEBAR]);
    int h = getmaxy(ui.wins[PANEL_SIDEBAR]);

    /* Header */
    wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_HEADING) | A_BOLD);
    mvwprintw(ui.wins[PANEL_SIDEBAR], y++, 0, " \xf0\x9f\x94\xb0 Slermes Agent");
    whline(ui.wins[PANEL_SIDEBAR], ACS_HLINE, w - 1);
    wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_HEADING) | A_BOLD);
    y++;

    /* Search bar */
    wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_HL));
    mvwprintw(ui.wins[PANEL_SIDEBAR], y, 0, " \xf0\x9f\x94\x8d %-*.*s",
              w - 4, w - 4, ui.sidebar_search_active ? ui.sidebar_search : "");
    wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_HL));
    y += 1;

    /* Sessions section */
    draw_section_header(ui.wins[PANEL_SIDEBAR], y++, w, "Sessions");
    int shown = 0;
    for (int i = 0; i < app.session_count && y < h - 10; i++) {
        /* Apply search filter */
        if (ui.sidebar_search_active && ui.sidebar_search_len > 0) {
            char lower_title[256], lower_q[128];
            strncpy(lower_title, app.session_titles[i], 255);
            for (int c = 0; lower_title[c]; c++) lower_title[c] = tolower((unsigned char)lower_title[c]);
            strncpy(lower_q, ui.sidebar_search, 127);
            for (int c = 0; lower_q[c]; c++) lower_q[c] = tolower((unsigned char)lower_q[c]);
            if (!strstr(lower_title, lower_q)) continue;
        }
        shown++;

        bool sel = (app.sidebar_section == 0 && app.session_sel == i);
        int cp = sel ? CP_SIDEBAR_SEL : CP_SIDEBAR;

        if (sel) {
            wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(cp) | A_BOLD | A_REVERSE);
            mvwprintw(ui.wins[PANEL_SIDEBAR], y, 0, " %c ", sel ? '>' : ' ');
            wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(cp) | A_BOLD | A_REVERSE);
        }

        /* Session title */
        char display[256];
        snprintf(display, sizeof(display), "%-*.*s", w - 3, w - 3,
                 app.session_titles[i][0] ? app.session_titles[i] : "(empty)");
        if (sel) {
            wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_SEL) | A_BOLD | A_REVERSE);
        } else {
            wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR));
        }
        mvwprintw(ui.wins[PANEL_SIDEBAR], y, 1, "%s", display);
        if (sel) {
            wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_SEL) | A_BOLD | A_REVERSE);
        } else {
            wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR));
        }

        /* Metadata line: message count + last active */
        y++;
        if (y < h - 10) {
            wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_META) | A_DIM);
            mvwprintw(ui.wins[PANEL_SIDEBAR], y, 2, "%d msgs", (i * 7 + 12) % 50 + 3);
            wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_META) | A_DIM);
        }
        y++;
    }

    if (shown == 0 && y < h - 10) {
        wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_DIM) | A_DIM);
        mvwprintw(ui.wins[PANEL_SIDEBAR], y++, 2, "No sessions found");
        wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_DIM) | A_DIM);
    }

    /* +New Chat item */
    if (y < h - 8) {
        bool new_sel = (app.sidebar_section == 0 && app.session_sel >= app.session_count);
        if (new_sel) {
            wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_SEL) | A_BOLD | A_REVERSE);
        } else {
            wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_PINNED));
        }
        mvwprintw(ui.wins[PANEL_SIDEBAR], y++, 1, "+ New Chat");
        if (new_sel) {
            wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_SEL) | A_BOLD | A_REVERSE);
        } else {
            wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_PINNED));
        }
    }
    y++;

    /* Navigation section */
    if (y < h - 1) {
        draw_section_header(ui.wins[PANEL_SIDEBAR], y++, w, "Navigation");

        const struct { const char *icon; const char *label; app_view_t view; } nav[] = {
            {"\xe2\x97\x8b", "Chat",        VIEW_CHAT},
            {"\xe2\x96\xb6", "Cmd Center",  VIEW_COMMAND_CENTER},
            {"\xe2\x9c\xa6", "Skills",      VIEW_SKILLS},
            {"\xe2\x9d\x90", "Artifacts",   VIEW_ARTIFACTS},
            {"\xe2\x8c\x9a", "Cron",        VIEW_CRON},
            {"\xe2\x99\xa0", "Profiles",    VIEW_PROFILES},
            {"\xe2\x99\x9f", "Agents",      VIEW_AGENTS},
            {"\xe2\x87\x84", "Messaging",   VIEW_MESSAGING},
        };
        int ncnt = sizeof(nav) / sizeof(nav[0]);

        for (int i = 0; i < ncnt && y < h - 1; i++) {
            bool sel = (app.sidebar_section == 1 && app.sidebar_sel == i);

            /* Highlight if it's the active view */
            bool is_active = (nav[i].view == app.active_view && !sel);

            if (sel) {
                wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_SEL) | A_BOLD | A_REVERSE);
            } else if (is_active) {
                wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_HL) | A_BOLD);
            } else {
                wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR));
            }

            mvwprintw(ui.wins[PANEL_SIDEBAR], y++, 1, "%s %-*.*s",
                      nav[i].icon, w - 6, w - 6, nav[i].label);

            if (sel)
                wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_SEL) | A_BOLD | A_REVERSE);
            else if (is_active)
                wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR_HL) | A_BOLD);
            else
                wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_SIDEBAR));
        }
    }

    /* Profile section at bottom of sidebar */
    if (y < h - 3) {
        y = h - 3;
        whline(ui.wins[PANEL_SIDEBAR], ACS_HLINE, w - 1);
        y++;
        wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_TITLEBAR));
        mvwprintw(ui.wins[PANEL_SIDEBAR], y, 1, "\xf0\x9f\x91\xa4 wubu");
        wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_TITLEBAR));
        wattron(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_STATUSBAR_GREEN) | A_DIM);
        mvwprintw(ui.wins[PANEL_SIDEBAR], y, w - 14, "Connected");
        wattroff(ui.wins[PANEL_SIDEBAR], COLOR_PAIR(CP_STATUSBAR_GREEN) | A_DIM);
    }

    /* Right border (ACS_VLINE) separate sidebar from chat */
    mvwvline(ui.wins[PANEL_SIDEBAR], 0, w - 1, ACS_VLINE, h);

    wnoutrefresh(ui.wins[PANEL_SIDEBAR]);
}

void ui_draw_chat(void) {
    if (!ui.wins[PANEL_CHAT]) return;
    werase(ui.wins[PANEL_CHAT]);
    wbkgd(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_BG));
    int rows = getmaxy(ui.wins[PANEL_CHAT]);
    int cols = getmaxx(ui.wins[PANEL_CHAT]);

    int y = 0;
    /* Draw messages */
    for (int i = ui.scroll_offset; i < ui.rendered_count && y < rows - APP_COMPOSER_HEIGHT; i++) {
        if (!ui.rendered_msgs[i]) continue;
        const char *role = ui.rendered_msgs[i]->role;
        const char *label = "";
        int rcp = CP_CHAT_ASSISTANT;
        if (strcmp(role, "system") == 0)    { label = "System";    rcp = CP_CHAT_SYSTEM; }
        else if (strcmp(role, "user") == 0) { label = "User";      rcp = CP_CHAT_USER; }
        else if (strcmp(role, "assistant")==0){label = "Assistant";rcp = CP_CHAT_ASSISTANT; }
        else                                { label = "Tool";      rcp = CP_CHAT_TOOL; }

        /* Role header with separator line */
        mvwhline(ui.wins[PANEL_CHAT], y, 0, ACS_HLINE, cols);
        wattron(ui.wins[PANEL_CHAT], COLOR_PAIR(rcp) | A_BOLD);
        mvwprintw(ui.wins[PANEL_CHAT], y, 0, " %s ", label);
        wattroff(ui.wins[PANEL_CHAT], COLOR_PAIR(rcp) | A_BOLD);
        y++;

        /* Render tokens */
        for (int t = 0; t < ui.rendered_msgs[i]->token_count && y < rows - APP_COMPOSER_HEIGHT; t++) {
            chat_render_token_t *tok = &ui.rendered_msgs[i]->tokens[t];
            if (!tok->text) continue;
            int cp = CP_CHAT_ASSISTANT;
            bool bold = false;
            switch (tok->type) {
                case TOKEN_TEXT:            cp = CP_CHAT_ASSISTANT; break;
                case TOKEN_BOLD_START:      bold = true; continue;
                case TOKEN_BOLD_END:        bold = false; continue;
                case TOKEN_CODE_INLINE:     cp = CP_CHAT_CODE; break;
                case TOKEN_CODE_BLOCK_START:cp = CP_CHAT_SYSTEM; bold = true; break;
                case TOKEN_TOOL_CALL_START: cp = CP_CHAT_TOOL; bold = true; break;
                case TOKEN_TOOL_NAME:       cp = CP_CHAT_TOOL; bold = true; break;
                case TOKEN_TOOL_RESULT:     cp = CP_CHAT_DIM; break;
                case TOKEN_TOOL_RESULT_ERROR: cp = CP_CHAT_ERROR; break;
                case TOKEN_KEYWORD:         cp = CP_CHAT_TOOL; break;
                case TOKEN_STRING:          cp = CP_CHAT_USER; break;
                case TOKEN_COMMENT:         cp = CP_CHAT_DIM; break;
                default: cp = CP_CHAT_ASSISTANT;
            }
            wattron(ui.wins[PANEL_CHAT], COLOR_PAIR(cp));
            if (bold) wattron(ui.wins[PANEL_CHAT], A_BOLD);
            mvwprintw(ui.wins[PANEL_CHAT], y, 0, " %-*.*s", cols - 2, cols - 2, tok->text);
            if (bold) wattroff(ui.wins[PANEL_CHAT], A_BOLD);
            wattroff(ui.wins[PANEL_CHAT], COLOR_PAIR(cp));
            y++;
        }
    }

    /* If no messages, show placeholder */
    if (ui.rendered_count == 0 && y < rows - APP_COMPOSER_HEIGHT) {
        wattron(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_DIM));
        int py = (rows - APP_COMPOSER_HEIGHT) / 3;
        int pcol = cols / 4;
        mvwprintw(ui.wins[PANEL_CHAT], py, pcol, "No messages yet");
        mvwprintw(ui.wins[PANEL_CHAT], py+1, pcol, "Type : for commands, s for settings");
        mvwprintw(ui.wins[PANEL_CHAT], py+2, pcol, "F1 or ? for keyboard shortcuts");
        wattroff(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_DIM));
        y = py + 4;
    }

    /* Composer area */
    int comp_y = rows - APP_COMPOSER_HEIGHT;
    if (comp_y >= 0 && comp_y < rows) {
        mvwhline(ui.wins[PANEL_CHAT], comp_y, 0, ACS_HLINE, cols);
        /* Model pill */
        wattron(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_TOOL));
        mvwprintw(ui.wins[PANEL_CHAT], comp_y, 0, " %s ", app.model);
        wattroff(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_TOOL));

        /* Input area */
        wattron(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_COMPOSER));
        const char *ct = composer_get_text(ui.composer);
        char ps[512];
        snprintf(ps, sizeof(ps), "> %s", ct ? ct : "");
        mvwprintw(ui.wins[PANEL_CHAT], comp_y + 1, 0, "%-*.*s", cols - 1, cols - 1, ps);
        wattroff(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_COMPOSER));

        /* Autocomplete popup (above the input line). */
        if (ui.composer && ui.composer->autocomplete_visible) {
            const composer_suggestion_t *sugg = NULL;
            int n = composer_get_suggestions(ui.composer, &sugg, COMPOSER_MAX_SUGGEST);
            int sel = ui.composer->suggestion_selected;
            int pop_y = comp_y - n - 1;
            if (pop_y < 0) pop_y = 0;
            for (int si = 0; si < n && si < 8; si++) {
                wattron(ui.wins[PANEL_CHAT],
                        si == sel ? COLOR_PAIR(CP_CHAT_TOOL) : COLOR_PAIR(CP_CHAT_DIM));
                char line[200];
                snprintf(line, sizeof(line), "  %-24s %s",
                         sugg[si].text, sugg[si].description);
                mvwprintw(ui.wins[PANEL_CHAT], pop_y + si, 0, "%-*.*s",
                          cols - 1, cols - 1, line);
                wattroff(ui.wins[PANEL_CHAT],
                         si == sel ? COLOR_PAIR(CP_CHAT_TOOL) : COLOR_PAIR(CP_CHAT_DIM));
            }
        }

        /* Controls hint — show available keybinds */
        wattron(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_DIM));
        mvwprintw(ui.wins[PANEL_CHAT], comp_y + 2, 0,
                  " Tab:sidebar t:terminal ::palette s:settings ?:help");
        if (ui.sidebar_search_active) {
            mvwprintw(ui.wins[PANEL_CHAT], comp_y + 3, 0,
                      " Searching: %s_", ui.sidebar_search);
        } else {
            mvwprintw(ui.wins[PANEL_CHAT], comp_y + 3, 0,
                      " Enter:send  n:new  r:rename  d:delete  q:quit");
        }
        wattroff(ui.wins[PANEL_CHAT], COLOR_PAIR(CP_CHAT_DIM));
    }

    wnoutrefresh(ui.wins[PANEL_CHAT]);
}

void ui_draw_terminal(void) {
    if (!ui.wins[PANEL_TERMINAL] || !ui.terminal_visible) return;
    werase(ui.wins[PANEL_TERMINAL]);
    int rows = getmaxy(ui.wins[PANEL_TERMINAL]);
    int cols = getmaxx(ui.wins[PANEL_TERMINAL]);

    wattron(ui.wins[PANEL_TERMINAL], A_REVERSE);
    mvwprintw(ui.wins[PANEL_TERMINAL], 0, 0, " %-*.*s  [pinned: parity-scan]",
              cols - 25, cols - 25, " Terminal ");
    wattroff(ui.wins[PANEL_TERMINAL], A_REVERSE);

    if (!app.term_pty || !app.term_pty->active) {
        wattron(ui.wins[PANEL_TERMINAL], COLOR_PAIR(CP_CHAT_DIM));
        mvwprintw(ui.wins[PANEL_TERMINAL], 1, 0, " PTY not connected — press 't' to toggle");
        wattroff(ui.wins[PANEL_TERMINAL], COLOR_PAIR(CP_CHAT_DIM));
        for (int r = 2; r < rows; r++) {
            mvwprintw(ui.wins[PANEL_TERMINAL], r, 0, "~");
        }
    } else {
        /* Render PTY output */
        wattron(ui.wins[PANEL_TERMINAL], COLOR_PAIR(CP_CHAT_DIM));
        mvwprintw(ui.wins[PANEL_TERMINAL], 1, 0, " PID:%d %dx%d fd:%d",
                  app.term_pty->pid, app.term_pty->cols, app.term_pty->rows,
                  app.term_pty->master_fd);
        wattroff(ui.wins[PANEL_TERMINAL], COLOR_PAIR(CP_CHAT_DIM));

        /* Render terminal buffer content */
        if (app.term_buf_len > 0) {
            /* Split buffer into lines based on terminal width */
            int render_row = 2;
            int buf_pos = 0;
            int skip = 0;
            /* Skip to last N lines that fit in the window */
            int line_count = 0;
            for (int i = 0; i < app.term_buf_len; i++) {
                if (app.term_buf[i] == '\n') line_count++;
            }
            int start_line = (line_count > rows - 2) ? line_count - (rows - 2) : 0;
            for (int i = 0; i < app.term_buf_len && render_row < rows; i++) {
                if (app.term_buf[i] == '\n') {
                    if (skip < start_line) { skip++; buf_pos = i + 1; continue; }
                    render_row++;
                    buf_pos = i + 1;
                } else {
                    if (skip < start_line) continue;
                    int col = i - buf_pos;
                    if (col < cols) {
                        mvwaddch(ui.wins[PANEL_TERMINAL], render_row, col,
                                 (unsigned char)app.term_buf[i]);
                    }
                }
            }
        }
    }
    wnoutrefresh(ui.wins[PANEL_TERMINAL]);
}

void ui_draw_statusbar(void) {
    if (!ui.wins[PANEL_STATUSBAR]) return;
    char left[512], right[512];
    int cols = ui.cols;

    const char *gw_str = "";
    switch (app.conn_state) {
        case CONN_CONNECTED:      gw_str = "●"; break;
        case CONN_CONNECTING:     gw_str = "◎"; break;
        case CONN_DISCONNECTED:   gw_str = "○"; break;
        case CONN_ERROR:          gw_str = "✗"; break;
        case CONN_REAUTH_REQUIRED:gw_str = "\xf0\x9f\x94\x91"; break;
    }

    char model_str[64];
    snprintf(model_str, sizeof(model_str), "%.24s", app.model);

    /* Background tasks */
    char tasks_str[64] = "";
    if (app.bg_task_count > 0) {
        int run = 0, fail = 0;
        for (int i = 0; i < app.bg_task_count; i++) {
            if (app.bg_tasks[i].running) run++;
            if (!app.bg_tasks[i].running && app.bg_tasks[i].has_error) fail++;
        }
        if (run > 0 || fail > 0)
            snprintf(tasks_str, sizeof(tasks_str), " [%d\xe2\x86\x91%d\xe2\x86\x93]", run, fail);
    }

    char sub_str[32] = "";
    if (app.subagent_count > 0) {
        int sr = 0;
        for (int i = 0; i < app.subagent_count; i++)
            if (app.subagents[i].running) sr++;
        if (sr > 0) snprintf(sub_str, sizeof(sub_str), " S:%d", sr);
    }

    const char *yolo_str = app.yolo_active ? " \xe2\x9a\xa1YOLO" : "";
    char token_str[64];
    snprintf(token_str, sizeof(token_str), " In:%dK Out:%dK",
             app.tokens_in / 1000, app.tokens_out / 1000);
    char ctx_str[16] = "";
    if (app.context_usage_pct > 0)
        snprintf(ctx_str, sizeof(ctx_str), " [%d%%]", app.context_usage_pct);

    snprintf(left, sizeof(left), " %s %s%s%s%s%s%s",
             gw_str, model_str, token_str, ctx_str, tasks_str, sub_str, yolo_str);

    char iter_str[64];
    snprintf(iter_str, sizeof(iter_str), " %d/%d", app.iteration, app.max_iterations);
    char upt_str[32] = "";
    if (app.update_available)
        snprintf(upt_str, sizeof(upt_str), " v%s\xe2\x86\xbb", app.update_version);
    char ses_str[32];
    snprintf(ses_str, sizeof(ses_str), " %d ses", app.gateway.active_sessions);
    snprintf(right, sizeof(right), "%s%s%s ", ses_str, upt_str, iter_str);

    if (app.notification[0] && time(NULL) - app.notification_time < app.notification_duration_sec) {
        draw_bar(ui.wins[PANEL_STATUSBAR], CP_NOTIFICATION, cols, left, NULL, right);
    } else {
        draw_bar(ui.wins[PANEL_STATUSBAR], CP_STATUSBAR, cols, left, NULL, right);
    }
}

void ui_draw_all(void) {
    ui_draw_titlebar();
    ui_draw_sidebar();
    ui_draw_chat();
    ui_draw_terminal();
    ui_draw_statusbar();

    if (app.delete_confirm || app.rename_active) {
        ui_draw_dialog();
    } else if (ui.wins[PANEL_OVERLAY]) {
        ui_draw_overlay();
    }
    ui_refresh_panels();
    ui.dirty = false;
}
