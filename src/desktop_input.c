/* desktop_input.c -- extracted from src/app_desktop.c (angel-coder monolith split).
 * Self-contained desktop UI/PTY concern module. See app_desktop_internals.h.
 */

#include "app_desktop_internals.h"

void filter_palette_commands(void) {
    /* Already handled inline in ui_draw_command_palette — this is for
     * execute_palette_command which needs the filter state. */
    const struct { const char *cat, *label, *action; } cmds[] = {
        {"Navigate", "Chat", "view:chat"},
        {"Navigate", "Settings", "view:settings"},
        {"Sessions", "+ New Chat", "session:new"},
        {"Sessions", "Delete Chat", "session:delete"},
        {"Sessions", "Rename Chat", "session:rename"},
        {"Settings", "Model", "settings:model"},
        {"Actions",  "Keyboard Help", "action:keyboard-help"},
        {"Actions",  "Quit", "action:quit"},
    };
    int n = sizeof(cmds) / sizeof(cmds[0]);

    char q_lower[256];
    for (int i = 0; app.palette_query[i]; i++)
        q_lower[i] = tolower((unsigned char)app.palette_query[i]);
    q_lower[app.palette_query_len] = '\0';

    int fi = 0;
    for (int i = 0; i < n && fi < APP_MAX_PALETTE_CMDS; i++) {
        if (app.palette_query_len == 0) {
            app.palette_filtered_indices[fi] = i;
            strncpy(app.palette_labels[fi], cmds[i].label, 63);
            strncpy(app.palette_actions[fi], cmds[i].action, 63);
            fi++;
        } else {
            char h[256];
            snprintf(h, sizeof(h), "%s %s %s", cmds[i].cat, cmds[i].label, cmds[i].action);
            for (int c = 0; h[c]; c++) h[c] = tolower((unsigned char)h[c]);
            if (strstr(h, q_lower)) {
                app.palette_filtered_indices[fi] = i;
                strncpy(app.palette_labels[fi], cmds[i].label, 63);
                strncpy(app.palette_actions[fi], cmds[i].action, 63);
                fi++;
            }
        }
    }
    app.palette_cmd_count = n;
    app.palette_filtered_count = fi;
    if (app.palette_sel >= fi) app.palette_sel = fi - 1;
    if (app.palette_sel < 0) app.palette_sel = 0;
}

void execute_palette_command(const char *action) {
    if (!action) return;
    app.command_palette = false;
    ui.model_picker_active = false;
    app.settings_overlay = false;
    ui.keyboard_shortcuts = false;
    ui_destroy_overlay();

    if (strncmp(action, "view:", 5) == 0) {
        const char *v = action + 5;
        if (strcmp(v, "chat") == 0)           app.active_view = VIEW_CHAT;
        else if (strcmp(v, "settings") == 0)  { app.active_view = VIEW_SETTINGS; app_desktop_toggle_settings(&app); }
        else if (strcmp(v, "command-center")==0) app.active_view = VIEW_COMMAND_CENTER;
        else if (strcmp(v, "skills") == 0)    app.active_view = VIEW_SKILLS;
        else if (strcmp(v, "artifacts") == 0) app.active_view = VIEW_ARTIFACTS;
        else if (strcmp(v, "cron") == 0)      app.active_view = VIEW_CRON;
        else if (strcmp(v, "profiles") == 0)  app.active_view = VIEW_PROFILES;
        else if (strcmp(v, "agents") == 0)    app.active_view = VIEW_AGENTS;
        else if (strcmp(v, "messaging") == 0) app.active_view = VIEW_MESSAGING;
    }
    else if (strncmp(action, "session:", 8) == 0) {
        const char *op = action + 8;
        if (strcmp(op, "new") == 0)    app_desktop_create_session(&app);
        else if (strcmp(op, "delete")==0) app_desktop_delete_session(&app, app.session_ids[app.session_sel]);
        else if (strcmp(op, "rename")==0) app_desktop_rename_session_open(&app);
    }
    else if (strncmp(action, "settings:", 9) == 0) {
        const char *tab = action + 9;
        if (strcmp(tab, "model") == 0)         app.settings_tab = SETTINGS_TAB_MODEL;
        else if (strcmp(tab, "providers") == 0)app.settings_tab = SETTINGS_TAB_PROVIDERS;
        else if (strcmp(tab, "gateway") == 0)  app.settings_tab = SETTINGS_TAB_GATEWAY;
        else if (strcmp(tab, "notifications") == 0) app.settings_tab = SETTINGS_TAB_NOTIFICATIONS;
        else if (strcmp(tab, "profiles") == 0) app.settings_tab = SETTINGS_TAB_PROFILES;
        else if (strcmp(tab, "theme") == 0)    app.settings_tab = SETTINGS_TAB_THEME;
        else if (strcmp(tab, "keys") == 0)     app.settings_tab = SETTINGS_TAB_KEYS;
        else if (strcmp(tab, "about") == 0)    app.settings_tab = SETTINGS_TAB_ABOUT;
        app.settings_overlay = true;
        ui_create_overlay(ui.rows - 2, ui.cols - 6);
    }
    else if (strncmp(action, "action:", 7) == 0) {
        const char *a = action + 7;
        if (strcmp(a, "export") == 0) app_desktop_notify(&app, "Export not yet implemented", 3);
        else if (strcmp(a, "clear") == 0) { ui.rendered_count = 0; app_desktop_notify(&app, "Chat cleared", 2); }
        else if (strcmp(a, "reset") == 0) app_desktop_notify(&app, "Config reset not yet implemented", 3);
        else if (strcmp(a, "check-update") == 0) { app.update_available = false; app_desktop_notify(&app, "No updates available", 2); }
        else if (strcmp(a, "keyboard-help") == 0) {
            ui.keyboard_shortcuts = true;
            ui_create_overlay(ui.rows - 2, ui.cols - 8);
            return; /* don't auto-execute */
        }
        else if (strcmp(a, "quit") == 0) app.running = false;
    }
}

