/*
 * slv_ui.c -- a Lua-driven terminal UI host built on slerm-Lua.
 *
 * See slv_ui.h for the rationale: this is the seed of replacing curses (and
 * SDL, and C#) with Lua as the UI substrate, per the WWW/Wua direction. The
 * ncurses TUI cannot build on Windows at all, so a Lua host is the route
 * forward rather than a rival to it.
 *
 * The buffer belongs to the host. Lua reaches it only through the `term`
 * table, so a script can draw but cannot corrupt host state.
 */
#include "slv_ui.h"

#include <stdlib.h>
#include <string.h>

struct slv_ui {
    int   w, h;
    char *cells;   /* w * h, row-major, spaces mean "unset" */
    slv  *L;
};

/* ------------------------------------------------------------ the cfns */

static slv_value ui_term_clear(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n;
    slv_ui *ui = (slv_ui *)ud;
    memset(ui->cells, ' ', (size_t)ui->w * (size_t)ui->h);
    return slv_nil();
}

static slv_value ui_term_fill(slv *L, slv_value *a, int n, void *ud) {
    (void)L;
    slv_ui *ui = (slv_ui *)ud;
    if (n < 1) return slv_nil();
    const char *s = slv_tostring(a[0]);
    char ch = (s && s[0]) ? s[0] : ' ';
    memset(ui->cells, ch, (size_t)ui->w * (size_t)ui->h);
    return slv_nil();
}

static slv_value ui_term_put(slv *L, slv_value *a, int n, void *ud) {
    slv_ui *ui = (slv_ui *)ud;
    if (n < 3) return slv_nil();

    int okx = 0, oky = 0;
    int x = (int)slv_tointeger(a[0], &okx);
    int y = (int)slv_tointeger(a[1], &oky);
    if (!okx || !oky) return slv_nil();

    const char *s = slv_tostring(a[2]);
    if (!s) return slv_nil();

    for (size_t i = 0; s[i]; i++) {
        int cx = x + (int)i;
        if (cx < 0 || cx >= ui->w) break;   /* clip at the right edge */
        if (y < 0 || y >= ui->h) break;
        ui->cells[(size_t)y * (size_t)ui->w + (size_t)cx] = s[i];
    }
    return slv_nil();
}

static slv_value ui_term_get(slv *L, slv_value *a, int n, void *ud) {
    slv_ui *ui = (slv_ui *)ud;
    if (n < 2) return slv_nil();

    int okx = 0, oky = 0;
    int x = (int)slv_tointeger(a[0], &okx);
    int y = (int)slv_tointeger(a[1], &oky);
    if (!okx || !oky) return slv_nil();
    if (x < 0 || x >= ui->w || y < 0 || y >= ui->h) return slv_nil();

    char one[2];
    one[0] = ui->cells[(size_t)y * (size_t)ui->w + (size_t)x];
    one[1] = '\0';
    /* slv_str() stores the pointer without copying, so the single-character
     * result must be interned rather than returned from a stack buffer. */
    return slv_str(slv_intern(L, one, 1));
}

/* refresh() is a no-op here: the host renders on demand via slv_ui_dump().
 * It exists so a Lua script can mark a frame boundary without the host
 * needing to know when Lua thinks it is done drawing. */
static slv_value ui_term_refresh(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n; (void)ud;
    return slv_nil();
}

/* box(x, y, w, h [, title]) -- outline a rectangle with single-line borders.
 * A TUI is mostly boxes, so making this one call instead of four loops keeps
 * the Lua side readable. Anything off-screen is clipped. */
