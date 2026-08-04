/*
 * gui_core.c — Slermes Custom GUI Framework Implementation
 *
 * Built on SDL2 as the platform abstraction layer only.
 * ALL widgets, layout, themes, and rendering are our own.
 *
 * Hermes Electron Desktop exact dark theme palette:
 *   Chrome/chat bg:  #0d0d0e
 *   Sidebar bg:      #0a0a0b
 *   Card bg:         #161618
 *   Text primary:    #e8e8ea  (94% of #17171a)
 *   Text secondary:  #b8b8bb  (74% of #17171a)
 *   Text tertiary:   #88888c  (54% of #17171a)
 *   Accent/blue:     #0053fd
 *   Red:             #e75e78
 *   Green:           #55a583
 *   Cyan:            #6f9ba6
 *   Border:          mix(accent 10%, text 5%) ≈ #222228
 *
 * MIT License — Slermes Fork
 */
#define _GNU_SOURCE
#include "gui_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ══════════════════════════════════════════════════════════════════════
 *  Themes — Hermes Electron Desktop Exact Palette
 * ══════════════════════════════════════════════════════════════════════ */
gc_theme_t gc_theme_dark = {
    .bg             = GC_RGB(0x0d, 0x0d, 0x0e),  /* chrome/chat bg — Neutral #0d0d0e */
    .bg_secondary   = GC_RGB(0x0a, 0x0a, 0x0b),  /* sidebar bg — Neutral #0a0a0b */
    .bg_card        = GC_RGB(0x16, 0x16, 0x18),  /* card/editor bg — Neutral #161618 */
    .accent         = GC_RGB(0x00, 0x53, 0xfd),  /* blue accent #0053fd */
    .accent2        = GC_RGB(0x0f, 0x2a, 0x5a),  /* selection highlight */
    .text           = GC_RGB(0xdd, 0xdd, 0xdd),  /* primary text — 94% of #eaeaea on #0d0d0e */
    .text_secondary = GC_RGB(0xb1, 0xb1, 0xb1),  /* secondary text — 74% of #eaeaea on #0d0d0e */
    .text_dim       = GC_RGB(0x85, 0x85, 0x85),  /* tertiary text — 54% of #eaeaea on #0d0d0e */
    .border         = GC_RGB(0x22, 0x22, 0x28),  /* subtle stroke #222228 */
    .border_subtle  = GC_RGB(0x16, 0x16, 0x1a),  /* very subtle #16161a */
    .error          = GC_RGB(0xe7, 0x5e, 0x78),  /* red #e75e78 */
    .warn           = GC_RGB(0xff, 0xbd, 0x38),  /* yellow/warning #ffbd38 */
    .success        = GC_RGB(0x55, 0xa5, 0x83),  /* green #55a583 */
    .cyan           = GC_RGB(0x6f, 0x9b, 0xa6),  /* cyan #6f9ba6 */
    .font_size      = 10,        /* 10pt ≈ 13px at 96dpi — matches Electron body text */
    .sidebar_w      = 237,       /* 14.8125rem */
    .header_h       = 34,        /* TITLEBAR_HEIGHT */
    .statusbar_h    = 20,        /* h-5 */
    .padding        = 16,        /* sidebar-content-inline-padding */
};

gc_theme_t gc_theme_light = {
    .bg             = GC_RGB(0xf8, 0xfa, 0xff),
    .bg_secondary   = GC_RGB(0xf3, 0xf7, 0xff),
    .bg_card        = GC_RGB(0xff, 0xff, 0xff),
    .accent         = GC_RGB(0x00, 0x53, 0xfd),
    .accent2        = GC_RGB(0x0f, 0x2a, 0x5a),
    .text           = GC_RGB(0x17, 0x17, 0x1a),
    .text_secondary = GC_RGB(0x44, 0x44, 0x48),
    .text_dim       = GC_RGB(0x77, 0x77, 0x7a),
    .border         = GC_RGB(0xd0, 0xd0, 0xd4),
    .border_subtle  = GC_RGB(0xe0, 0xe0, 0xe4),
    .error          = GC_RGB(0xcf, 0x2d, 0x56),
    .warn           = GC_RGB(0xc0, 0x85, 0x32),
    .success        = GC_RGB(0x1f, 0x8a, 0x65),
    .cyan           = GC_RGB(0x4c, 0x7f, 0x8c),
    .font_size      = 10,
    .sidebar_w      = 237,
    .header_h       = 34,
    .statusbar_h    = 20,
    .padding        = 16,
};

