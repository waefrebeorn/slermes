/*
 * sidebar.c — Sidebar Rendering and Interaction
 *
 * Handles sidebar drawing, session list, navigation, search, profile section.
 */

#define _GNU_SOURCE
#include "sidebar.h"
#include "app_state_internal.h"
#include "session_db.h"
#include "gui_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ═════════════════════════════════════════════════════════════════════
 * Internal Helpers - format_age must come before use
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

static void draw_search_bar(app_state_t *app, int x, int y, int w) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font = gc_get_font(app_get_window(app));
    gc_font_t *font_small = gc_get_font_small(app_get_window(app));
    
    gc_rect_t search_bg = {x + 8, y + 8, w - 16, SEARCH_H};
    gc_fill_round_rect(app_get_window(app), search_bg, 6, t->bg_card);
    gc_draw_rect(app_get_window(app), search_bg, 1, t->border_subtle);
    
    gc_draw_text(app_get_window(app), font_small, "\xf0\x9f\x94\x8a", x + 16, y + 8 + (SEARCH_H - gc_font_height(font_small)) / 2, t->text_dim);
    
    if (app_search_query_len(app) > 0) {
        gc_draw_text(app_get_window(app), font, app_search_query(app), x + 40, y + 8 + (SEARCH_H - gc_font_height(font)) / 2, t->text);
    } else {
        gc_draw_text(app_get_window(app), font_small, "Search sessions...", x + 40, y + 8 + (SEARCH_H - gc_font_height(font_small)) / 2, t->text_dim);
    }
}

static void draw_section_header(app_state_t *app, int x, int y, int w, const char *label, bool expanded, bool is_sessions) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font_small = gc_get_font_small(app_get_window(app));
    gc_font_t *font_bold = gc_get_font_bold(app_get_window(app));
    int sfh = gc_font_height(font_small);
    
    gc_rect_t hdr = {x + 8, y, w - 16, SECTION_H + 4};
    gc_fill_rect(app_get_window(app), hdr, t->bg_secondary);
    
    const char *caret = expanded ? "\xe2\x96\xbc" : "\xe2\x96\xb6";  // ▼ : ▶
    gc_draw_text(app_get_window(app), font_small, caret, x + 12, y + (SECTION_H + 4 - sfh) / 2, t->text_dim);
    gc_draw_text(app_get_window(app), font_bold, label, x + 30, y + (SECTION_H + 4 - gc_font_height(font_bold)) / 2, t->text_secondary);
    
    if (is_sessions) {
        /* Session count badge */
        char count_buf[32];
        snprintf(count_buf, sizeof(count_buf), "%d", app_session_count(app));
        int tw = gc_text_width(font_small, count_buf);
        gc_rect_t badge = {x + w - tw - 20, y + 2, tw + 8, sfh + 2};
        gc_fill_round_rect(app_get_window(app), badge, 3, t->accent);
        gc_draw_text(app_get_window(app), font_small, count_buf, x + w - tw - 16, y + 2, GC_RGB(0xff,0xff,0xff));
    }
}

static void draw_session_item(app_state_t *app, int x, int y, int w, int idx, bool selected, bool hovered) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font = gc_get_font(app_get_window(app));
    gc_font_t *font_small = gc_get_font_small(app_get_window(app));
    int fh = gc_font_height(font);
    int sfh = gc_font_height(font_small);
    
    app_session_entry_t *s = app_get_session(app, idx);
    if (!s) return;
    
    gc_rect_t item_bg = {x + 8, y, w - 16, ITEM_H};
    gc_color_t bg = selected ? t->accent2 : (hovered ? GC_RGBA(255,255,255,8) : t->bg_secondary);
    gc_fill_round_rect(app_get_window(app), item_bg, 4, bg);
    
    if (selected) {
        gc_draw_rect(app_get_window(app), item_bg, 1, t->accent);
    }
    
    /* Title */
    gc_draw_text_clipped(app_get_window(app), font, s->title[0] ? s->title : s->id, x + 16, y + 2, w - 32, t->text);
    
    /* Metadata: model + age */
    char age[16];
    format_age(s->started_at, age, sizeof(age));
    char meta[128];
    snprintf(meta, sizeof(meta), "%s \xc2\xb7 %s", s->model[0] ? s->model : "default", age);
    gc_draw_text_clipped(app_get_window(app), font_small, meta, x + 16, y + fh, w - 32, t->text_dim);
}

