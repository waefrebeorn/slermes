/*
 * event_handling.c — Main Event Loop and Input Processing
 *
 * Handles all input events and dispatches to appropriate modules.
 */

#define _GNU_SOURCE
#include "event_handling.h"
#include "app_state_internal.h"
#include "chat_composer.h"
#include "sidebar.h"
#include "chat_view.h"
#include "titlebar.h"
#include "session_db.h"
#include "gui_core.h"
#include "pet_ui.h"
#include "session_switcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════
 * Hit Testing
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct { int type; int index; } hit_t;

static hit_t hit_test(app_state_t *app, int mx, int my) {
    hit_t r = { HIT_NONE, -1 };
    if (!app || !app_get_window(app)) return r;
    
    gc_window_t *win = app_get_window(app);
    int w = gc_window_w(win);
    int h = gc_window_h(win);
    int cw = app_chat_w(app);
    
    /* Titlebar */
    if (my < TITLEBAR_H) {
        int tx = w - 16 - 20;
        for (int i = 3; i >= 0; i--) {
            if (mx >= tx && mx < tx + 20 && my >= 8 && my < 28) {
                r.type = HIT_TOOL; r.index = i; return r;
            }
            tx -= 24;
        }
        r.type = HIT_TITLEBAR; return r;
    }
    
    if (my >= h - STATUSBAR_H) { r.type = HIT_STATUSBAR; return r; }
    
    /* Composer / Model pill */
    int comp_y = h - STATUSBAR_H - COMPOSER_H - 12;
    if (mx >= app_chat_x(app) + 20 && mx < app_chat_x(app) + cw - 20 && 
        my >= comp_y && my < comp_y + COMPOSER_H) {
        r.type = HIT_COMPOSER; return r;
    }
    /* Model pill above composer */
    if (mx >= app_chat_x(app) + 24 && mx < app_chat_x(app) + cw - 24 && 
        my >= comp_y - PILL_H - 6 && my < comp_y - 2) {
        r.type = HIT_PILL; return r;
    }
    
    /* Sidebar */
    if (mx < app_chat_x(app)) {
        int y = TITLEBAR_H;
        
        /* Search bar */
        if (my >= y + 8 && my < y + 8 + SEARCH_H) { r.type = HIT_SEARCH; return r; }
        y += 8 + SEARCH_H + 8;
        
        /* SESSIONS section header */
        if (my >= y && my < y + SECTION_H + 4) { r.type = HIT_SESSIONS_HDR; return r; }
        y += SECTION_H + 4;
        
        /* Session items */
        if (app_sessions_expanded(app)) {
            for (int i = 0; i < app_session_count(app); i++) {
                if (my >= y && my < y + ITEM_H) { r.type = HIT_SESSION; r.index = i; return r; }
                y += ITEM_H;
            }
        } else {
            y += ITEM_H;
        }
        
        y += 4;
        
        /* +New Chat */
        if (my >= y && my < y + 26) { r.type = HIT_NEWCHAT; return r; }
        y += 26 + 12;
        
        /* NAVIGATION section header */
        if (my >= y && my < y + SECTION_H + 4) { r.type = HIT_NAV_HDR; return r; }
        y += SECTION_H + 4;
        
        if (app_nav_expanded(app)) {
            for (int i = 0; i < app_nav_item_count(); i++) {
                if (my >= y && my < y + ITEM_H) { r.type = HIT_NAV; r.index = i; return r; }
                y += ITEM_H;
            }
        } else {
            if (my >= y && my < y + ITEM_H) { r.type = HIT_NAV; r.index = app_selected_nav(app); return r; }
            y += ITEM_H;
        }
        
        /* Profile at bottom */
        int pr_y = TITLEBAR_H + app_sidebar_h(app) - ITEM_H - 14;
        int pr_h = ITEM_H + 10;
        if (my >= pr_y && my < pr_y + pr_h) { r.type = HIT_PROFILE; return r; }
        
        return r;
    }
    
    r.type = HIT_CHAT;
    return r;
}

