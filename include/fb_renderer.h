/*
 * fb_renderer.h — Slermes Custom Software Framebuffer Renderer
 *
 * Zero-dependency (beyond X11), pixel-perfect 2D rendering engine.
 * Uses a 32-bit RGBA memory framebuffer + XPutImage to display.
 * stb_truetype.h for anti-aliased font rendering.
 *
 * MIT License — Slermes Fork
 */
#ifndef FB_RENDERER_H
#define FB_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Color helpers ────────────────────────────────────────────────── */
#define RGB(r,g,b)   ((uint32_t)(((r)<<16)|((g)<<8)|(b)|0xFF000000))
#define RGBA(r,g,a)  ((uint32_t)(((r)<<16)|((g)<<8)|(a)|0x000000FF))
#define HEX(hex)     ((uint32_t)(((hex)<<8)|0xFF))
#define FB_BLACK     0xFF000000
#define FB_WHITE     0xFFFFFFFF
#define FB_TRANSPARENT 0x00000000

/* Hermes Teal palette — exact match */
#define TEAL_DARK    0xFF0A0A0A  /* near-black bg */
#define TEAL_MUTED   0xFF0B5C43  /* muted teal accent */
#define TEAL_TEXT    0xFFE5E5E5  /* main text */
#define TEAL_DIM     0xFF888888  /* dim text */
#define TEAL_HIGHLIGHT 0xFF0F221E /* teal highlight bg */

/* ── Opaque types ─────────────────────────────────────────────────── */
typedef struct fb_window fb_window_t;
typedef struct fb_font  fb_font_t;

/* ── Window creation ──────────────────────────────────────────────── */
fb_window_t *fb_create(const char *title, int width, int height);
void         fb_destroy(fb_window_t *win);
int          fb_width(fb_window_t *win);
int          fb_height(fb_window_t *win);
void        *fb_pixels(fb_window_t *win);   /* raw RGBA buffer */
int          fb_stride(fb_window_t *win);    /* bytes per row */

/* ── Font ─────────────────────────────────────────────────────────── */
fb_font_t   *fb_load_font(const char *ttf_path, float size_pt);
void         fb_free_font(fb_font_t *font);
int          fb_font_height(fb_font_t *font);
int          fb_text_width(fb_font_t *font, const char *text);

/* ── Drawing primitives (all coordinates in pixels) ────────────────── */
void fb_clear       (fb_window_t *win, uint32_t color);
void fb_fill_rect   (fb_window_t *win, int x, int y, int w, int h, uint32_t color);
void fb_draw_rect   (fb_window_t *win, int x, int y, int w, int h, int bw, uint32_t color);
void fb_fill_round_rect(fb_window_t *win, int x, int y, int w, int h, int r, uint32_t color);
void fb_draw_hline  (fb_window_t *win, int x, int y, int w, uint32_t color);
void fb_draw_vline  (fb_window_t *win, int x, int y, int h, uint32_t color);
void fb_blit        (fb_window_t *win, int dx, int dy, int dw, int dh,
                     const uint32_t *src, int sw, int sh);

/* ── Text rendering (anti-aliased via stb_truetype) ────────────────── */
void fb_draw_text   (fb_window_t *win, fb_font_t *font, const char *text,
                     int x, int y, uint32_t color);
void fb_draw_text_clipped(fb_window_t *win, fb_font_t *font, const char *text,
                          int x, int y, int clip_w, uint32_t color);
int  fb_text_width_clipped(fb_font_t *font, const char *text, int max_w);

/* ── Events ───────────────────────────────────────────────────────── */
typedef enum {
    FB_EVENT_NONE      = 0,
    FB_EVENT_QUIT,
    FB_EVENT_KEY_DOWN,
    FB_EVENT_KEY_UP,
    FB_EVENT_MOUSE_MOVE,
    FB_EVENT_MOUSE_DOWN,
    FB_EVENT_MOUSE_UP,
    FB_EVENT_MOUSE_WHEEL,
    FB_EVENT_RESIZE,
} fb_event_type_t;

typedef struct {
    fb_event_type_t type;
    int64_t timestamp_ms;
    union {
        int keycode;          /* X11 keysym */
        struct { int x, y; int button; } mouse;
        struct { int x, y; int delta; } wheel;
        struct { int w, h; } resize;
    };
} fb_event_t;

/* Poll for an event (non-blocking). Returns 0 if none. */
int  fb_poll_event(fb_window_t *win, fb_event_t *ev);

/* Wait for the next event (blocking). Returns 1. */
int  fb_wait_event(fb_window_t *win, fb_event_t *ev);

/* ── Frame sync ───────────────────────────────────────────────────── */
void fb_present(fb_window_t *win);  /* flip framebuffer to screen */

/* ── Cursor ───────────────────────────────────────────────────────── */
void fb_set_cursor(fb_window_t *win, int cursor_id);

#ifdef __cplusplus
}
#endif
#endif /* FB_RENDERER_H */
