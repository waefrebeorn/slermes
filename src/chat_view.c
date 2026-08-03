/*
 * chat_view.c — Chat View Rendering and Interaction
 *
 * Handles drawing and interaction for the chat message area.
 */

#define _GNU_SOURCE
#include "chat_view.h"
#include "app_state_internal.h"
#include "session_db.h"
#include "gui_core.h"
#include "chat_render.h"
#include "clipboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ══════════════════════════════════════════════════════════════════════ */

static void format_age(long started_at, char *buf, size_t sz) {
    time_t now = time(NULL);
    double delta = difftime(now, (time_t)started_at);
    if (delta < 0) delta = 0;
    if (delta < 60) snprintf(buf, sz, "now");
    else if (delta < 3600) snprintf(buf, sz, "%dm", (int)(delta / 60));
    else if (delta < 86400) snprintf(buf, sz, "%dh", (int)(delta / 3600));
    else if (delta < 604800) snprintf(buf, sz, "%dd", (int)(delta / 86400));
    else if (delta < 2592000) snprintf(buf, sz, "%dw", (int)(delta / 604800));
    else snprintf(buf, sz, "%dmo", (int)(delta / 2592000));
}

static void draw_scrollbar(app_state_t *app, int x, int y, int h, int content_h, int scroll) {
    if (content_h <= h) return;
    gc_theme_t *t = app_get_theme(app);
    int track_x = x - 6;
    int thumb_h = (h * h) / content_h;
    if (thumb_h < 16) thumb_h = 16;
    int thumb_y = y + (scroll * (h - thumb_h)) / (content_h - h);
    if (thumb_y < y) thumb_y = y;
    if (thumb_y + thumb_h > y + h) thumb_y = y + h - thumb_h;
    gc_fill_rect(app_get_window(app), gc_rect(track_x, y, 6, h), GC_RGBA(255,255,255,4));
    gc_fill_round_rect(app_get_window(app), gc_rect(track_x, thumb_y, 6, thumb_h), 3, GC_RGBA(255,255,255,18));
}

static void draw_message_actions(app_state_t *app, int bx, int by, int bw, int msg_idx) {
    gc_theme_t *t = app_get_theme(app);
    if (msg_idx < 0 || msg_idx != app_hover_message(app)) return;
    int ax = bx + bw - 90, ay = by + 12;
    gc_rect_t abg = {ax, ay, 80, 20};
    gc_fill_round_rect(app_get_window(app), abg, 4, GC_RGBA(40,40,48,220));
    gc_draw_rect(app_get_window(app), abg, 1, GC_RGBA(255,255,255,20));
    
    /* Copy button */
    bool copy_hov = (app_hover_action(app) == 0);
    gc_color_t cc = copy_hov ? t->text : t->text_secondary;
    if (copy_hov) gc_fill_round_rect(app_get_window(app), gc_rect(ax, ay, 38, 20), 4, GC_RGBA(255,255,255,30));
    gc_draw_text(app_get_window(app), gc_get_font_small(app_get_window(app)), "\xf0\x9f\x93\x8b Copy", ax+4, ay+2, cc);
    
    /* Edit button */
    bool edit_hov = (app_hover_action(app) == 1);
    gc_color_t ec = edit_hov ? t->text : t->text_secondary;
    if (edit_hov) gc_fill_round_rect(app_get_window(app), gc_rect(ax+42, ay, 38, 20), 4, GC_RGBA(255,255,255,30));
    gc_draw_text(app_get_window(app), gc_get_font_small(app_get_window(app)), "\xe2\x9c\x8f\xef\xb8\x8f Edit", ax+42, ay+2, ec);
}

/* ══════════════════════════════════════════════════════════════════════
 * Chat View Drawing
 * ══════════════════════════════════════════════════════════════════════ */