void ui_handle_dialog(int key) {
    if (app.delete_confirm) {
        if (key == 'y' || key == 'Y') {
            int idx = -1;
            for (int i = 0; i < app.session_count; i++)
                if (strcmp(app.session_ids[i], app.confirm_session_id) == 0) { idx = i; break; }
            if (idx >= 0) {
                for (int i = idx; i < app.session_count - 1; i++) {
                    strncpy(app.session_ids[i], app.session_ids[i + 1], 63);
                    strncpy(app.session_titles[i], app.session_titles[i + 1], 255);
                }
                app.session_count--;
                if (app.session_sel >= app.session_count && app.session_count > 0)
                    app.session_sel = app.session_count - 1;
                app_desktop_notify(&app, "Session deleted", 2);
            }
            app.delete_confirm = false;
            ui_destroy_dialog();
        } else if (key == 'n' || key == 'N' || key == 27 || key == 'q') {
            app.delete_confirm = false;
            ui_destroy_dialog();
        }
    } else if (app.rename_active) {
        if (key == '\n' || key == KEY_ENTER) {
            if (app.rename_buf[0] && app.session_sel >= 0 && app.session_sel < app.session_count) {
                strncpy(app.session_titles[app.session_sel], app.rename_buf, 255);
                app_desktop_notify(&app, "Session renamed", 2);
            }
            app.rename_active = false;
            ui_destroy_dialog();
        } else if (key == 27) {
            app.rename_active = false;
            ui_destroy_dialog();
        } else if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
            if (app.rename_len > 0) app.rename_buf[--app.rename_len] = '\0';
        } else if (key >= 32 && key < 127 && app.rename_len < (int)sizeof(app.rename_buf) - 1) {
            app.rename_buf[app.rename_len++] = (char)key;
            app.rename_buf[app.rename_len] = '\0';
        }
    }
    ui.dirty = true;
}

