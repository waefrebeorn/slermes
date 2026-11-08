/*
 * tui_config_editor.c — interactive settings/config editor overlay.
 *
 * Subsystem split (soul.md: self-contained modules). Touches only
 * tui.config_editor + tui.agent + tui.panes + tui.modal_mode (via
 * tui_state.h) and the render-core entry points; no file-scope state.
 */
#include "tui_state.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void tui_config_editor_init(void) {
    tui.config_editor.count = 0;
    tui.config_editor.selected = 0;
    tui.config_editor.scroll_offset = 0;
    tui.config_editor.edit_mode = false;
    tui.config_editor.search_mode = false;
    tui.config_editor.search[0] = '\0';

    /* Populate with key config entries */
    struct { const char *k; const char *v; const char *d; } cfg_entries[] = {
        {"model", "", "LLM model name"},
        {"provider", "", "LLM provider"},
        {"base_url", "", "API base URL"},
        {"max_turns", "90", "Maximum tool-calling iterations"},
        {"verbose", "1", "Verbosity level (0/1/2)"},
        {"stream", "true", "Enable token streaming"},
        {"show_reasoning", "false", "Show reasoning content"},
        {"skin", "default", "Display theme/skin name"},
        {"compact", "false", "Compact display mode"},
        {"show_cost", "false", "Show token cost"},
        {"timestamps", "false", "Show timestamps"},
        {"quiet_mode", "false", "Suppress non-essential output"},
        {"yolo_mode", "false", "Skip all approval prompts"},
        {"fast_mode", "false", "Skip system prompt for speed"},
        {"allow_bg", "true", "Allow background processes"},
        {"approval_mode", "manual", "Approval mode (off/manual/auto)"},
        {"terminal_timeout", "180", "Terminal command timeout (s)"},
        {"compress_enabled", "false", "Smart context compression"},
        {NULL, NULL, NULL}
    };

    for (int i = 0; cfg_entries[i].k && tui.config_editor.count < CONFIG_KEY_MAX; i++) {
        strncpy(tui.config_editor.entries[tui.config_editor.count].key,
                cfg_entries[i].k, CONFIG_KEY_MAX - 1);
        strncpy(tui.config_editor.entries[tui.config_editor.count].value,
                cfg_entries[i].v, CONFIG_VALUE_MAX - 1);
        strncpy(tui.config_editor.entries[tui.config_editor.count].description,
                cfg_entries[i].d, sizeof(tui.config_editor.entries[0].description) - 1);

        /* Try to load current value from agent config */
        if (tui.agent) {
            /* Simplified: just use defaults for now */
        }

        tui.config_editor.count++;
    }
}
void tui_draw_config_editor(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " CONFIG EDITOR ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    /* Search bar */
    mvwprintw(win, 1, 0, " Search: %s", tui.config_editor.search);
    if (tui.config_editor.search_mode) {
        wattron(win, A_BLINK);
        mvwaddch(win, 1, 9 + strlen(tui.config_editor.search), '|');
        wattroff(win, A_BLINK);
    }

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 2, 0, " %-25s %-25s %s", "Key", "Value", "Description");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 3, 0, ACS_HLINE, w_cols - 1);

    /* Entries */
    int y = 4;
    int search_len = tui.config_editor.search[0] ? (int)strlen(tui.config_editor.search) : 0;
    for (int i = 0;
         i < tui.config_editor.count && y < w_rows - 2;
         i++) {

        /* Apply search filter: case-insensitive key match */
        if (search_len > 0) {
            if (strncasecmp(tui.config_editor.entries[i].key, tui.config_editor.search, (size_t)search_len) != 0 &&
                strstr_lower(tui.config_editor.entries[i].description, tui.config_editor.search) == false)
                continue;
        }

        if (i < tui.config_editor.scroll_offset) continue;

        bool selected = (i == tui.config_editor.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        mvwprintw(win, y, 0, " %-25s %-25s %s",
                  tui.config_editor.entries[i].key,
                  tui.config_editor.entries[i].value,
                  tui.config_editor.entries[i].description);

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));

        y++;
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " [Enter] edit [g] get value [e] explain [/] search [q] quit ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}
int tui_config_editor_handle(int ch) {
    if (tui.config_editor.search_mode) {
        if (ch == 27 || ch == '\n') {
            tui.config_editor.search_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            size_t slen = strlen(tui.config_editor.search);
            if (slen > 0) tui.config_editor.search[slen - 1] = '\0';
        } else if (ch >= 32 && ch <= 126) {
            size_t slen = strlen(tui.config_editor.search);
            if (slen < CONFIG_SEARCH_MAX - 1) {
                tui.config_editor.search[slen] = (char)ch;
                tui.config_editor.search[slen + 1] = '\0';
            }
        }
        return 0;
    }
    if (tui.config_editor.edit_mode) {
        if (ch == '\n' || ch == '\r') {
            /* Save edited value */
            strncpy(tui.config_editor.entries[tui.config_editor.selected].value,
                    tui.config_editor.edit_buf, CONFIG_VALUE_MAX - 1);
            tui.config_editor.edit_mode = false;
        } else if (ch == 27) {
            tui.config_editor.edit_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            size_t elen = strlen(tui.config_editor.edit_buf);
            if (elen > 0) tui.config_editor.edit_buf[elen - 1] = '\0';
        } else if (ch >= 32 && ch <= 126) {
            size_t elen = strlen(tui.config_editor.edit_buf);
            if (elen < sizeof(tui.config_editor.edit_buf) - 2) {
                tui.config_editor.edit_buf[elen] = (char)ch;
                tui.config_editor.edit_buf[elen + 1] = '\0';
            }
        }
        return 0;
    }

    switch (ch) {
        case 'q':
        case 27:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.config_editor.selected > 0) tui.config_editor.selected--;
            break;

        case KEY_DOWN:
            if (tui.config_editor.selected < tui.config_editor.count - 1)
                tui.config_editor.selected++;
            break;

        case KEY_PPAGE:
            tui.config_editor.selected -= 10;
            if (tui.config_editor.selected < 0) tui.config_editor.selected = 0;
            break;

        case KEY_NPAGE:
            tui.config_editor.selected += 10;
            if (tui.config_editor.selected >= tui.config_editor.count)
                tui.config_editor.selected = tui.config_editor.count - 1;
            break;

        case '\n':
        case '\r':
            /* Enter edit mode for selected key */
            tui.config_editor.edit_mode = true;
            strncpy(tui.config_editor.edit_buf,
                    tui.config_editor.entries[tui.config_editor.selected].value,
                    sizeof(tui.config_editor.edit_buf) - 1);
            break;

        case 'g': {
            /* 'get' — show current value as history message */
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s = %s  (%s)",
                     tui.config_editor.entries[tui.config_editor.selected].key,
                     tui.config_editor.entries[tui.config_editor.selected].value,
                     tui.config_editor.entries[tui.config_editor.selected].description);
            tui_history_add(MSG_ROLE_INFO, buf, false);
            tui_redraw_history();
            break;
        }

        case 'e': {
            /* 'explain' — show description as history message */
            char buf[1024];
            snprintf(buf, sizeof(buf), "  * %s: %s\n  Default: %s\n  Current: %s",
                     tui.config_editor.entries[tui.config_editor.selected].key,
                     tui.config_editor.entries[tui.config_editor.selected].description,
                     /* Use stored value as both default and current for now */
                     tui.config_editor.entries[tui.config_editor.selected].value,
                     tui.config_editor.entries[tui.config_editor.selected].value);
            tui_history_add(MSG_ROLE_INFO, buf, false);
            tui_redraw_history();
            break;
        }

        case '/':
            tui.config_editor.search_mode = true;
            tui.config_editor.search[0] = '\0';
            break;
    }

    /* Adjust scroll */
    int vis_rows = tui.panes[PANE_HISTORY].rows - 5;
    if (tui.config_editor.selected < tui.config_editor.scroll_offset)
        tui.config_editor.scroll_offset = tui.config_editor.selected;
    if (tui.config_editor.selected >= tui.config_editor.scroll_offset + vis_rows)
        tui.config_editor.scroll_offset = tui.config_editor.selected - vis_rows + 1;

    return 0;
}