/* Public hover updater (declared in app_state.h; lives here with hit_test). */
void app_update_hover(app_state_t *app, int mx, int my) {
    if (!app) return;
    hit_t hit = hit_test(app, mx, my);
    if (hit.type == HIT_SESSION) {
        app_set_hover_message(app, hit.index);
    } else if (hit.type == HIT_CHAT) {
        chat_view_handle_hover(app, mx, my);
    } else {
        app_set_hover_message(app, -1);
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Event Processing
 * ══════════════════════════════════════════════════════════════════════ */

bool event_process(app_state_t *app, gc_event_t *ev) {
    if (!app || !ev) return false;
    
    switch (ev->type) {
        case GC_EV_QUIT:
            app->running = false;
            return true;
            
        case GC_EV_KEY_DOWN:
            return event_handle_key(app, ev->key, ev->mod);
            
        case GC_EV_TEXT_INPUT:
            return event_handle_text(app, ev->text);
            
        case GC_EV_MOUSE_MOVE:
            return event_handle_mouse(app, ev);
            
        case GC_EV_MOUSE_DOWN:
            return event_handle_mouse(app, ev);
            
        case GC_EV_MOUSE_UP:
            return event_handle_mouse(app, ev);
            
        case GC_EV_MOUSE_WHEEL:
            {
                hit_t hit = hit_test(app, ev->x, ev->y);
                if (hit.type == HIT_NAV || hit.type == HIT_SESSION || hit.type == HIT_SESSIONS_HDR || 
                    hit.type == HIT_NAV_HDR || hit.type == HIT_NEWCHAT || hit.type == HIT_PROFILE ||
                    hit.type == HIT_SEARCH) {
                    sidebar_handle_wheel(app, ev->wheel_delta);
                } else if (hit.type == HIT_CHAT) {
                    chat_view_handle_wheel(app, ev->wheel_delta);
                }
            }
            return true;
            
        case GC_EV_RESIZE:
            event_handle_resize(app, ev->resize_w, ev->resize_h);
            return true;
            
        default:
            break;
    }
    
    return false;
}

void event_run(app_state_t *app) {
    if (!app) return;
    
    gc_event_t ev;
    bool shot_done = false;
    bool switcher_done = false;
    const char *shot_path = getenv("SLERMES_GUI_SCREENSHOT");
    while (app->running) {
        gc_begin_frame(app_get_window(app));
        
        /* Update pet animation + derive state from live app signals
         * (busy/api state, composer input). */
        pet_ui_derive_state(app);
        pet_ui_update_animation(app);
        
        /* Poll and process events */
        while (gc_poll_event(app_get_window(app), &ev)) {
            event_process(app, &ev);
        }
        
        /* Draw UI */
        titlebar_draw(app);
        sidebar_draw(app);
        chat_view_draw(app);
        statusbar_draw(app);
        pet_ui_draw(app);
        session_switcher_draw(app);

        /* One-shot screenshot capture for headless verification
         * (SLERMES_GUI_SCREENSHOT=/path/out.bmp) — must run AFTER the draw
         * calls (gc_begin_frame clears the backbuffer). */
        if (shot_path && !shot_done && app->frame_count >= 30) {
            gc_save_screenshot(app_get_window(app), shot_path);
            shot_done = true;
        }
        /* Headless switcher demo (SLERMES_GUI_SWITCHER=1) — open the HUD
         * after a few frames so screenshots can capture it. */
        if (getenv("SLERMES_GUI_SWITCHER") && !switcher_done && app->frame_count >= 10) {
            session_switcher_open(app);
            switcher_done = true;
        }
        /* Headless preview demo (SLERMES_GUI_PREVIEW=1) — open the
         * right-rail preview pane so screenshots can capture it. */
        if (getenv("SLERMES_GUI_PREVIEW") && !app_show_preview(app) && app->frame_count >= 5) {
            app_set_preview(app, "File Preview",
                "This is the right-rail preview pane.\nIt mirrors the Electron "
                "app's side-by-side preview: select a session or message to "
                "preview its content here.\n\nPreview content wraps at the panel "
                "edge and updates live as you navigate.");
            app_set_show_preview(app, true);
        }
        app->frame_count++;
        
        gc_end_frame(app_get_window(app));
    }
}

void event_handle_resize(app_state_t *app, int w, int h) {
    if (!app || !app_get_window(app)) return;
    /* Window size is managed by SDL, just trigger redraw */
    (void)w; (void)h;
}
bool event_handle_key(app_state_t *app, int key, int mod) {
    if (!app) return false;
    
    /* Session switcher HUD consumes keys while open (except its own
     * Ctrl+Tab cycle, handled in the switch below). */
    if (session_switcher_visible(app) &&
        session_switcher_handle_key(app, key, mod)) {
        return true;
    }
    
    /* Global hotkeys */
    switch (key) {
        case SDLK_ESCAPE:
            if (session_switcher_visible(app)) {
                session_switcher_close(app);
                return true;
            }
            if (app_pet_show_gallery(app)) {
                app_set_pet_show_gallery(app, false);
                return true;
            }
            if (app_show_model_picker(app)) {
                app_set_show_model_picker(app, false);
                return true;
            }
            if (app_show_session_menu(app)) {
                app_set_show_session_menu(app, false);
                return true;
            }
            if (app_composer_focused(app)) {
                app_set_composer_focused(app, false);
                return true;
            }
            if (app_search_active(app)) {
                app_set_search_active(app, false);
                return true;
            }
            app->running = false;
            return true;
            
        case SDLK_t:
            if (mod & KMOD_CTRL) {
                app_toggle_theme(app);
                return true;
            }
            break;

        case SDLK_r:
            if (mod & KMOD_CTRL) {
                /* Toggle the right-rail preview pane (v54x parity). */
                app_set_show_preview(app, !app_show_preview(app));
                if (app_show_preview(app) && !app_preview_title(app)[0]) {
                    app_set_preview(app, "Preview",
                        "Select a session or message to preview its content here.\n"
                        "The right rail mirrors the Electron app's side-by-side preview.");
                }
                return true;
            }
            break;
            
        case SDLK_b:
            if (mod & KMOD_CTRL) {
                app_toggle_sidebar(app);
                return true;
            }
            break;
            
        case SDLK_n:
            if (mod & KMOD_CTRL) {
                session_db_create_session(app, "New Chat", "cli", "");
                session_db_load_sessions(app);
                return true;
            }
            break;

        case SDLK_s:
            if (mod & KMOD_CTRL) {
                /* Export current session (Ctrl+S, v479 parity). */
                int sel = app_selected_session(app);
                if (sel >= 0 && sel < app_session_count(app)) {
                    app_session_entry_t *s = app_get_session(app, sel);
                    if (s) {
                        char out_path[1024];
                        snprintf(out_path, sizeof(out_path), "%s/export-%s.json",
                                 getenv("HOME") ? getenv("HOME") : "/tmp", s->id);
                        extern bool desktop_session_export(const char *id,
                                                          const char *path,
                                                          const char *format);
                        if (desktop_session_export(s->id, out_path, "json")) {
                            printf("Session exported to %s\n", out_path);
                        } else {
                            fprintf(stderr, "export failed\n");
                        }
                    }
                }
                return true;
            }
            break;

        case SDLK_i:
            if (mod & KMOD_CTRL) {
                /* Import session (Ctrl+I, v479 parity). */
                extern char *desktop_file_dialog_open(const char *title,
                                                      const char *filter);
                extern char *desktop_session_import(const char *path);
                char *path = desktop_file_dialog_open("Import Session", "*.json");
                if (path && *path) {
                    char *new_id = desktop_session_import(path);
                    if (new_id) {
                        printf("Session imported: %s\n", new_id);
                        free(new_id);
                        session_db_load_sessions(app);
                    } else {
                        fprintf(stderr, "import failed\n");
                    }
                    free(path);
                } else {
                    fprintf(stderr, "import cancelled\n");
                }
                return true;
            }
            break;

        case SDLK_o:
            if (mod & KMOD_CTRL) {
                /* Attach a file (Ctrl+O, paperclip parity): the file dialog
                 * picks a path, which is inserted into the composer as the
                 * attachment marker (matches the Electron paperclip flow). */
                extern char *desktop_file_dialog_open(const char *title,
                                                      const char *filter);
                char *path = desktop_file_dialog_open("Attach a file", NULL);
                if (path && path[0]) {
                    /* Append " [attach:<path>]" to the composer buffer. */
                    char buf[4600];
                    snprintf(buf, sizeof(buf), "%s [attach:%s]",
                             app_composer_buf(app), path);
                    app_set_composer_buf(app, buf);
                    app_set_composer_pos(app, (int)strlen(buf));
                    app_set_composer_focused(app, true);
                    app_set_api_status(app, "File attached");
                }
                free(path);
                return true;
            }
            break;
            
        case SDLK_f:
            if (mod & KMOD_CTRL) {
                app_set_search_active(app, true);
                app_set_composer_focused(app, false);
                return true;
            }
            break;
            
        case SDLK_k:
            if (mod & KMOD_CTRL) {
                app_set_chat_scroll(app, app_chat_scroll(app) - 30);
                return true;
            }
            break;
            
        case SDLK_j:
            if (mod & KMOD_CTRL) {
                app_set_chat_scroll(app, app_chat_scroll(app) + 30);
                return true;
            }
            break;
            
        case SDLK_p:
            if (mod & KMOD_CTRL) {
                /* Pet gallery toggle (Ctrl+P) */
                if (app_pet_active(app)) {
                    app_set_pet_show_gallery(app, !app_pet_show_gallery(app));
                    return true;
                }
            }
            break;
            
        case SDLK_g:
            if (mod & KMOD_SHIFT) {
                app_set_chat_scroll(app, 0);
                return true;
            }
            break;
            
        case SDLK_m:
            if (mod & KMOD_CTRL) {
                app_set_show_model_picker(app, !app_show_model_picker(app));
                return true;
            }
            break;
            
        case SDLK_TAB:
            /* Ctrl+Tab / Ctrl+Shift+Tab: session switcher HUD. */
            if (mod & KMOD_CTRL) {
                if (session_switcher_visible(app)) {
                    session_switcher_cycle(app, (mod & KMOD_SHIFT) ? -1 : 1);
                } else {
                    session_switcher_open(app);
                }
                return true;
            }
            /* Plain Tab: cycle through focus areas */
            if (app_composer_focused(app)) {
                app_set_composer_focused(app, false);
                app_set_search_active(app, true);
            } else if (app_search_active(app)) {
                app_set_search_active(app, false);
                app_set_composer_focused(app, true);
            } else {
                app_set_composer_focused(app, true);
            }
            return true;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (app_composer_focused(app)) {
                /* Send: append the user message to the session + persist. */
                const char *text = app_composer_buf(app);
                if (text && text[0]) {
                    int sel = app_selected_session(app);
                    if (sel >= 0 && sel < app_session_count(app)) {
                        app_session_entry_t *s = session_db_get_session(app, sel);
                        char sid[64] = "";
                        if (s) snprintf(sid, sizeof(sid), "%s", s->id);
                        /* Append locally. */
                        extern int session_db_insert_message(const char *session_id,
                                                             const char *role,
                                                             const char *content,
                                                             double timestamp);
                        if (sid[0]) {
                            session_db_insert_message(sid, "user", text,
                                                      (double)time(NULL));
                        }
                        /* Reload so the message appears. */
                        session_db_load_messages(app, sel);
                        app_set_chat_scroll(app, 0);
                        chat_view_scroll_to_bottom(app);
                    }
                    app_set_composer_buf(app, "");
                    app_set_composer_pos(app, 0);
                }
                return true;
            }
            break;
    }
    
    /* Delegate to chat view */
    if (chat_view_handle_key(app, key, mod, NULL)) {
        return true;
    }
    
    return false;
}

bool event_handle_text(app_state_t *app, const char *text) {
    if (!app || !text) return false;
    
    /* Handle composer input */
    if (app_composer_focused(app)) {
        int pos = app_composer_pos(app);
        int len = strlen(text);
        if (pos + len < 4095) {
            char buf[4096];
            strcpy(buf, app_composer_buf(app));
            memmove(buf + pos + len, buf + pos, strlen(buf + pos) + 1);
            memcpy(buf + pos, text, len);
            app_set_composer_buf(app, buf);
            app_set_composer_pos(app, pos + len);
        }
        return true;
    }
    
    /* Handle search input */
    if (app_search_active(app)) {
        int len = strlen(app_search_query(app));
        int add_len = strlen(text);
        if (len + add_len < 63) {
            char buf[64];
            strcpy(buf, app_search_query(app));
            strcat(buf, text);
            app_set_search_query(app, buf, len + add_len);
        }
        return true;
    }
    
    return false;
}

bool event_handle_mouse(app_state_t *app, gc_event_t *ev) {
    if (!app || !ev) return false;
    
    /* The pet floats above the UI — a click on it takes precedence over
     * whatever panel is underneath. */
    if (ev->type == GC_EV_MOUSE_DOWN && pet_ui_handle_click(app, ev->x, ev->y)) {
        return true;
    }
    
    hit_t hit = hit_test(app, ev->x, ev->y);
    
    if (ev->type == GC_EV_MOUSE_MOVE) {
        app_update_hover(app, ev->x, ev->y);
        return true;
    }
    if (ev->type == GC_EV_MOUSE_DOWN) {
        if (ev->button == 1) { /* Left click */
            switch (hit.type) {
                case HIT_TOOL:
                    titlebar_handle_click(app, ev->x, ev->y);
                    return true;
                    
                case HIT_SEARCH:
                    app_set_search_active(app, true);
                    app_set_composer_focused(app, false);
                    return true;
                    
                case HIT_SESSIONS_HDR:
                    app_toggle_sessions_expanded(app);
                    return true;
                    
                case HIT_SESSION:
                    app_set_selected_session(app, hit.index);
                    app_set_current_view(app, 0);
                    app_set_current_view_name(app, "Chat");
                    app_set_chat_scroll(app, 0);
                    session_db_load_messages(app, hit.index);
                    return true;
                    
                case HIT_NEWCHAT:
                    session_db_create_session(app, "New Chat", "cli", "");
                    session_db_load_sessions(app);
                    return true;
                    
                case HIT_NAV_HDR:
                    app_toggle_nav_expanded(app);
                    return true;
                    
                case HIT_NAV:
                    app_set_selected_nav(app, hit.index);
                    app_set_current_view(app, hit.index);
                    {
                        const nav_item_t *nav = app_nav_items();
                        if (nav && hit.index < app_nav_item_count()) {
                            app_set_current_view_name(app, nav[hit.index].label);
                        }
                    }
                    return true;
                    
                case HIT_PROFILE:
                    /* Show profile menu */
                    return true;
                    
                case HIT_COMPOSER:
                    app_set_composer_focused(app, true);
                    app_set_search_active(app, false);
                    return true;
                    
                case HIT_PILL:
                    app_set_show_model_picker(app, !app_show_model_picker(app));
                    return true;
                    
                case HIT_CHAT:
                    return chat_view_handle_click(app, ev->x, ev->y);
                    
                case HIT_TITLEBAR:
                    return true;
                    
                case HIT_STATUSBAR:
                    return statusbar_handle_click(app, ev->x, ev->y);
                    
                default:
                    break;
            }
        }
    }
    
    if (ev->type == GC_EV_MOUSE_UP) {
        /* Handle drag end, etc. */
    }
    
    return false;
}