static slv_value ui_term_box(slv *L, slv_value *a, int n, void *ud) {
    slv_ui *ui = (slv_ui *)ud;
    if (n < 4) return slv_nil();

    int ok = 0;
    int x = (int)slv_tointeger(a[0], &ok); if (!ok) return slv_nil();
    int y = (int)slv_tointeger(a[1], &ok); if (!ok) return slv_nil();
    int w = (int)slv_tointeger(a[2], &ok); if (!ok) return slv_nil();
    int h = (int)slv_tointeger(a[3], &ok); if (!ok) return slv_nil();
    if (w < 2 || h < 2) return slv_nil();

    const char *title = (n >= 5) ? slv_tostring(a[4]) : NULL;

    /* corners */
    static const char *TL = "+", *TR = "+", *BL = "+", *BR = "+";
    static const char *HZ = "-", *VT = "|";
#define SLV_UI_PUT(px, py, s) \
    do { \
        int _x = (px), _y = (py); \
        if (_x >= 0 && _x < ui->w && _y >= 0 && _y < ui->h) \
            ui->cells[(size_t)_y * (size_t)ui->w + (size_t)_x] = (s)[0]; \
    } while (0)

    SLV_UI_PUT(x,         y,         TL);
    SLV_UI_PUT(x + w - 1, y,         TR);
    SLV_UI_PUT(x,         y + h - 1, BL);
    SLV_UI_PUT(x + w - 1, y + h - 1, BR);
    for (int i = 1; i < w - 1; i++) { SLV_UI_PUT(x + i, y, HZ); SLV_UI_PUT(x + i, y + h - 1, HZ); }
    for (int j = 1; j < h - 1; j++) { SLV_UI_PUT(x,     y + j, VT); SLV_UI_PUT(x + w - 1, y + j, VT); }
#undef SLV_UI_PUT

    if (title) {
        /* indent the title one cell inside the top edge, like a panel caption */
        for (size_t i = 0; title[i] && i + 2 < (size_t)w; i++)
            ui->cells[(size_t)y * (size_t)ui->w + (size_t)(x + 1 + (int)i)] = title[i];
    }
    return slv_nil();
}

/* hline / vline -- one call for rules and column separators. */
static slv_value ui_term_hline(slv *L, slv_value *a, int n, void *ud) {
    (void)L;
    slv_ui *ui = (slv_ui *)ud;
    if (n < 3) return slv_nil();
    int ok = 0;
    int x = (int)slv_tointeger(a[0], &ok); if (!ok) return slv_nil();
    int y = (int)slv_tointeger(a[1], &ok); if (!ok) return slv_nil();
    int len = (int)slv_tointeger(a[2], &ok); if (!ok) return slv_nil();
    for (int i = 0; i < len; i++) {
        int cx = x + i;
        if (cx < 0 || cx >= ui->w || y < 0 || y >= ui->h) break;
        ui->cells[(size_t)y * (size_t)ui->w + (size_t)cx] = '-';
    }
    return slv_nil();
}

static slv_value ui_term_vline(slv *L, slv_value *a, int n, void *ud) {
    (void)L;
    slv_ui *ui = (slv_ui *)ud;
    if (n < 3) return slv_nil();
    int ok = 0;
    int x = (int)slv_tointeger(a[0], &ok); if (!ok) return slv_nil();
    int y = (int)slv_tointeger(a[1], &ok); if (!ok) return slv_nil();
    int len = (int)slv_tointeger(a[2], &ok); if (!ok) return slv_nil();
    for (int j = 0; j < len; j++) {
        int cy = y + j;
        if (x < 0 || x >= ui->w || cy < 0 || cy >= ui->h) break;
        ui->cells[(size_t)cy * (size_t)ui->w + (size_t)x] = '|';
    }
    return slv_nil();
}

/* scroll([n]) -- push the buffer up by n rows (default 1), clearing the
 * bottom. A chat pane is just a scroll plus a redraw. */
static slv_value ui_term_scroll(slv *L, slv_value *a, int n, void *ud) {
    (void)L;
    slv_ui *ui = (slv_ui *)ud;
    int lines = 1;
    if (n >= 1) {
        int ok = 0;
        lines = (int)slv_tointeger(a[0], &ok);
        if (!ok || lines <= 0) lines = 1;
    }
    if (lines >= ui->h) { memset(ui->cells, ' ', (size_t)ui->w * (size_t)ui->h); return slv_nil(); }
    size_t rows = (size_t)(ui->h - lines);
    memmove(ui->cells, ui->cells + (size_t)lines * (size_t)ui->w, rows * (size_t)ui->w);
    memset(ui->cells + rows * (size_t)ui->w, ' ', (size_t)lines * (size_t)ui->w);
    return slv_nil();
}

