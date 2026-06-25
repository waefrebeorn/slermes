/*
 * gui_core.h — Slermes Custom GUI Framework
 *
 * A cross-platform graphical UI toolkit built on SDL2.
 * SDL2 is used ONLY as the platform abstraction (window, input, texture).
 * ALL widgets, layouts, themes, and rendering are our own.
 *
 * Platform support: Wayland, X11, Windows, macOS (via SDL2 backends)
 *
 * MIT License — Slermes Fork
 */
#ifndef GUI_CORE_H
#define GUI_CORE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════
 * Color
 * ══════════════════════════════════════════════════════════════════════ */
typedef uint32_t gc_color_t;
#define GC_RGB(r,g,b)   ((gc_color_t)(((r)<<16)|((g)<<8)|(b)|0xFF000000))
#define GC_RGBA(r,g,b,a)((gc_color_t)(((a)<<24)|((r)<<16)|((g)<<8)|(b)))
#define GC_HEX(h)       ((gc_color_t)(((h)<<8)|0xFF))

/* ══════════════════════════════════════════════════════════════════════
 * Rect
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct { int x, y, w, h; } gc_rect_t;
#define gc_rect(x,y,w,h) ((gc_rect_t){x,y,w,h})

/* ══════════════════════════════════════════════════════════════════════
 * Theme — Hermes Electron Desktop exact palette
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    gc_color_t bg;           /* chrome/chat background #0d0d0e */
    gc_color_t bg_secondary; /* sidebar background #0a0a0b */
    gc_color_t bg_card;      /* card/editor background #161618 */
    gc_color_t accent;       /* accent/blue #0053fd */
    gc_color_t accent2;      /* selection/highlight */
    gc_color_t text;         /* primary text 94% #e8e8ea */
    gc_color_t text_secondary;/* secondary text 74% #b8b8bb */
    gc_color_t text_dim;     /* tertiary text 54% #88888c */
    gc_color_t border;       /* strokes #222228 */
    gc_color_t border_subtle;/* very subtle stroke */
    gc_color_t error, warn, success, cyan;
    int        font_size;    /* base font size in pts */
    int        sidebar_w;    /* sidebar width in px (237 = 14.8125rem) */
    int        header_h;     /* titlebar height (34px) */
    int        statusbar_h;  /* statusbar height (20px) */
    int        padding;      /* sidebar content padding (16px) */
} gc_theme_t;

extern gc_theme_t gc_theme_dark;
extern gc_theme_t gc_theme_light;
extern gc_theme_t gc_theme_solarized;
extern gc_theme_t gc_theme_nord;

/* ══════════════════════════════════════════════════════════════════════
 * Font
 * ══════════════════════════════════════════════════════════════════════ */
typedef TTF_Font gc_font_t;

int  gc_font_height(gc_font_t *font);
int  gc_text_width(gc_font_t *font, const char *text);

/* Font accessors */
typedef struct gc_window gc_window_t;
gc_font_t *gc_get_font(gc_window_t *win);
gc_font_t *gc_get_font_mono(gc_window_t *win);
gc_font_t *gc_get_font_small(gc_window_t *win);
gc_font_t *gc_get_font_bold(gc_window_t *win);

/* ══════════════════════════════════════════════════════════════════════
 * Window
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct gc_window gc_window_t;

gc_window_t *gc_create_window(const char *title, int w, int h, const gc_theme_t *theme);
void         gc_destroy_window(gc_window_t *win);
int          gc_window_w(gc_window_t *win);
int          gc_window_h(gc_window_t *win);
gc_theme_t   gc_get_theme(gc_window_t *win);
void         gc_set_theme(gc_window_t *win, const gc_theme_t *theme);
void         gc_set_title(gc_window_t *win, const char *title);

/* ══════════════════════════════════════════════════════════════════════
 * Drawing API
 * ══════════════════════════════════════════════════════════════════════ */
void gc_fill_rect(gc_window_t *win, gc_rect_t r, gc_color_t color);
void gc_draw_rect(gc_window_t *win, gc_rect_t r, int bw, gc_color_t color);
void gc_fill_round_rect(gc_window_t *win, gc_rect_t r, int radius, gc_color_t color);
void gc_draw_hline(gc_window_t *win, int x, int y, int w, gc_color_t color);
void gc_draw_vline(gc_window_t *win, int x, int y, int h, gc_color_t color);
void gc_draw_text(gc_window_t *win, gc_font_t *font, const char *text,
                  int x, int y, gc_color_t color);
void gc_draw_text_clipped(gc_window_t *win, gc_font_t *font, const char *text,
                          int x, int y, int clip_w, gc_color_t color);
void gc_draw_text_centered(gc_window_t *win, gc_font_t *font, const char *text,
                           gc_rect_t area, gc_color_t color);
int  gc_draw_text_wrapped(gc_window_t *win, gc_font_t *font, const char *text,
                           int x, int y, int max_w, int line_h,
                           gc_color_t color);

/* ══════════════════════════════════════════════════════════════════════
 * Events
 * ══════════════════════════════════════════════════════════════════════ */
typedef enum {
    GC_EV_NONE,
    GC_EV_QUIT,
    GC_EV_KEY_DOWN,
    GC_EV_KEY_UP,
    GC_EV_TEXT_INPUT,
    GC_EV_MOUSE_MOVE,
    GC_EV_MOUSE_DOWN,
    GC_EV_MOUSE_UP,
    GC_EV_MOUSE_WHEEL,
    GC_EV_RESIZE,
} gc_event_type_t;

#define GC_MAX_TEXT 64

typedef struct {
    gc_event_type_t type;
    int x, y;
    int key;
    int button;
    int wheel_delta;
    int resize_w, resize_h;
    int mod; /* KMOD_* modifier flags */
    char text[GC_MAX_TEXT];
} gc_event_t;

int gc_poll_event(gc_window_t *win, gc_event_t *ev);

/* ══════════════════════════════════════════════════════════════════════
 * Frame control
 * ══════════════════════════════════════════════════════════════════════ */
void gc_begin_frame(gc_window_t *win);
void gc_end_frame(gc_window_t *win);
void gc_save_screenshot(gc_window_t *win, const char *path);
void gc_set_fps(gc_window_t *win, int fps);
uint64_t gc_now_ms(void);

/* ══════════════════════════════════════════════════════════════════════
 * Initialization
 * ══════════════════════════════════════════════════════════════════════ */
int  gc_init(void);
void gc_quit(void);

#ifdef __cplusplus
}
#endif
#endif /* GUI_CORE_H */