/* ── Solarized Dark ──────────────────────────────────────────────── */
gc_theme_t gc_theme_solarized = {
    .bg             = GC_RGB(0x00, 0x2b, 0x36),  /* base03 */
    .bg_secondary   = GC_RGB(0x07, 0x36, 0x42),  /* base02 */
    .bg_card        = GC_RGB(0x00, 0x44, 0x58),  /* base01-ish */
    .accent         = GC_RGB(0x26, 0x8b, 0xd2),  /* blue */
    .accent2        = GC_RGB(0x00, 0x65, 0x80),  /* darker blue */
    .text           = GC_RGB(0x93, 0xa1, 0xa1),  /* base1 */
    .text_secondary = GC_RGB(0x83, 0x94, 0x96),  /* base0 */
    .text_dim       = GC_RGB(0x58, 0x6e, 0x75),  /* base00 */
    .border         = GC_RGB(0x00, 0x44, 0x58),
    .border_subtle  = GC_RGB(0x07, 0x36, 0x42),
    .error          = GC_RGB(0xdc, 0x32, 0x2f),  /* red */
    .warn           = GC_RGB(0xb5, 0x89, 0x00),  /* yellow */
    .success        = GC_RGB(0x85, 0x99, 0x00),  /* green */
    .cyan           = GC_RGB(0x2a, 0xa1, 0x98),  /* cyan */
    .font_size      = 10,
    .sidebar_w      = 237,
    .header_h       = 34,
    .statusbar_h    = 20,
    .padding        = 16,
};

/* ── Nord ────────────────────────────────────────────────────────── */
gc_theme_t gc_theme_nord = {
    .bg             = GC_RGB(0x2e, 0x34, 0x40),  /* nord0 */
    .bg_secondary   = GC_RGB(0x3b, 0x42, 0x52),  /* nord1 */
    .bg_card        = GC_RGB(0x43, 0x4c, 0x5e),  /* nord2 */
    .accent         = GC_RGB(0x88, 0xc0, 0xd0),  /* nord8 */
    .accent2        = GC_RGB(0x5e, 0x81, 0xac),  /* nord9 */
    .text           = GC_RGB(0xec, 0xef, 0xf4),  /* nord6 */
    .text_secondary = GC_RGB(0xd8, 0xde, 0xe9),  /* nord4 */
    .text_dim       = GC_RGB(0x61, 0x6e, 0x88),  /* nord3-ish */
    .border         = GC_RGB(0x4c, 0x56, 0x6a),  /* nord3 */
    .border_subtle  = GC_RGB(0x3b, 0x42, 0x52),
    .error          = GC_RGB(0xbf, 0x61, 0x6a),  /* nord11 */
    .warn           = GC_RGB(0xeb, 0xcb, 0x8b),  /* nord13 */
    .success        = GC_RGB(0xa3, 0xbe, 0x8c),  /* nord14 */
    .cyan           = GC_RGB(0x8f, 0xbc, 0xbb),  /* nord7 */
    .font_size      = 10,
    .sidebar_w      = 237,
    .header_h       = 34,
    .statusbar_h    = 20,
    .padding        = 16,
};

/* ══════════════════════════════════════════════════════════════════════
 *  Window struct
 * ══════════════════════════════════════════════════════════════════════ */
struct gc_window {
    SDL_Window   *sdl_win;
    SDL_Renderer *renderer;
    int           w, h;
    gc_theme_t    theme;
    gc_font_t    *font;
    gc_font_t    *font_mono;
    gc_font_t    *font_small;
    gc_font_t    *font_bold;
    uint64_t      frame_start;
    uint32_t      frame_delay_ms;
    bool          running;
    char          title[256];
};

/* ══════════════════════════════════════════════════════════════════════
 *  Initialization / Quit
 * ══════════════════════════════════════════════════════════════════════ */
static int gc_refcount = 0;

int gc_init(void) {
    if (gc_refcount > 0) { gc_refcount++; return 0; }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }
    gc_refcount = 1;
    return 0;
}

void gc_quit(void) {
    if (gc_refcount == 0) return;
    gc_refcount--;
    if (gc_refcount == 0) {
        TTF_Quit();
        SDL_Quit();
    }
}

/* ══════════════════════════════════════════════════════════════════════
 *  Window creation
 * ══════════════════════════════════════════════════════════════════════ */