void chat_view_draw(app_state_t *app) {
    if (!app || !app_get_window(app)) return;
    
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);
    int cw = app_chat_w(app);
    int ch = app_chat_h(app);
    int cx = app_chat_x(app);
    int cy = app_chat_y(app);
    
    /* Background */
    gc_rect_t chat_bg = {cx, cy, cw, ch};
    gc_fill_rect(win, chat_bg, t->bg);
    
    /* Chat area border left */
    gc_draw_vline(win, cx - 1, cy, ch, t->border_subtle);
    
    int msg_count = app_message_count(app);
    if (msg_count == 0) {
        /* Empty state */
        gc_font_t *font = gc_get_font(win);
        gc_font_t *font_small = gc_get_font_small(win);
        int fh = gc_font_height(font);
        int sfh = gc_font_height(font_small);
        gc_draw_text(win, font, "No messages yet", cx + (cw - gc_text_width(font, "No messages yet")) / 2, cy + ch / 2 - fh / 2, t->text_dim);
        gc_draw_text(win, font_small, "Start a conversation", cx + (cw - gc_text_width(font_small, "Start a conversation")) / 2, cy + ch / 2 + fh / 2 + 4, t->text_dim);
        return;
    }
    
    int scroll = app_chat_scroll(app);
    int cur_y = cy - scroll;
    int bubble_x = cx + 20;
    int bubble_w = cw - 40;
    
    for (int i = 0; i < msg_count; i++) {
        message_entry_t *msg = app_get_message(app, i);
        if (!msg) continue;
        
        bool is_user = strcmp(msg->role, "user") == 0;
        gc_color_t role_color = is_user ? GC_RGB(0x00, 0x53, 0xfd) : GC_RGB(0x6f, 0x9b, 0xa6);
        
        /* Render message using chat_render */
        chat_rendered_msg_t *rm = chat_render_message(msg->content, msg->role);
        
        if (rm) {
            /* Calculate message height */
            int pad = 10;
            int small_h = gc_font_height(gc_get_font_small(win));
            int body_h = gc_font_height(gc_get_font(win));
            int line_h = body_h + 3;
            int mono_h = gc_font_height(gc_get_font_mono(win));
            
            int content_h = 0;
            int content_x = bubble_x + pad;
            int content_w = bubble_w - pad * 2;
            
            /* Walk tokens to calculate height */
            for (int ti = 0; ti < rm->token_count; ti++) {
                chat_render_token_t *tok = &rm->tokens[ti];
                switch (tok->type) {
                    case TOKEN_TEXT:
                    case TOKEN_LIST_ITEM:
                    case TOKEN_HEADING:
                    case TOKEN_BLOCKQUOTE:
                    case TOKEN_CODE_INLINE:
                    case TOKEN_LINK_TEXT:
                        if (tok->text) {
                            int tw = gc_text_width(gc_get_font(win), tok->text);
                            content_h += ((tw / content_w) + 1) * line_h;
                        }
                        break;
                    case TOKEN_NEWLINE:
                        content_h += line_h;
                        break;
                    case TOKEN_CODE_BLOCK_START: {
                        /* Find end and count lines */
                        int lines = 1;
                        for (int j = ti + 1; j < rm->token_count; j++) {
                            if (rm->tokens[j].type == TOKEN_CODE_BLOCK_END) break;
                            if (rm->tokens[j].type == TOKEN_TEXT && rm->tokens[j].text) {
                                for (const char *p = rm->tokens[j].text; *p; p++)
                                    if (*p == '\n') lines++;
                            }
                        }
                        content_h += (tok->text && tok->text[0] ? small_h + 4 : 0) + lines * (mono_h + 2) + pad;
                        break;
                    }
                    default:
                        break;
                }
            }
            
            int total_h = pad + small_h + 6 + content_h + pad;
            
            /* Check visibility */
            if (cur_y + total_h < cy || cur_y > cy + ch) {
                cur_y += total_h + BUBBLE_GAP;
                chat_render_free(rm);
                continue;
            }
            
            /* Draw bubble */
            gc_rect_t bubble = {bubble_x, cur_y, bubble_w, total_h};
            gc_fill_round_rect(win, bubble, 6, is_user ? GC_RGBA(0,83,253,10) : GC_RGBA(255,255,255,6));
            gc_draw_rect(win, bubble, 1, t->border_subtle);
            
            /* Role label */
            gc_draw_text(win, gc_get_font_small(win), is_user ? "You" : "Assistant", bubble_x + pad, cur_y + pad + 2, role_color);
            
            /* Draw message content using chat_render logic */
            /* This is simplified - full rendering would use chat_render tokens */
            gc_draw_text_wrapped(win, gc_get_font(win), msg->content, 
                                 bubble_x + pad, cur_y + pad + small_h + 6,
                                 content_w, line_h, t->text);
            
            /* Message actions on hover */
            draw_message_actions(app, bubble_x, cur_y, bubble_w, i);
            
            cur_y += total_h + BUBBLE_GAP;
            chat_render_free(rm);
        } else {
            /* Fallback rendering */
            int pad = 10;
            int small_h = gc_font_height(gc_get_font_small(win));
            int body_h = gc_font_height(gc_get_font(win));
            int line_h = body_h + 3;
            int text_h = gc_draw_text_wrapped(win, gc_get_font(win), msg->content,
                                               bubble_x + pad, cur_y + pad + small_h + 6,
                                               bubble_w - pad * 2, line_h, t->text);
            if (text_h < line_h) text_h = line_h;
            int total_h = pad + small_h + 6 + text_h + pad;
            
            gc_rect_t bubble = {bubble_x, cur_y, bubble_w, total_h};
            gc_fill_round_rect(win, bubble, 6, is_user ? GC_RGBA(0,83,253,10) : GC_RGBA(255,255,255,6));
            gc_draw_rect(win, bubble, 1, t->border_subtle);
            gc_draw_text(win, gc_get_font_small(win), is_user ? "You" : "Assistant", bubble_x + pad, cur_y + pad + 2, role_color);
            draw_message_actions(app, bubble_x, cur_y, bubble_w, i);
            
            cur_y += total_h + BUBBLE_GAP;
        }
    }
    
    /* Update content height */
    app_set_chat_content_h(app, cur_y - cy + scroll);
    
    /* Draw scrollbar */
    draw_scrollbar(app, cx + cw, cy, ch, app_chat_content_h(app), scroll);
    
    /* Scroll to bottom button */
    if (app_show_scroll_button(app)) {
        int btn_x = cx + cw - 50;
        int btn_y = cy + ch - 60;
        gc_rect_t btn = {btn_x, btn_y, 40, 40};
        gc_fill_round_rect(win, btn, 20, app_scroll_button_hover(app) ? GC_RGBA(0,83,253,80) : GC_RGBA(0,0,0,80));
        gc_draw_rect(win, btn, 1, t->border);
        gc_draw_text(win, gc_get_font(win), "\xe2\x86\x93", btn_x + 12, btn_y + 8, t->text);
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Chat View Interaction
 * ══════════════════════════════════════════════════════════════════════ */

bool chat_view_handle_click(app_state_t *app, int mx, int my) {
    if (!app) return false;
    
    gc_window_t *win = app_get_window(app);
    int cx = app_chat_x(app);
    int cy = app_chat_y(app);
    int cw = app_chat_w(app);
    int ch = app_chat_h(app);
    
    /* Check if click is in chat area */
    if (mx < cx || mx >= cx + cw || my < cy || my >= cy + ch) return false;
    
    /* Check scroll to bottom button */
    if (app_show_scroll_button(app)) {
        int btn_x = cx + cw - 50;
        int btn_y = cy + ch - 60;
        if (mx >= btn_x && mx < btn_x + 40 && my >= btn_y && my < btn_y + 40) {
            chat_view_scroll_to_bottom(app);
            return true;
        }
    }

    /* Copy button on the hovered message: copies the message content to the
     * system clipboard (the Electron copy action — clipboard_write_text). */
    {
        int hm = app_hover_message(app);
        if (hm >= 0) {
            message_entry_t *msg = app_get_message(app, hm);
            if (msg) {
                /* Recompute the bubble geometry to find the copy button. */
                gc_window_t *win = app_get_window(app);
                int pad = 10;
                int bubble_w = cw - pad * 2;
                int small_h = gc_font_height(gc_get_font_small(win));
                int body_h = gc_font_height(gc_get_font(win));
                int line_h = body_h + 3;
                int cur_y = cy + 8;
                for (int i = 0; i <= hm; i++) {
                    message_entry_t *m = app_get_message(app, i);
                    if (!m) break;
                    int text_h = gc_draw_text_wrapped(win, gc_get_font(win),
                        m->content, 0, 0, bubble_w - pad * 2, line_h, 0);
                    if (text_h < line_h) text_h = line_h;
                    int total_h = pad + small_h + 6 + text_h + pad;
                    if (i == hm) {
                        int ax = cx + 8 + bubble_w - 90, ay = cur_y + 12;
                        if (mx >= ax && mx < ax + 38 && my >= ay && my < ay + 20) {
                            if (clipboard_write_text(msg->content)) {
                                app_set_api_status(app, "Message copied to clipboard");
                            } else {
                                app_set_api_status(app, "Copy failed — no clipboard tool");
                            }
                            return true;
                        }
                        break;
                    }
                    cur_y += total_h + BUBBLE_GAP;
                }
            }
        }
    }
    
    return false;
}

void chat_view_handle_hover(app_state_t *app, int mx, int my) {
    if (!app) return;
    
    gc_window_t *win = app_get_window(app);
    int cx = app_chat_x(app);
    int cy = app_chat_y(app);
    int cw = app_chat_w(app);
    int ch = app_chat_h(app);
    
    /* Check scroll to bottom button hover */
    if (app_show_scroll_button(app)) {
        int btn_x = cx + cw - 50;
        int btn_y = cy + ch - 60;
        bool hover = (mx >= btn_x && mx < btn_x + 40 && my >= btn_y && my < btn_y + 40);
        app_set_scroll_button_hover(app, hover);
    }
    
    /* Check message hover - simplified */
    int scroll = app_chat_scroll(app);
    int cur_y = cy - scroll;
    int bubble_x = cx + 20;
    int bubble_w = cw - 40;
    
    int msg_count = app_message_count(app);
    for (int i = 0; i < msg_count; i++) {
        message_entry_t *msg = app_get_message(app, i);
        if (!msg) continue;
        
        /* Approximate bubble height */
        int pad = 10;
        int small_h = gc_font_height(gc_get_font_small(win));
        int body_h = gc_font_height(gc_get_font(win));
        int line_h = body_h + 3;
        int text_h = gc_draw_text_wrapped(win, gc_get_font(win), msg->content, 0, 0, bubble_w - pad * 2, line_h, 0);
        if (text_h < line_h) text_h = line_h;
        int total_h = pad + small_h + 6 + text_h + pad;
        
        if (mx >= bubble_x && mx < bubble_x + bubble_w && my >= cur_y && my < cur_y + total_h) {
            app_set_hover_message(app, i);
            /* Check copy/edit button hover */
            int ax = bubble_x + bubble_w - 90, ay = cur_y + 12;
            if (mx >= ax && mx < ax + 38 && my >= ay && my < ay + 20) {
                app_set_hover_action(app, 0); /* copy */
            } else if (mx >= ax + 42 && mx < ax + 80 && my >= ay && my < ay + 20) {
                app_set_hover_action(app, 1); /* edit */
            } else {
                app_set_hover_action(app, -1);
            }
            return;
        }
        cur_y += total_h + BUBBLE_GAP;
    }
    
    app_set_hover_message(app, -1);
    app_set_hover_action(app, -1);
}

void chat_view_handle_wheel(app_state_t *app, int delta) {
    if (!app) return;
    int content_h = app_chat_content_h(app);
    int view_h = app_chat_h(app);
    if (content_h <= view_h) return;
    
    int scroll = app_chat_scroll(app);
    scroll -= delta * 30;
    if (scroll < 0) scroll = 0;
    if (scroll > content_h - view_h) scroll = content_h - view_h;
    app_set_chat_scroll(app, scroll);
}

bool chat_view_handle_key(app_state_t *app, int key, int mod, const char *text) {
    if (!app) return false;
    
    /* Handle composer input */
    if (app_composer_focused(app)) {
        /* Composer handles its own keys */
        return false;
    }
    
    switch (key) {
        case 'g': /* Scroll to top */
            if (mod & 1) { /* Shift */
                app_set_chat_scroll(app, 0);
                return true;
            }
            break;
        case 'G': /* Scroll to bottom */
            chat_view_scroll_to_bottom(app);
            return true;
        case ' ': /* Space - scroll down */
            chat_view_handle_wheel(app, -3);
            return true;
        case 'b': /* b - scroll up */
            chat_view_handle_wheel(app, 3);
            return true;
    }
    
    return false;
}

int chat_content_height(app_state_t *app) {
    return app ? app_chat_content_h(app) : 0;
}

void chat_view_scroll_to_bottom(app_state_t *app) {
    if (!app) return;
    int content_h = app_chat_content_h(app);
    int view_h = app_chat_h(app);
    if (content_h > view_h) {
        app_set_chat_scroll(app, content_h - view_h);
    } else {
        app_set_chat_scroll(app, 0);
    }
}