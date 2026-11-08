/*
 * tui_model_picker.c — interactive model-selection overlay.
 *
 * Subsystem split (soul.md: self-contained modules). Touches only
 * tui.model_picker + tui.status + tui.agent + tui.modal_mode (via
 * tui_state.h) and the render-core entry points; no file-scope state.
 */
#include "tui_state.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void tui_model_picker_init(void) {
    tui.model_picker.count = 0;
    tui.model_picker.selected = 0;
    tui.model_picker.scroll_offset = 0;

    char *json = model_metadata_list_json();
    if (!json) return;

    json_t *root = json_parse(json, NULL);
    free(json);
    if (!root || root->type != JSON_ARRAY) {
        json_free(root);
        return;
    }
    for (size_t i = 0; i < root->c.count && tui.model_picker.count < MODEL_PICKER_MAX; i++) {
        json_t *entry = root->c.items[i];
        if (!entry) continue;
        json_t *name = json_obj_get(entry, "model");
        if (name && name->type == JSON_STRING && name->str_val && name->str_val[0]) {
            snprintf(tui.model_picker.names[tui.model_picker.count],
                     MODEL_NAME_MAX, "%s", name->str_val);
            tui.model_picker.count++;
        }
    }
    json_free(root);

    /* Find current model in list and pre-select it */
    const char *current = tui.status.model[0] ? tui.status.model : "";
    for (int i = 0; i < tui.model_picker.count; i++) {
        if (strcmp(tui.model_picker.names[i], current) == 0) {
            tui.model_picker.selected = i;
            break;
        }
    }
}
void tui_draw_model_picker(void) {
    int win_height = 18;
    int win_width = 50;
    int start_y = (tui.rows - win_height) / 2;
    int start_x = (tui.cols - win_width) / 2;
    if (start_y < 0) start_y = 0;
    if (start_x < 0) start_x = 0;
    if (win_height > tui.rows) win_height = tui.rows;
    if (win_width > tui.cols) win_width = tui.cols;

    WINDOW *win = newwin(win_height, win_width, start_y, start_x);
    if (!win) return;

    /* Box with title */
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " Model Picker ");
    mvwprintw(win, 1, 2, "\\u2191\\u2193 select | Enter apply | q/ESC close");

    /* Visible rows */
    int visible = win_height - 4;  /* header + help + footer */
    if (visible < 1) visible = 1;

    /* Clamp scroll_offset */
    if (tui.model_picker.selected < tui.model_picker.scroll_offset)
        tui.model_picker.scroll_offset = tui.model_picker.selected;
    if (tui.model_picker.selected >= tui.model_picker.scroll_offset + visible)
        tui.model_picker.scroll_offset = tui.model_picker.selected - visible + 1;
    if (tui.model_picker.scroll_offset > tui.model_picker.count - visible)
        tui.model_picker.scroll_offset = tui.model_picker.count - visible;
    if (tui.model_picker.scroll_offset < 0)
        tui.model_picker.scroll_offset = 0;

    const char *current = tui.status.model[0] ? tui.status.model : "";

    for (int i = 0; i < visible && (tui.model_picker.scroll_offset + i) < tui.model_picker.count; i++) {
        int idx = tui.model_picker.scroll_offset + i;
        bool is_selected = (idx == tui.model_picker.selected);
        bool is_current = (strcmp(tui.model_picker.names[idx], current) == 0);
        char line[MODEL_NAME_MAX + 16];
        if (is_current)
            snprintf(line, sizeof(line), " * %s (active)", tui.model_picker.names[idx]);
        else
            snprintf(line, sizeof(line), "   %s", tui.model_picker.names[idx]);
        if (is_selected)
            wattron(win, A_REVERSE);
        mvwprintw(win, 3 + i, 2, "%-*s", win_width - 4, line);
        if (is_selected)
            wattroff(win, A_REVERSE);
    }

    /* Footer */
    mvwprintw(win, win_height - 2, 2, "Models: %d  Current: %s",
              tui.model_picker.count, current);

    wrefresh(win);
    delwin(win);
    (void)visible;
    (void)current;
}
int tui_model_picker_handle(int ch) {
    switch (ch) {
        case KEY_UP:
        case 'k':
            if (tui.model_picker.selected > 0)
                tui.model_picker.selected--;
            return 1;

        case KEY_DOWN:
        case 'j':
            if (tui.model_picker.selected < tui.model_picker.count - 1)
                tui.model_picker.selected++;
            return 1;

        case KEY_PPAGE:
            tui.model_picker.selected -= 10;
            if (tui.model_picker.selected < 0)
                tui.model_picker.selected = 0;
            return 1;

        case KEY_NPAGE:
            tui.model_picker.selected += 10;
            if (tui.model_picker.selected >= tui.model_picker.count)
                tui.model_picker.selected = tui.model_picker.count - 1;
            return 1;

        case KEY_HOME:
            tui.model_picker.selected = 0;
            return 1;

        case KEY_END:
            tui.model_picker.selected = tui.model_picker.count - 1;
            return 1;

        case '\n':
        case '\r': {
            /* Apply selected model */
            if (tui.model_picker.selected >= 0 && tui.model_picker.selected < tui.model_picker.count) {
                const char *name = tui.model_picker.names[tui.model_picker.selected];
                if (tui.agent) {
                    snprintf(tui.agent->llm.model, sizeof(tui.agent->llm.model), "%s", name);
                }
                snprintf(tui.status.model, sizeof(tui.status.model), "%s", name);
                tui.status.dirty = true;
                char msg[256];
                snprintf(msg, sizeof(msg), "Model set to: %s", name);
                tui_history_add(MSG_ROLE_INFO, msg, false);
            }
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            tui_redraw_status();
            return 1;
        }

        case 'q':
        case 27: /* ESC */
            tui.modal_mode = MODE_NORMAL;
            tui_redraw_history();
            return 1;

        default:
            return 0;
    }
}