static void draw_new_chat(app_state_t *app, int x, int y, int w, bool hovered) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font_bold = gc_get_font_bold(app_get_window(app));
    
    gc_rect_t btn = {x + 8, y, w - 16, 26};
    gc_fill_round_rect(app_get_window(app), btn, 4, hovered ? GC_RGBA(0,83,253,40) : t->bg_card);
    gc_draw_rect(app_get_window(app), btn, 1, t->border_subtle);
    
    gc_draw_text(app_get_window(app), font_bold, "\xf0\x9f\x93\x8b  + New Chat", x + 16, y + (26 - gc_font_height(font_bold)) / 2, t->accent);
}
static void draw_nav_item(app_state_t *app, int x, int y, int w, int idx, bool selected, bool hovered) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font = gc_get_font(app_get_window(app));
    
    nav_item_t *nav = (nav_item_t*)app_nav_items();
    if (!nav || idx < 0 || idx >= app_nav_item_count()) return;
    
    gc_rect_t item_bg = {x + 8, y, w - 16, ITEM_H};
    gc_color_t bg = selected ? t->accent2 : (hovered ? GC_RGBA(255,255,255,8) : t->bg_secondary);
    gc_fill_round_rect(app_get_window(app), item_bg, 4, bg);
    
    if (selected) {
        gc_draw_rect(app_get_window(app), item_bg, 1, t->accent);
    }
    
    gc_draw_text(app_get_window(app), font, nav[idx].icon, x + 16, y + (ITEM_H - gc_font_height(font)) / 2, t->text);
    gc_draw_text(app_get_window(app), font, nav[idx].label, x + 44, y + (ITEM_H - gc_font_height(font)) / 2, selected ? t->accent : t->text);
}

static void draw_profile_section(app_state_t *app, int x, int y, int w) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font = gc_get_font(app_get_window(app));
    gc_font_t *font_small = gc_get_font_small(app_get_window(app));
    int fh = gc_font_height(font);
    int sfh = gc_font_height(font_small);
    
    int pr_y = y;
    int pr_h = ITEM_H + 10;
    
    gc_rect_t prof_bg = {x + 8, pr_y, w - 16, pr_h};
    gc_fill_round_rect(app_get_window(app), prof_bg, 6, t->bg_card);
    gc_draw_rect(app_get_window(app), prof_bg, 1, t->border_subtle);
    
    /* Avatar placeholder */
    gc_rect_t avatar = {x + 16, pr_y + 5, 28, 28};
    gc_fill_round_rect(app_get_window(app), avatar, 14, t->accent);
    gc_draw_text(app_get_window(app), font, "H", avatar.x + 6, avatar.y + 2, GC_RGB(0xff,0xff,0xff));
    
    /* Name and status */
    gc_draw_text(app_get_window(app), font, "Hermes Agent", x + 52, pr_y + 4, t->text);
    
    const char *status = app_gateway_connected(app) ? "\xe2\x80\xa2 Connected" : "\xe2\x80\xa2 Offline";
    gc_color_t status_color = app_gateway_connected(app) ? t->success : t->text_dim;
    gc_draw_text(app_get_window(app), font_small, status, x + 52, pr_y + 4 + fh + 2, status_color);
}

/* ══════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════ */

void sidebar_draw(app_state_t *app) {
    if (!app) return;
    
    gc_window_t *win = app_get_window(app);
    if (!win) return;
    
    gc_theme_t *t = app_get_theme(app);
    int w = app_sidebar_w(app);
    int h = app_sidebar_h(app);
    int x = 0;
    int y = TITLEBAR_H;
    
    /* Sidebar background */
    gc_rect_t sb_bg = {x, y, w, h};
    gc_fill_rect(win, sb_bg, t->bg_secondary);
    gc_draw_vline(win, w - 1, y, h, t->border);
    
    int cy = y + 8;
    
    /* Search bar */
    draw_search_bar(app, x, cy, w);
    cy += 8 + SEARCH_H + 8;
    
    /* SESSIONS section header */
    draw_section_header(app, x, cy, w, "SESSIONS", app_sessions_expanded(app), true);
    cy += SECTION_H + 4;
    
    /* Session items */
    if (app_sessions_expanded(app)) {
        for (int i = 0; i < app_session_count(app); i++) {
            bool sel = (i == app_selected_session(app));
            bool hov = (app_hover_message(app) == i); /* hover tracking would be in event handling */
            draw_session_item(app, x, cy, w, i, sel, hov);
            cy += ITEM_H;
        }
    } else {
        cy += ITEM_H; /* Show hint */
    }
    
    cy += 4;
    
    /* +New Chat */
    draw_new_chat(app, x, cy, w, false);
    cy += 26 + 12;
    
    /* NAVIGATION section header */
    draw_section_header(app, x, cy, w, "NAVIGATION", app_nav_expanded(app), false);
    cy += SECTION_H + 4;
    
    /* Navigation items */
    if (app_nav_expanded(app)) {
        for (int i = 0; i < app_nav_item_count(); i++) {
            bool sel = (i == app_selected_nav(app));
            draw_nav_item(app, x, cy, w, i, sel, false);
            cy += ITEM_H;
        }
    } else {
        draw_nav_item(app, x, cy, w, app_selected_nav(app), true, false);
        cy += ITEM_H;
    }
    
    /* Profile at bottom */
    int pr_y = TITLEBAR_H + h - ITEM_H - 14;
    draw_profile_section(app, x, pr_y, w);
}