void ui_handle_overlay(int key) {
    /* Keyboard shortcut overlay — any key closes it */
    if (ui.keyboard_shortcuts) {
        ui.keyboard_shortcuts = false;
        ui_destroy_overlay();
        ui.dirty = true;
        return;
    }
    switch (key) {
    case 27: case 'q': case 'Q':
        ui.model_picker_active = false;
        app.settings_overlay = false;
        app.command_palette = false;
        ui.keyboard_shortcuts = false;
        ui_destroy_overlay();
        break;
    case '\t':
        if (app.settings_overlay)
            app.settings_tab = (settings_tab_t)((app.settings_tab + 1) % APP_SETTINGS_TABS);
        break;
    case KEY_LEFT:
        if (app.settings_overlay)
            app.settings_tab = (settings_tab_t)((app.settings_tab - 1 + APP_SETTINGS_TABS) % APP_SETTINGS_TABS);
        break;
    case KEY_RIGHT:
        if (app.settings_overlay)
            app.settings_tab = (settings_tab_t)((app.settings_tab + 1) % APP_SETTINGS_TABS);
        break;
    case '1': case '2': case '3':
        if (app.settings_overlay && app.settings_tab == SETTINGS_TAB_THEME) {
            app_desktop_set_theme(&app, (desktop_theme_t)(key - '1'));
        }
        break;
    case KEY_UP:
        if (ui.model_picker_active || app.command_palette)
            if (app.palette_sel > 0) app.palette_sel--;
        break;
    case KEY_DOWN:
        if (ui.model_picker_active) {
            if (app.palette_sel < app.model_count - 1) app.palette_sel++;
        } else if (app.command_palette) {
            if (app.palette_sel < app.palette_filtered_count - 1) app.palette_sel++;
        }
        break;
    case '\n': case KEY_ENTER:
        if (ui.model_picker_active) {
            if (app.palette_sel >= 0 && app.palette_sel < app.model_count) {
                strncpy(app.model, app.model_names[app.palette_sel], sizeof(app.model) - 1);
                app_desktop_notify(&app, "Model selected", 2);
            }
            ui.model_picker_active = false;
            ui_destroy_overlay();
        } else if (app.command_palette) {
            if (app.palette_sel >= 0 && app.palette_sel < app.palette_filtered_count) {
                const char *act = app.palette_actions[app.palette_sel];
                app.command_palette = false;
                ui_destroy_overlay();
                execute_palette_command(act);
            }
        } else if (ui.composer && ui.composer->autocomplete_visible &&
                   ui.composer->suggestion_selected >= 0) {
            /* Apply the selected autocomplete suggestion: replace the
             * word at the cursor with the suggestion text. */
            const composer_suggestion_t *sugg = NULL;
            int n = composer_get_suggestions(ui.composer, &sugg, COMPOSER_MAX_SUGGEST);
            int sel = ui.composer->suggestion_selected;
            if (sel >= 0 && sel < n) {
                /* Backspace the current word, then insert the suggestion. */
                int word_start = ui.composer->cursor_pos;
                while (word_start > 0 &&
                       !isspace((unsigned char)ui.composer->text[word_start - 1]))
                    word_start--;
                while (ui.composer->cursor_pos > word_start)
                    composer_backspace(ui.composer);
                composer_insert(ui.composer, sugg[sel].text);
                composer_update_suggestions(ui.composer);
            }
        }
        break;
    case KEY_BACKSPACE: case 127: case '\b':
        if (app.command_palette && app.palette_query_len > 0) {
            app.palette_query[--app.palette_query_len] = '\0';
        }
        break;
    default:
        if (app.command_palette && key >= 32 && key < 127 && app.palette_query_len < (int)sizeof(app.palette_query) - 1) {
            app.palette_query[app.palette_query_len++] = (char)key;
            app.palette_query[app.palette_query_len] = '\0';
        }
        break;
    }
    ui.dirty = true;
}

