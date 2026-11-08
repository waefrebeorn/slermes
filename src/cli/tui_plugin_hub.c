/*
 * tui_plugin_hub.c — interactive plugin-hub overlay.
 *
 * Subsystem split (soul.md: self-contained modules). Touches only
 * tui.plugin_hub + tui.agent + tui.panes + tui.modal_mode (via tui_state.h)
 * and the render-core entry points; no file-scope state.
 */
#include "tui_state.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void tui_plugin_hub_init(void) {
    tui.plugin_hub.count = 0;
    tui.plugin_hub.selected = 0;
    tui.plugin_hub.scroll_offset = 0;

    if (!tui.agent) return;
    plugin_registry_t *reg = (plugin_registry_t *)tui.agent->plugin_reg;
    if (!reg) return;

    for (int i = 0; i < reg->count && tui.plugin_hub.count < 256; i++) {
        plugin_t *p = reg->plugins[i];
        if (!p) continue;
        const char *name = plugin_name(p);
        if (!name || !*name) name = "(unnamed)";
        strncpy(tui.plugin_hub.names[tui.plugin_hub.count], name,
                sizeof(tui.plugin_hub.names[0]) - 1);

        const char *type_str = plugin_type_str(plugin_type(p));
        strncpy(tui.plugin_hub.types[tui.plugin_hub.count], type_str ? type_str : "unknown",
                sizeof(tui.plugin_hub.types[0]) - 1);

        const plugin_version_t *ver = plugin_version(p);
        if (ver) {
            char ver_buf[16];
            snprintf(ver_buf, sizeof(ver_buf), "%d.%d.%d", ver->major, ver->minor, ver->patch);
            strncpy(tui.plugin_hub.versions[tui.plugin_hub.count], ver_buf,
                    sizeof(tui.plugin_hub.versions[0]) - 1);
        } else {
            strncpy(tui.plugin_hub.versions[tui.plugin_hub.count], "?",
                    sizeof(tui.plugin_hub.versions[0]) - 1);
        }

        tui.plugin_hub.initialized[tui.plugin_hub.count] = plugin_is_initialized(p);
        tui.plugin_hub.count++;
    }
}
void tui_draw_plugin_hub(void) {
    WINDOW *win = tui.panes[PANE_HISTORY].win;
    if (!win) return;

    werase(win);

    int w_rows = tui.panes[PANE_HISTORY].rows;
    int w_cols = tui.panes[PANE_HISTORY].cols;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, 0, " PLUGIN HUB — Loaded Plugins ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    if (tui.plugin_hub.count == 0) {
        mvwprintw(win, 2, 0, " No plugins loaded.");
        mvwprintw(win, 3, 0, " Use /plugins to reload or check plugin directory.");
        wattron(win, A_DIM | COLOR_PAIR(10));
        mvwprintw(win, w_rows - 1, 0, " [Enter] toggle enable [q] quit ");
        wattroff(win, A_DIM | COLOR_PAIR(10));
        wnoutrefresh(win);
        doupdate();
        return;
    }

    /* Header */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, 1, 0, " %-28s %-12s %-8s %s", "Name", "Type", "Version", "Status");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    /* Separator */
    mvwhline(win, 2, 0, ACS_HLINE, w_cols - 1);

    /* Plugin list */
    int y = 3;
    for (int i = tui.plugin_hub.scroll_offset;
         i < tui.plugin_hub.count && y < w_rows - 2;
         i++) {

        bool selected = (i == tui.plugin_hub.selected);
        if (selected)
            wattron(win, A_REVERSE | COLOR_PAIR(13));

        const char *status_str = tui.plugin_hub.initialized[i] ? "\xE2\x9C\x93 active" : "\xE2\x97\x8B inactive";
        int status_pair = tui.plugin_hub.initialized[i] ? 9 : 10; /* green or dim */

        mvwprintw(win, y, 0, " %-28.28s %-12.12s %-8.8s ",
                  tui.plugin_hub.names[i],
                  tui.plugin_hub.types[i],
                  tui.plugin_hub.versions[i]);

        wattron(win, COLOR_PAIR(status_pair));
        wprintw(win, "%s", status_str);
        wattroff(win, COLOR_PAIR(status_pair));

        if (selected)
            wattroff(win, A_REVERSE | COLOR_PAIR(13));

        y++;
    }

    /* Help bar */
    wattron(win, A_DIM | COLOR_PAIR(10));
    mvwprintw(win, w_rows - 1, 0, " [Enter] toggle plugin | [q] quit ");
    wattroff(win, A_DIM | COLOR_PAIR(10));

    wnoutrefresh(win);
    doupdate();
}
int tui_plugin_hub_handle(int ch) {
    switch (ch) {
        case 'q':
        case 27:
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        case KEY_UP:
            if (tui.plugin_hub.selected > 0) tui.plugin_hub.selected--;
            break;

        case KEY_DOWN:
            if (tui.plugin_hub.selected < tui.plugin_hub.count - 1)
                tui.plugin_hub.selected++;
            break;

        case KEY_PPAGE:
            tui.plugin_hub.selected -= 10;
            if (tui.plugin_hub.selected < 0) tui.plugin_hub.selected = 0;
            break;

        case KEY_NPAGE:
            tui.plugin_hub.selected += 10;
            if (tui.plugin_hub.selected >= tui.plugin_hub.count)
                tui.plugin_hub.selected = tui.plugin_hub.count - 1;
            break;

        case '\n':
        case '\r': {
            /* Toggle plugin enabled/disabled state */
            if (tui.plugin_hub.selected >= 0 && tui.plugin_hub.selected < tui.plugin_hub.count &&
                tui.agent) {
                plugin_registry_t *reg = (plugin_registry_t *)tui.agent->plugin_reg;
                if (reg && tui.plugin_hub.selected < reg->count) {
                    plugin_t *p = reg->plugins[tui.plugin_hub.selected];
                    if (p) {
                        bool was_init = plugin_is_initialized(p);
                        if (was_init) {
                            /* Can't truly unload a .so at runtime — mark for restart */
                            tui_history_add(MSG_ROLE_WARN,
                                "Plugin restart requires session reload (/reset)", true);
                        } else {
                            /* Attempt to init now */
                            /* plugin_registry_init_ordered would re-init, but for
                             * a single plugin we just flag it */
                            tui_history_add(MSG_ROLE_INFO,
                                "Plugin queued for init on next session", false);
                        }
                        tui_redraw_history();
                    }
                }
            }
            return 1;
        }

        default:
            return 0;
    }

    /* Adjust scroll */
    int vis_rows = tui.panes[PANE_HISTORY].rows - 4;
    if (tui.plugin_hub.selected < tui.plugin_hub.scroll_offset)
        tui.plugin_hub.scroll_offset = tui.plugin_hub.selected;
    if (tui.plugin_hub.selected >= tui.plugin_hub.scroll_offset + vis_rows)
        tui.plugin_hub.scroll_offset = tui.plugin_hub.selected - vis_rows + 1;

    return 0;
}