gc_window_t *gc_create_window(const char *title, int w, int h,
                              const gc_theme_t *theme) {
    gc_window_t *win = calloc(1, sizeof(gc_window_t));
    if (!win) return NULL;

    win->sdl_win = SDL_CreateWindow(title ? title : "Slermes Agent",
                                     SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     w, h,
                                     SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win->sdl_win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        free(win);
        return NULL;
    }

    /* Create renderer — prefer software (works on Xvfb and all platforms) */
    win->renderer = SDL_CreateRenderer(win->sdl_win, -1,
                                       SDL_RENDERER_SOFTWARE);
    /* If that fails, try each driver individually */
    if (!win->renderer) {
        int ndrv = SDL_GetNumRenderDrivers();
        for (int i = 0; i < ndrv; i++) {
            win->renderer = SDL_CreateRenderer(win->sdl_win, i, 0);
            if (win->renderer) break;
        }
    }
    if (!win->renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win->sdl_win);
        free(win);
        return NULL;
    }
    SDL_SetRenderDrawBlendMode(win->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(win->renderer, 0x0d, 0x0d, 0x0e, 0xFF);
    SDL_RenderClear(win->renderer);

    win->w = w; win->h = h;
    if (theme) win->theme = *theme;
    else win->theme = gc_theme_dark;

    /* ══════════════════════════════════════════════════════════════════
     * Load fonts — try fontconfig first, then common paths
     * ══════════════════════════════════════════════════════════════════ */
    int fs = win->theme.font_size;
    win->font = NULL;

    /* Try fontconfig first */
    {
        FILE *fp = popen("fc-match -f '%{file}' 'sans-serif' 2>/dev/null", "r");
        if (fp) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0]) win->font = TTF_OpenFont(buf, fs);
            }
            pclose(fp);
        }
    }

    /* Fall back to known paths */
    if (!win->font) {
        const char *paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
            "/run/current-system/sw/share/fonts/dejavu/DejaVuSans.ttf",
            NULL
        };
        for (int i = 0; paths[i]; i++) {
            win->font = TTF_OpenFont(paths[i], fs);
            if (win->font) { fprintf(stderr, "gui: font: %s\n", paths[i]); break; }
        }
    } else {
        fprintf(stderr, "gui: font: (fc-match)\n");
    }

    /* Try fontconfig for monospace */
    {
        FILE *fp = popen("fc-match -f '%{file}' 'monospace' 2>/dev/null", "r");
        if (fp) {
            char buf[1024];
            if (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0]) {
                    win->font_mono = TTF_OpenFont(buf, fs);
                    if (win->font_mono) fprintf(stderr, "gui: mono: (fc-match)\n");
                }
            }
            pclose(fp);
        }
    }
    if (!win->font_mono) {
        const char *paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
            "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/TTF/LiberationMono-Regular.ttf",
            NULL
        };
        for (int i = 0; paths[i]; i++) {
            win->font_mono = TTF_OpenFont(paths[i], fs);
            if (win->font_mono) { fprintf(stderr, "gui: mono: %s\n", paths[i]); break; }
        }
    }

    /* Small font (11pt) — try same list at smaller size */
    win->font_small = NULL;
    {
        const char *paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            NULL
        };
        for (int i = 0; paths[i]; i++) {
            win->font_small = TTF_OpenFont(paths[i], fs - 2);
            if (win->font_small) break;
        }
    }

    /* Bold font variant */
    win->font_bold = NULL;
    {
        const char *bold_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/noto/NotoSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            NULL
        };
        for (int i = 0; bold_paths[i]; i++) {
            win->font_bold = TTF_OpenFont(bold_paths[i], fs);
            if (win->font_bold) { fprintf(stderr, "gui: bold: %s\n", bold_paths[i]); break; }
        }
    }

    if (!win->font) {
        fprintf(stderr, "gui: WARNING — no font loaded! Text will be invisible.\n");
    }

    strncpy(win->title, title ? title : "Slermes Agent", sizeof(win->title)-1);
    win->title[sizeof(win->title)-1] = '\0';
    win->running = true;
    win->frame_delay_ms = 16;
    return win;
}

void gc_destroy_window(gc_window_t *win) {
    if (!win) return;
    if (win->font)      TTF_CloseFont(win->font);
    if (win->font_mono) TTF_CloseFont(win->font_mono);
    if (win->font_small)TTF_CloseFont(win->font_small);
    if (win->font_bold) TTF_CloseFont(win->font_bold);
    if (win->renderer)  SDL_DestroyRenderer(win->renderer);
    if (win->sdl_win)   SDL_DestroyWindow(win->sdl_win);
    free(win);
}