bool sidebar_handle_click(app_state_t *app, int mx, int my) {
    if (!app) return false;
    
    int w = app_sidebar_w(app);
    int x = 0;
    int y = TITLEBAR_H;
    int h = app_sidebar_h(app);
    
    if (mx >= x + w || my < y || my >= y + h) return false;
    
    int cy = y + 8;
    
    /* Search bar click */
    if (my >= cy + 8 && my < cy + 8 + SEARCH_H) {
        app_set_search_active(app, true);
        app_set_composer_focused(app, false);
        return true;
    }
    cy += 8 + SEARCH_H + 8;
    
    /* SESSIONS header */
    if (my >= cy && my < cy + SECTION_H + 4) {
        app_toggle_sessions_expanded(app);
        return true;
    }
    cy += SECTION_H + 4;
    
    /* Session items */
    if (app_sessions_expanded(app)) {
        for (int i = 0; i < app_session_count(app); i++) {
            if (my >= cy && my < cy + ITEM_H) {
                app_set_selected_session(app, i);
                app_set_current_view(app, 0);
                app_set_current_view_name(app, "Chat");
                app_set_chat_scroll(app, 0);
                session_db_load_messages(app, i);
                return true;
            }
            cy += ITEM_H;
        }
    } else {
        cy += ITEM_H;
    }
    
    cy += 4;
    
    /* +New Chat */
    if (my >= cy && my < cy + 26) {
        /* Create new session */
        session_db_create_session(app, "New Chat", "cli", "");
        session_db_load_sessions(app);
        return true;
    }
    cy += 26 + 12;
    
    /* NAVIGATION header */
    if (my >= cy && my < cy + SECTION_H + 4) {
        app_toggle_nav_expanded(app);
        return true;
    }
    cy += SECTION_H + 4;
    
    /* Navigation items */
    if (app_nav_expanded(app)) {
        for (int i = 0; i < app_nav_item_count(); i++) {
            if (my >= cy && my < cy + ITEM_H) {
                app_set_selected_nav(app, i);
                app_set_current_view(app, i);
                const nav_item_t *nav = app_nav_items();
                if (nav && i < app_nav_item_count()) {
                    app_set_current_view_name(app, nav[i].label);
                }
                return true;
            }
            cy += ITEM_H;
        }
    } else {
        if (my >= cy && my < cy + ITEM_H) {
            app_toggle_nav_expanded(app);
            return true;
        }
        cy += ITEM_H;
    }
    
    /* Profile */
    int pr_y = TITLEBAR_H + h - ITEM_H - 14;
    int pr_h = ITEM_H + 10;
    if (my >= pr_y && my < pr_y + pr_h) {
        /* Show profile menu */
        return true;
    }
    
    return false;
}

void sidebar_handle_hover(app_state_t *app, int mx, int my) {
    /* Update hover state - simplified */
    (void)app; (void)mx; (void)my;
}

void sidebar_handle_wheel(app_state_t *app, int delta) {
    if (!app) return;
    int content_h = sidebar_content_height(app);
    int view_h = app_sidebar_h(app);
    if (content_h <= view_h) return;
    
    int scroll = app_sidebar_scroll(app);
    scroll -= delta * 30;
    if (scroll < 0) scroll = 0;
    if (scroll > content_h - view_h) scroll = content_h - view_h;
    app_set_sidebar_scroll(app, scroll);
}

int sidebar_content_height(app_state_t *app) {
    if (!app) return 0;
    
    int h = 0;
    h += 8 + SEARCH_H + 8;  /* Search */
    h += SECTION_H + 4;     /* Sessions header */
    
    if (app_sessions_expanded(app)) {
        h += app_session_count(app) * ITEM_H;
    } else {
        h += ITEM_H;
    }
    
    h += 4 + 26 + 12;       /* New Chat */
    h += SECTION_H + 4;     /* Nav header */
    
    if (app_nav_expanded(app)) {
        h += app_nav_item_count() * ITEM_H;
    } else {
        h += ITEM_H;
    }
    
    h += ITEM_H + 10;       /* Profile */
    return h;
}