/* row(y) -- the whole row as one string, so Lua can compose a line and read it
 * back without a per-cell loop. Trailing spaces are significant here. */
static slv_value ui_term_row(slv *L, slv_value *a, int n, void *ud) {
    slv_ui *ui = (slv_ui *)ud;
    if (n < 1) return slv_nil();
    int ok = 0;
    int y = (int)slv_tointeger(a[0], &ok);
    if (!ok || y < 0 || y >= ui->h) return slv_nil();
    return slv_str(slv_intern(L, ui->cells + (size_t)y * (size_t)ui->w, (size_t)ui->w));
}

/* ------------------------------------------------------------- hosting */

static void ui_register_term(slv_ui *ui) {
    slv *L = ui->L;

    slv_table *t = slv_table_new(L, 0, 8);
    if (!t) return;

    struct { const char *name; slv_cfn fn; } methods[] = {
        { "clear",   ui_term_clear   },
        { "fill",    ui_term_fill    },
        { "put",     ui_term_put     },
        { "get",     ui_term_get     },
        { "refresh", ui_term_refresh },
        { "box",     ui_term_box     },
        { "hline",   ui_term_hline   },
        { "vline",   ui_term_vline   },
        { "scroll",  ui_term_scroll  },
        { "row",     ui_term_row     },
    };
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        slv_value v = slv_nil();
        v.t = SLV_CFUNCTION;
        v.as.cf.f  = methods[i].fn;
        v.as.cf.ud = ui;
        slv_table_set(L, t, slv_str(methods[i].name), v);
    }

    /* width / height are plain values, not functions. */
    slv_table_set(L, t, slv_str("width"),  slv_integer(ui->w));
    slv_table_set(L, t, slv_str("height"), slv_integer(ui->h));

    slv_value tv = slv_nil();
    tv.t = SLV_TABLE;
    tv.as.tab = t;
    slv_set_global(L, "term", tv);
}

slv_ui *slv_ui_new(int w, int h) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return NULL;

    slv_ui *ui = (slv_ui *)calloc(1, sizeof(*ui));
    if (!ui) return NULL;

    ui->w = w;
    ui->h = h;
    ui->cells = (char *)malloc((size_t)w * (size_t)h);
    if (!ui->cells) { free(ui); return NULL; }
    memset(ui->cells, ' ', (size_t)w * (size_t)h);

    ui->L = slv_new();
    if (!ui->L) { free(ui->cells); free(ui); return NULL; }

    slv_openlibs(ui->L);
    ui_register_term(ui);
    return ui;
}

void slv_ui_free(slv_ui *ui) {
    if (!ui) return;
    if (ui->L) slv_free(ui->L);
    free(ui->cells);
    free(ui);
}

const char *slv_ui_run(slv_ui *ui, const char *src, const char *chunkname) {
    if (!ui || !src) return "bad arguments";
    return slv_run(ui->L, src, chunkname ? chunkname : "=slv_ui");
}

int         slv_ui_width(const slv_ui *ui)  { return ui ? ui->w : 0; }
int         slv_ui_height(const slv_ui *ui) { return ui ? ui->h : 0; }

char slv_ui_cell(const slv_ui *ui, int x, int y) {
    if (!ui || x < 0 || x >= ui->w || y < 0 || y >= ui->h) return '\0';
    return ui->cells[(size_t)y * (size_t)ui->w + (size_t)x];
}

const char *slv_ui_line(const slv_ui *ui, int y) {
    if (!ui || y < 0 || y >= ui->h) return "";
    return ui->cells + (size_t)y * (size_t)ui->w;   /* NOT NUL-terminated */
}

void slv_ui_dump(const slv_ui *ui, FILE *out) {
    if (!ui || !out) return;
    for (int y = 0; y < ui->h; y++) {
        const char *row = ui->cells + (size_t)y * (size_t)ui->w;
        int last = ui->w - 1;
        while (last >= 0 && row[last] == ' ') last--;
        for (int x = 0; x <= last; x++) fputc(row[x], out);
        fputc('\n', out);
    }
}

slv *slv_ui_state(slv_ui *ui) { return ui ? ui->L : NULL; }
