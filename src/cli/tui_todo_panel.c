/*
 * tui_todo_panel.c — todo/kanban overlay for tui_fullscreen.
 *
 * Subsystem split (soul.md: self-contained modules). Touches only
 * tui.todo_panel + tui.panes + tui.modal_mode (via tui_state.h) and the two
 * render-core entry points. No file-scope state of its own.
 */
#include "tui_state.h"
#include "hermes_json.h"
#include <curses.h>
#include <string.h>
#include <stdlib.h>

void tui_todo_panel_init(void) {
    tui.todo_panel.count = 0;
    tui.todo_panel.selected = 0;
    tui.todo_panel.scroll_offset = 0;
    tui.todo_panel.filter_idx = 0;
    tui.todo_panel.active = true;

    /* Call registry_dispatch for kanban_list */
    char *result = registry_dispatch("kanban_list",
        "{\"limit\":100,\"include_archived\":false}", "");
    if (!result) return;

    json_t *jr = json_parse(result, NULL);
    free(result);
    if (!jr) return;

    json_t *tasks = json_obj_get(jr, "tasks");
    if (tasks && tasks->type == JSON_ARRAY) {
        size_t n = tasks->c.count;
        if (n > TODO_PANEL_MAX) n = TODO_PANEL_MAX;
        for (size_t i = 0; i < n; i++) {
            json_t *t = tasks->c.items[i];
            const char *id = json_get_str(t, "id", "");
            const char *title = json_get_str(t, "title", "");
            const char *status = json_get_str(t, "status", "");
            const char *assignee = json_get_str(t, "assignee", "");
            strncpy(tui.todo_panel.id[(int)i], id, TODO_ID_MAX - 1);
            strncpy(tui.todo_panel.title[(int)i], title, TODO_TITLE_MAX - 1);
            strncpy(tui.todo_panel.status[(int)i], status, TODO_STATUS_MAX - 1);
            strncpy(tui.todo_panel.assignee[(int)i], assignee, TODO_STATUS_MAX - 1);
            tui.todo_panel.count++;
        }
    }
    json_free(jr);
    if (tui.todo_panel.count == 0) {
        strncpy(tui.todo_panel.title[0], "(no kanban tasks)", TODO_TITLE_MAX - 1);
        tui.todo_panel.count = 1;
    }
}
void tui_draw_todo_panel(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " TODO BOARD ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Filter bar */
    const char *filters[] = {"All", "Active", "Done/Archived"};
    mvwprintw(win, 1, 0, " [");
    for (int f = 0; f < 3; f++) {
        if (f == tui.todo_panel.filter_idx)
            wattron(win, A_REVERSE);
        mvwprintw(win, 1, 1 + (int)(strlen(filters[f-1]) + (f > 0 ? 3 : 0)), "%s", filters[f]);
        wattroff(win, A_REVERSE);
        if (f < 2) mvwprintw(win, 1, 1 + (int)(strlen(filters[f]) + (f > 0 ? 3 : 0)), " | ");
    }
    mvwprintw(win, 1, w_cols - 2, "]");

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 2, 0, " %-40s %-12s %-12s", "Title", "Status", "Assignee");
    wattroff(win, A_DIM | COLOR_PAIR(10));
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Task list */
    int y = 4;
    int shown = 0;
    for (int i = 0; i < tui.todo_panel.count && y < w_rows - 2; i++) {
        /* Apply filter */
        if (tui.todo_panel.filter_idx == 1) {
            if (strcmp(tui.todo_panel.status[i], "done") == 0 ||
                strcmp(tui.todo_panel.status[i], "archived") == 0)
                continue;
        } else if (tui.todo_panel.filter_idx == 2) {
            if (strcmp(tui.todo_panel.status[i], "done") != 0 &&
                strcmp(tui.todo_panel.status[i], "archived") != 0)
                continue;
        }

        if (shown < tui.todo_panel.scroll_offset) {
            shown++;
            continue;
        }

        bool selected = (i == tui.todo_panel.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        int status_color = 10;
        if (strcmp(tui.todo_panel.status[i], "done") == 0 ||
            strcmp(tui.todo_panel.status[i], "archived") == 0)
            status_color = 10;
        else if (strcmp(tui.todo_panel.status[i], "blocked") == 0)
            status_color = 7;
        else if (strcmp(tui.todo_panel.status[i], "running") == 0)
            status_color = 4;
        else if (strcmp(tui.todo_panel.status[i], "ready") == 0)
            status_color = 9;
        else
            status_color = 10;

        if (!selected)
            wattron(win, COLOR_PAIR(status_color));

        char display_title[TODO_TITLE_MAX + 4];
        strncpy(display_title, tui.todo_panel.title[i], TODO_TITLE_MAX - 1);
        display_title[TODO_TITLE_MAX - 1] = '\0';
        if ((int)strlen(display_title) > w_cols - 30)
            display_title[w_cols - 33] = '\0';

        mvwprintw(win, y, 0, " %-40s %-12s %-12s",
                  display_title,
                  tui.todo_panel.status[i],
                  tui.todo_panel.assignee[i]);

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));
        else
            wattroff(win, COLOR_PAIR(status_color));

        y++;
        shown++;
    }

    if (shown - tui.todo_panel.scroll_offset == 0) {
        mvwprintw(win, y++, 0, " (no tasks match filter)");
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0,
        " [Up/Dn] nav  [1-3] filter  [Enter] show  [c] complete  [q] close ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}
int tui_todo_panel_handle(int ch) {
    switch (ch) {
        case 'q':
        case 27:
            tui.modal_mode = MODE_NORMAL;
            tui.todo_panel.active = false;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.todo_panel.selected > 0) tui.todo_panel.selected--;
            break;

        case KEY_DOWN:
            if (tui.todo_panel.selected < tui.todo_panel.count - 1)
                tui.todo_panel.selected++;
            break;

        case KEY_PPAGE:
            tui.todo_panel.scroll_offset -= 10;
            if (tui.todo_panel.scroll_offset < 0)
                tui.todo_panel.scroll_offset = 0;
            break;

        case KEY_NPAGE:
            tui.todo_panel.scroll_offset += 10;
            break;

        case '1': case '2': case '3':
            tui.todo_panel.filter_idx = ch - '1';
            tui.todo_panel.selected = 0;
            tui.todo_panel.scroll_offset = 0;
            break;

        case '\n':
        case '\r':
            if (tui.todo_panel.count > 0 && tui.todo_panel.id[tui.todo_panel.selected][0]) {
                char json_args[128];
                snprintf(json_args, sizeof(json_args),
                         "{\"task_id\":\"%s\"}", tui.todo_panel.id[tui.todo_panel.selected]);
                char *result = registry_dispatch("kanban_show", json_args, "");
                if (result) {
                    json_t *jr = json_parse(result, NULL);
                    free(result);
                    if (jr) {
                        const char *title = json_get_str(jr, "title", "(no title)");
                        const char *status = json_get_str(jr, "status", "?");
                        const char *assignee = json_get_str(jr, "assignee", "");
                        const char *body = json_get_str(jr, "body", "");
                        char detail[2048];
                        snprintf(detail, sizeof(detail),
                                 "Task: %s | Status: %s | Assignee: %s\n%s",
                                 title, status, assignee, body);
                        tui_history_add(MSG_ROLE_INFO, detail, false);
                        json_free(jr);
                    }
                }
                tui.modal_mode = MODE_NORMAL;
                tui.todo_panel.active = false;
                tui_redraw_history();
                return 1;
            }
            break;

        case 'c':
            if (tui.todo_panel.count > 0 && tui.todo_panel.id[tui.todo_panel.selected][0]) {
                char json_args[128];
                snprintf(json_args, sizeof(json_args),
                         "{\"task_id\":\"%s\",\"summary\":\"Completed from TUI\"}",
                         tui.todo_panel.id[tui.todo_panel.selected]);
                char *result = registry_dispatch("kanban_complete", json_args, "");
                if (result) {
                    tui_history_add(MSG_ROLE_INFO, "Task completed", false);
                    free(result);
                }
                tui_todo_panel_init();
                tui.todo_panel.active = true;
            }
            break;

        default:
            return 0;
    }
    tui_draw_todo_panel();
    return 1;
}