void ui_handle_normal(int key) {
    switch (key) {
    case 'q': case 'Q':
        if (ui.composer && composer_get_length(ui.composer) > 0) {
            composer_insert(ui.composer, "q");
        } else {
            app.running = false;
        }
        break;
    case '\t':
        /* With autocomplete visible, Tab cycles suggestions; otherwise it
         * toggles the sidebar (the Electron composer behavior). */
        if (ui.composer && ui.composer->autocomplete_visible) {
            composer_suggestion_next(ui.composer);
            break;
        }
        ui.sidebar_visible = !ui.sidebar_visible;
        ui_resize();
        break;
    case 't':
        ui.terminal_visible = !ui.terminal_visible;
        if (ui.terminal_visible) term_launch_pty();
        ui_resize();
        break;
    case 'y': {
        /* Copy last assistant message to clipboard */
        if (ui.rendered_count > 0) {
            /* Find last assistant message */
            for (int i = ui.rendered_count - 1; i >= 0; i--) {
                if (ui.rendered_msgs[i] && ui.rendered_msgs[i]->role[0] == 'a' &&
                    ui.rendered_msgs[i]->raw[0]) {
                    char *plain = chat_render_plain_text(ui.rendered_msgs[i]);
                    if (plain && *plain) {
                        clipboard_write_text(plain);
                        app_desktop_notify(&app, "Copied to clipboard", 2);
                    }
                    free(plain);
                    break;
                }
            }
        }
        break;
    }
    case 'i': {
        /* Insert from clipboard to composer */
        char *clip = clipboard_read_text();
        if (clip && *clip) {
            if (ui.composer) {
                composer_insert(ui.composer, clip);
                app_desktop_notify(&app, "Pasted from clipboard", 2);
            }
            free(clip);
        } else {
            app_desktop_notify(&app, "Clipboard empty", 2);
        }
        break;
    }
    case 's':
        app_desktop_toggle_settings(&app);
        break;
    case 'p': case 'P':
        ui.model_picker_active = true;
        app.palette_sel = 0;
        ui_create_overlay(ui.rows - 4, 40);
        break;
    case ':':
        app.command_palette = true;
        app.palette_query[0] = '\0';
        app.palette_query_len = 0;
        app.palette_sel = 0;
        filter_palette_commands();
        ui_create_overlay(ui.rows - 2, 52);
        break;
    case 'n': case 'N':
        if (ui.composer && composer_get_length(ui.composer) == 0) {
            app_desktop_create_session(&app);
        } else {
            composer_insert(ui.composer, "n");
        }
        break;
    case 'r':
        if (ui.composer && composer_get_length(ui.composer) == 0) {
            app_desktop_rename_session_open(&app);
        } else {
            composer_insert(ui.composer, "r");
        }
        break;
    case 'd':
        if (app.session_count > 0 && app.session_sel >= 0 && app.session_sel < app.session_count) {
            app_desktop_delete_session(&app, app.session_ids[app.session_sel]);
        }
        break;
    case KEY_F(1):
    case '?':
        ui.keyboard_shortcuts = true;
        ui_create_overlay(ui.rows - 2, ui.cols - 8);
        break;
    case '/':
        /* Activate sidebar search */
        ui.sidebar_search_active = true;
        ui.sidebar_search[0] = '\0';
        ui.sidebar_search_len = 0;
        break;
    case KEY_UP:
        if (app.sidebar_section == 0) { if (app.session_sel > 0) app.session_sel--; }
        else { if (app.sidebar_sel > 0) app.sidebar_sel--; }
        break;
    case KEY_DOWN:
        if (app.sidebar_section == 0) { if (app.session_sel < app.session_count) app.session_sel++; }
        else { int nc = 9; if (app.sidebar_sel < nc - 1) app.sidebar_sel++; }
        break;
    case KEY_LEFT:
        if (app.sidebar_section > 0) app.sidebar_section = 0;
        break;
    case KEY_RIGHT:
        if (app.sidebar_section == 0) { app.sidebar_section = 1; app.sidebar_sel = 0; }
        break;
    case KEY_PPAGE:
        ui.scroll_offset -= 10;
        if (ui.scroll_offset < 0) ui.scroll_offset = 0;
        break;
    case KEY_NPAGE:
        ui.scroll_offset += 10;
        break;
    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK && ev.bstate & BUTTON1_CLICKED) {
            /* Convert to terminal coordinates */
            int my = ev.y - 0;  /* relative to stdscr */
            int mx = ev.x - 0;
            
            /* Titlebar (row 0) — no action */
            if (my == 0) break;
            
            /* Statusbar (last row) — no action */
            if (my >= ui.rows - 1) break;
            
            /* Sidebar clicks */
            int sb_top = APP_TITLEBAR_HEIGHT;
            int sb_h = ui.rows - APP_TITLEBAR_HEIGHT - APP_STATUSBAR_HEIGHT;
            if (ui.sidebar_visible && mx < ui.sidebar_width && my >= sb_top && my < sb_top + sb_h) {
                int rel_y = my - sb_top;  /* y relative to sidebar top */
                int row = 0;
                
                /* Header (2 rows) */
                row += 2;
                
                /* Search bar */
                if (rel_y >= row && rel_y < row + 1) {
                    ui.sidebar_search_active = true;
                    ui.sidebar_search[0] = '\0';
                    ui.sidebar_search_len = 0;
                    break;
                }
                row += 1;
                
                /* "Sessions" section header (1 row) */
                row += 1;
                
                /* Session items: each session = 2 rows (title + metadata) */
                int shown = 0;
                for (int i = 0; i < app.session_count; i++) {
                    /* Check search filter */
                    if (ui.sidebar_search_active && ui.sidebar_search_len > 0) {
                        char lower_t[256], lower_q[128];
                        strncpy(lower_t, app.session_titles[i], 255);
                        for (int c = 0; lower_t[c]; c++) lower_t[c] = tolower((unsigned char)lower_t[c]);
                        strncpy(lower_q, ui.sidebar_search, 127);
                        for (int c = 0; lower_q[c]; c++) lower_q[c] = tolower((unsigned char)lower_q[c]);
                        if (!strstr(lower_t, lower_q)) continue;
                    }
                    shown++;
                    if (rel_y >= row && rel_y < row + 2) {
                        /* Clicked on this session */
                        app.sidebar_section = 0;
                        app.session_sel = i;
                        break;
                    }
                    row += 2;
                }
                
                /* "+ New Chat" */
                if (shown > 0 || true) {
                    if (rel_y >= row && rel_y < row + 1) {
                        /* Click +New Chat */
                        app_desktop_create_session(&app);
                        break;
                    }
                    /* If "No sessions found" was shown instead, just skip */
                    row += 1;
                }
                
                /* Blank row spacer */
                row += 1;
                
                /* "Navigation" section header */
                row += 1;
                
                /* Nav items */
                int nav_click = rel_y - row;
                if (nav_click >= 0 && nav_click < 9) {
                    app.sidebar_section = 1;
                    app.sidebar_sel = nav_click;
                    /* Also switch active view */
                    app_view_t views[] = {
                        VIEW_CHAT, VIEW_SETTINGS, VIEW_COMMAND_CENTER,
                        VIEW_SKILLS, VIEW_ARTIFACTS, VIEW_CRON,
                        VIEW_PROFILES, VIEW_AGENTS, VIEW_MESSAGING
                    };
                    if (nav_click < 9) {
                        app.active_view = views[nav_click];
                        if (nav_click == 1) {  /* Settings */
                            app_desktop_toggle_settings(&app);
                        }
                    }
                }
                break;
            }
            
            /* Chat area clicks — focus composer */
            if (mx >= (ui.sidebar_visible ? ui.sidebar_width : 0)) {
                /* Click in chat — no action needed, just acknowledge */
            }
        }
        break;
    }
    case KEY_RESIZE:
        ui_resize();
        break;
    case KEY_BACKSPACE: case 127: case '\b':
        if (ui.sidebar_search_active && ui.sidebar_search_len > 0) {
            ui.sidebar_search[--ui.sidebar_search_len] = '\0';
        }
        break;
    default:
        /* Terminal PTY input: send keystrokes to PTY when terminal is visible */
        if (ui.terminal_visible && app.term_pty && app.term_pty->active) {
            char c = (char)key;
            if (key >= 32 && key < 127) {
                term_write_pty(&c, 1);
            } else if (key == 13 || key == 10) {
                term_write_pty("\r", 1);
            }
            break;
        }
        /* Check for sidebar search input */
        if (ui.sidebar_search_active && key >= 32 && key < 127 &&
            ui.sidebar_search_len < (int)sizeof(ui.sidebar_search) - 1) {
            if (key == 27) { ui.sidebar_search_active = false; break; }
            ui.sidebar_search[ui.sidebar_search_len++] = (char)key;
            ui.sidebar_search[ui.sidebar_search_len] = '\0';
        } else if (ui.composer && key >= 32 && key < 127) {
            char ch[2] = { (char)key, '\0' };
            composer_insert(ui.composer, ch);
            /* Refresh autocomplete suggestions after every input. */
            composer_update_suggestions(ui.composer);
        }
        break;
    }
}
