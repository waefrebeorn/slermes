/*
 * tui_session_browser.c — interactive session-picker overlay.
 *
 * Subsystem split (soul.md: self-contained modules). Touches only
 * tui.sessions + tui.agent + tui.panes + tui.modal_mode (via tui_state.h)
 * and the render-core entry points; no file-scope state. The entry point
 * (tui_fullscreen_session_browse) is already public in tui_fullscreen.h.
 */
#include "tui_state.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void tui_draw_session_browser(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " SESSION BROWSER ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Search bar */
    mvwprintw(win, 1, 0, " Search: %s", tui.sessions.search);
    if (tui.sessions.search_mode) {
        wattron(win, A_BLINK);
        mvwaddch(win, 1, 9 + strlen(tui.sessions.search), '|');
        wattroff(win, A_BLINK);
    }

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 2, 0, " %-20s %-10s %-15s %-12s", "Title", "Messages", "Model", "Updated");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Session list with search filtering */
    int y = 4;
    int search_len = tui.sessions.search[0] ? (int)strlen(tui.sessions.search) : 0;
    for (int i = tui.sessions.scroll_offset;
         i < tui.sessions.count && y < w_rows - 2;
         i++) {

        /* Apply search filter: case-insensitive across ID, title, and model */
        if (search_len > 0) {
            bool match = false;
            /* Search session ID */
            if (strncasecmp(tui.sessions.sessions[i], tui.sessions.search, (size_t)search_len) == 0)
                match = true;
            /* Search title */
            if (!match && tui.sessions.meta[i].title[0] &&
                strstr_lower(tui.sessions.meta[i].title, tui.sessions.search))
                match = true;
            /* Search model */
            if (!match && tui.sessions.meta[i].model[0] &&
                strstr_lower(tui.sessions.meta[i].model, tui.sessions.search))
                match = true;
            if (!match)
                continue;
        }

        bool selected = (i == tui.sessions.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        /* Title or session ID (fallback) */
        const char *title = tui.sessions.meta[i].title[0] ?
                            tui.sessions.meta[i].title : tui.sessions.sessions[i];
        mvwprintw(win, y, 0, " %-20.20s", title);

        /* Message count */
        mvwprintw(win, y, 22, "%3d msgs", tui.sessions.meta[i].message_count);

        /* Model name */
        mvwprintw(win, y, 30, "%-15.15s", tui.sessions.meta[i].model);

        /* Last updated time */
        struct tm *tm_info = localtime(&tui.sessions.meta[i].updated_at);
        char time_buf[16];
        if (tm_info) {
            strftime(time_buf, sizeof(time_buf), "%m-%d %H:%M", tm_info);
        } else {
            snprintf(time_buf, sizeof(time_buf), "unknown");
        }
        mvwprintw(win, y, 46, "%-12s", time_buf);

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));

        y++;
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " [Enter] load [d] delete [e] export [/] search [q] quit ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}
int tui_session_browser_handle(int ch) {
    if (tui.sessions.search_mode) {
        if (ch == 27 || ch == '\n') {
            tui.sessions.search_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            size_t slen = strlen(tui.sessions.search);
            if (slen > 0) tui.sessions.search[slen - 1] = '\0';
        } else if (ch >= 32 && ch <= 126) {
            size_t slen = strlen(tui.sessions.search);
            if (slen < SESSION_SEARCH_MAX - 1) {
                tui.sessions.search[slen] = (char)ch;
                tui.sessions.search[slen + 1] = '\0';
            }
        }
        return 0;
    }

    switch (ch) {
        case 'q':
        case 27: /* ESC */
            tui.modal_mode = MODE_NORMAL;
            tui.sessions.active = false;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.sessions.selected > 0) tui.sessions.selected--;
            break;

        case KEY_DOWN:
            if (tui.sessions.selected < tui.sessions.count - 1)
                tui.sessions.selected++;
            break;

        case KEY_PPAGE:
            tui.sessions.selected -= 10;
            if (tui.sessions.selected < 0) tui.sessions.selected = 0;
            break;

        case KEY_NPAGE:
            tui.sessions.selected += 10;
            if (tui.sessions.selected >= tui.sessions.count)
                tui.sessions.selected = tui.sessions.count - 1;
            break;

        case '\n':
        case '\r':
            if (tui.sessions.selected >= 0 && tui.sessions.selected < tui.sessions.count) {
                /* Load session */
                if (tui.agent) {
                    if (strcmp(tui.sessions.sessions[tui.sessions.selected], "(no sessions)") != 0) {
                        agent_load_session(tui.agent,
                            tui.sessions.sessions[tui.sessions.selected]);
                        tui_history_add(MSG_ROLE_INFO, "Session loaded", false);
                    }
                }
            }
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        case 'd':
            if (tui.sessions.selected >= 0 && tui.sessions.selected < tui.sessions.count) {
                if (strcmp(tui.sessions.sessions[tui.sessions.selected], "(no sessions)") != 0) {
                    if (tui.agent && tui.agent->db) {
                        agent_session_delete(tui.agent,
                            tui.sessions.sessions[tui.sessions.selected]);
                    }
                    tui_history_add(MSG_ROLE_INFO, "Session deleted", false);
                    /* Refresh list */
                    tui_fullscreen_session_browse();
                    return 1;
                }
            }
            break;

        case 'e':
            if (tui.sessions.selected >= 0 && tui.sessions.selected < tui.sessions.count) {
                if (strcmp(tui.sessions.sessions[tui.sessions.selected], "(no sessions)") != 0) {
                    char *json = agent_session_export_json(
                        tui.agent, tui.sessions.sessions[tui.sessions.selected]);
                    if (json) {
                        tui_history_add(MSG_ROLE_INFO, json, false);
                        free(json);
                    }
                }
            }
            break;

        case '/':
            tui.sessions.search_mode = true;
            tui.sessions.search[0] = '\0';
            break;
    }

    /* Adjust scroll */
    int vis_rows = tui.panes[PANE_HISTORY].rows - 6;
    if (tui.sessions.selected < tui.sessions.scroll_offset)
        tui.sessions.scroll_offset = tui.sessions.selected;
    if (tui.sessions.selected >= tui.sessions.scroll_offset + vis_rows)
        tui.sessions.scroll_offset = tui.sessions.selected - vis_rows + 1;

    return 0;
}
void tui_fullscreen_session_browse(void) {
    tui.modal_mode = MODE_SESSION_BROWSE;
    tui.sessions.active = true;
    tui.sessions.selected = 0;
    tui.sessions.scroll_offset = 0;
    tui.sessions.search_mode = false;
    tui.sessions.search[0] = '\0';

    /* Populate session list from database */
    tui.sessions.count = 0;
    if (tui.agent && tui.agent->db) {
        size_t count = 0;
        session_entry_t *list = agent_session_list(&count, NULL, SESSION_LIST_MAX);
        if (list && count > 0) {
            int max_sessions = count < SESSION_LIST_MAX ? (int)count : SESSION_LIST_MAX;
            for (int i = 0; i < max_sessions; i++) {
                strncpy(tui.sessions.sessions[i], list[i].id,
                        sizeof(tui.sessions.sessions[0]) - 1);
                tui.sessions.meta[i] = list[i].meta; /* copy metadata */
                tui.sessions.count++;
            }
            /* Free the list — each entry's meta doesn't need deep free
             * since session_meta_t has no heap allocations. */
            free(list);
        }
    }
    /* If no DB or no sessions, show a single placeholder */
    if (tui.sessions.count == 0) {
        strncpy(tui.sessions.sessions[0], "(no sessions)",
                sizeof(tui.sessions.sessions[0]) - 1);
        tui.sessions.count = 1;
    }
}