int gc_window_w(gc_window_t *win) { return win->w; }
int gc_window_h(gc_window_t *win) { return win->h; }
gc_theme_t gc_get_theme(gc_window_t *win) { return win->theme; }
void gc_set_theme(gc_window_t *win, const gc_theme_t *theme) {
    if (theme) win->theme = *theme;
}
void gc_set_title(gc_window_t *win, const char *title) {
    if (title) {
        strncpy(win->title, title, sizeof(win->title)-1);
        win->title[sizeof(win->title)-1] = '\0';
        SDL_SetWindowTitle(win->sdl_win, title);
    }
}

/* ══════════════════════════════════════════════════════════════════════
 *  Font helpers
 * ══════════════════════════════════════════════════════════════════════ */
int gc_font_height(gc_font_t *f) {
    if (!f) return 16;
    return TTF_FontHeight(f);
}
int gc_text_width(gc_font_t *f, const char *text) {
    if (!f || !text) return 0;
    int w = 0;
    TTF_SizeUTF8(f, text, &w, NULL);
    return w;
}

gc_font_t *gc_get_font(gc_window_t *win) { return win ? win->font : NULL; }
gc_font_t *gc_get_font_mono(gc_window_t *win) { return win ? win->font_mono : NULL; }
gc_font_t *gc_get_font_small(gc_window_t *win) { return win ? win->font_small : NULL; }
gc_font_t *gc_get_font_bold(gc_window_t *win) { return win ? win->font_bold : NULL; }

/* ══════════════════════════════════════════════════════════════════════
 *  Drawing API
 * ══════════════════════════════════════════════════════════════════════ */
void gc_fill_rect(gc_window_t *win, gc_rect_t r, gc_color_t color) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SDL_SetRenderDrawColor(win->renderer,
                           (color>>16)&0xFF, (color>>8)&0xFF,
                           color&0xFF, (color>>24)&0xFF);
    SDL_RenderFillRect(win->renderer, &sr);
}

void gc_draw_rect(gc_window_t *win, gc_rect_t r, int bw, gc_color_t color) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_SetRenderDrawColor(win->renderer,
                           (color>>16)&0xFF, (color>>8)&0xFF,
                           color&0xFF, (color>>24)&0xFF);
    if (bw <= 1) {
        SDL_RenderDrawRect(win->renderer, (SDL_Rect[]){ {r.x, r.y, r.w, r.h} });
    } else {
        for (int i = 0; i < bw; i++)
            SDL_RenderDrawRect(win->renderer,
                &(SDL_Rect){r.x+i, r.y+i, r.w-i*2, r.h-i*2});
    }
}

void gc_fill_round_rect(gc_window_t *win, gc_rect_t r, int radius,
                        gc_color_t color) {
    if (r.w <= 0 || r.h <= 0) return;
    if (radius <= 0) { gc_fill_rect(win, r, color); return; }
    int rad = (radius > r.w/2 || radius > r.h/2) ?
              (r.w < r.h ? r.w/2 : r.h/2) : radius;
    gc_fill_rect(win, gc_rect(r.x+rad, r.y, r.w-2*rad, r.h), color);
    gc_fill_rect(win, gc_rect(r.x, r.y+rad, rad, r.h-2*rad), color);
    gc_fill_rect(win, gc_rect(r.x+r.w-rad, r.y+rad, rad, r.h-2*rad), color);
    SDL_SetRenderDrawColor(win->renderer,
        (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF, (color>>24)&0xFF);
    for (int dy = -rad; dy <= rad; dy++)
        for (int dx = -rad; dx <= rad; dx++)
            if (dx*dx + dy*dy <= rad*rad) {
                int px, py;
                px = r.x + rad + dx; py = r.y + rad + dy;
                if (px >= r.x && py >= r.y)
                    SDL_RenderDrawPoint(win->renderer, px, py);
                px = r.x + r.w - rad - 1 + dx; py = r.y + rad + dy;
                if (px < r.x + r.w && py >= r.y)
                    SDL_RenderDrawPoint(win->renderer, px, py);
                px = r.x + rad + dx; py = r.y + r.h - rad - 1 + dy;
                if (px >= r.x && py < r.y + r.h)
                    SDL_RenderDrawPoint(win->renderer, px, py);
                px = r.x + r.w - rad - 1 + dx;
                py = r.y + r.h - rad - 1 + dy;
                if (px < r.x + r.w && py < r.y + r.h)
                    SDL_RenderDrawPoint(win->renderer, px, py);
            }
}

void gc_draw_hline(gc_window_t *win, int x, int y, int w, gc_color_t color) {
    SDL_SetRenderDrawColor(win->renderer,
        (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF, (color>>24)&0xFF);
    SDL_RenderDrawLine(win->renderer, x, y, x + w - 1, y);
}

void gc_draw_vline(gc_window_t *win, int x, int y, int h, gc_color_t color) {
    SDL_SetRenderDrawColor(win->renderer,
        (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF, (color>>24)&0xFF);
    SDL_RenderDrawLine(win->renderer, x, y, x, y + h - 1);
}

static void _render_text(gc_window_t *win, gc_font_t *font,
                         const char *text, int x, int y,
                         gc_color_t color, int clip_w, bool centered,
                         gc_rect_t area) {
    if (!font || !text || !*text) return;
    SDL_Color fg = {
        (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF, (color>>24)&0xFF
    };
    char clip_buf[512];

    if (centered) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, fg);
        if (!surf) return;
        int tw = surf->w, th = surf->h;
        int cx = area.x + (area.w - tw) / 2;
        int cy = area.y + (area.h - th) / 2;
        SDL_Texture *tex = SDL_CreateTextureFromSurface(win->renderer, surf);
        SDL_Rect dst = { cx, cy, tw, th };
        if (clip_w > 0 && clip_w < tw) dst.w = clip_w;
        SDL_RenderCopy(win->renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
        return;
    }

    if (clip_w > 0) {
        int total = 0, pos = 0;
        for (const char *p = text; *p && pos < 510; p++) {
            int cw = 0;
            char ch[2] = {*p, 0};
            TTF_SizeUTF8(font, ch, &cw, NULL);
            if (total + cw > clip_w && pos > 0) break;
            total += cw; clip_buf[pos++] = *p;
        }
        clip_buf[pos] = '\0';
        text = clip_buf;
    }

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, fg);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(win->renderer, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(win->renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void gc_draw_text(gc_window_t *win, gc_font_t *font, const char *text,
                  int x, int y, gc_color_t color) {
    _render_text(win, font, text, x, y, color, 0, false, (gc_rect_t){0});
}

void gc_draw_text_clipped(gc_window_t *win, gc_font_t *font, const char *text,
                          int x, int y, int clip_w, gc_color_t color) {
    _render_text(win, font, text, x, y, color, clip_w, false, (gc_rect_t){0});
}

void gc_draw_text_centered(gc_window_t *win, gc_font_t *font, const char *text,
                           gc_rect_t area, gc_color_t color) {
    _render_text(win, font, text, 0, 0, color, 0, true, area);
}

int gc_draw_text_wrapped(gc_window_t *win, gc_font_t *font, const char *text,
                          int x, int y, int max_w, int line_h,
                          gc_color_t color) {
    /* Word-wrap text to fit max_w pixels, return total height drawn */
    if (!font || !text || !*text || max_w <= 0) return 0;
    if (line_h <= 0) line_h = TTF_FontHeight(font) + 2;

    int last_space = -1;
    int line_start = 0;
    int total_h = 0;
    int len = strlen(text);

    for (int i = 0; i <= len; i++) {
        char c = text[i];
        if (c == ' ' || c == '\t') last_space = i - line_start;
        if (c == '\n' || c == '\0' || c == ' ') {
            /* Check if current word fits */
            char tmp[512];
            (void)snprintf(tmp, sizeof(tmp), "%.*s", i - line_start, text + line_start);
            {
                int cw = 0;
                TTF_SizeUTF8(font, tmp, &cw, NULL);
                if (cw > max_w && last_space >= 0) {
                    /* Wrap at last space */
                    int line_len = last_space;
                    char line_buf[512];
                    snprintf(line_buf, sizeof(line_buf), "%.*s", line_len, text + line_start);
                    _render_text(win, font, line_buf, x, y + total_h,
                                 color, 0, false, (gc_rect_t){0});
                    total_h += line_h;
                    line_start += line_len + 1;
                    last_space = -1;
                    i = line_start;
                    continue;
                }
            }
        }
        if (c == '\n') {
            char line_buf[512];
            snprintf(line_buf, sizeof(line_buf), "%.*s", i - line_start, text + line_start);
            _render_text(win, font, line_buf, x, y + total_h,
                         color, 0, false, (gc_rect_t){0});
            total_h += line_h;
            line_start = i + 1;
            last_space = -1;
            continue;
        }
        if (c == '\0' && i > line_start) {
            char line_buf[512];
            snprintf(line_buf, sizeof(line_buf), "%s", text + line_start);
            _render_text(win, font, line_buf, x, y + total_h,
                         color, 0, false, (gc_rect_t){0});
            total_h += line_h;
        }
    }
    return total_h;
}

/* ══════════════════════════════════════════════════════════════════════
 *  Events
 * ══════════════════════════════════════════════════════════════════════ */
int gc_poll_event(gc_window_t *win, gc_event_t *ev) {
    if (!win || !ev) return 0;
    SDL_Event sdl_ev;
    while (SDL_PollEvent(&sdl_ev)) {
        switch (sdl_ev.type) {
        case SDL_QUIT:
            ev->type = GC_EV_QUIT; return 1;
        case SDL_KEYDOWN:
            ev->type = GC_EV_KEY_DOWN;
            ev->key = sdl_ev.key.keysym.sym;
            ev->mod = sdl_ev.key.keysym.mod;
            if (ev->key == SDLK_BACKSPACE || ev->key == SDLK_RETURN ||
                ev->key == SDLK_ESCAPE || ev->key == SDLK_TAB)
                return 1;
            break;
        case SDL_KEYUP:
            ev->type = GC_EV_KEY_UP;
            ev->key = sdl_ev.key.keysym.sym;
            return 1;
        case SDL_TEXTINPUT:
            ev->type = GC_EV_TEXT_INPUT;
            strncpy(ev->text, sdl_ev.text.text, GC_MAX_TEXT - 1);
            ev->text[GC_MAX_TEXT - 1] = '\0';
            return 1;
        case SDL_MOUSEMOTION:
            ev->type = GC_EV_MOUSE_MOVE;
            ev->x = sdl_ev.motion.x;
            ev->y = sdl_ev.motion.y;
            return 1;
        case SDL_MOUSEBUTTONDOWN:
            ev->type = GC_EV_MOUSE_DOWN;
            ev->x = sdl_ev.button.x;
            ev->y = sdl_ev.button.y;
            ev->button = sdl_ev.button.button;
            return 1;
        case SDL_MOUSEBUTTONUP:
            ev->type = GC_EV_MOUSE_UP;
            ev->x = sdl_ev.button.x;
            ev->y = sdl_ev.button.y;
            ev->button = sdl_ev.button.button;
            return 1;
        case SDL_MOUSEWHEEL:
            ev->type = GC_EV_MOUSE_WHEEL;
            ev->wheel_delta = sdl_ev.wheel.y;
            return 1;
        case SDL_WINDOWEVENT:
            if (sdl_ev.window.event == SDL_WINDOWEVENT_RESIZED ||
                sdl_ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                win->w = sdl_ev.window.data1;
                win->h = sdl_ev.window.data2;
                ev->type = GC_EV_RESIZE;
                ev->resize_w = win->w;
                ev->resize_h = win->h;
                return 1;
            }
            break;
        }
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  Frame control
 * ══════════════════════════════════════════════════════════════════════ */
void gc_begin_frame(gc_window_t *win) {
    win->frame_start = SDL_GetTicks64();
    gc_fill_rect(win, gc_rect(0, 0, win->w, win->h), win->theme.bg);
}

void gc_save_screenshot(gc_window_t *win, const char *path) {
    int w, h;
    SDL_GetRendererOutputSize(win->renderer, &w, &h);
    if (w <= 0 || h <= 0) { w = win->w; h = win->h; }
    SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (surf) {
        SDL_RenderReadPixels(win->renderer, NULL, surf->format->format,
                             surf->pixels, surf->pitch);
        SDL_SaveBMP(surf, path);
        SDL_FreeSurface(surf);
        fprintf(stderr, "screenshot: %s (%dx%d)\n", path, w, h);
    }
}

void gc_end_frame(gc_window_t *win) {
    SDL_RenderPresent(win->renderer);
    if (win->frame_delay_ms > 0) {
        uint64_t elapsed = SDL_GetTicks64() - win->frame_start;
        if (elapsed < win->frame_delay_ms)
            SDL_Delay(win->frame_delay_ms - elapsed);
    }
}

void gc_set_fps(gc_window_t *win, int fps) {
    if (fps > 0) win->frame_delay_ms = 1000 / fps;
    else win->frame_delay_ms = 0;
}

uint64_t gc_now_ms(void) {
    return SDL_GetTicks64();
}
