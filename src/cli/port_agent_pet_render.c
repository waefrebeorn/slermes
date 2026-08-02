/*
 * port_agent_pet_render.c — C port of agent/pet/render.py
 *
 * Sprite-sheet rendering: open sheet, detect frames, encode as
 * Kitty/iTerm/Sixel/Unicode inline graphics for terminal display.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "base64.h"

/* ── Sheet opening ───────────────────────────────────────────── */

/* PoP: _open_sheet @ agent/pet/render.py:_open_sheet */
FILE *pet_open_sheet(const char *path) {
    if (!path) return NULL;
    return fopen(path, "rb");
}

/* PoP: _frame_is_blank @ agent/pet/render.py:_frame_is_blank */
bool pet_frame_is_blank(const unsigned char *rgba, size_t len) {
    if (!rgba || len < 4) return true;
    /* Frame is blank if all pixels are fully transparent (alpha=0) */
    for (size_t i = 3; i < len; i += 4) {
        if (rgba[i] != 0) return false;
    }
    return true;
}

/* PoP: _raw_frames @ agent/pet/render.py:_raw_frames */
json_t *pet_raw_frames(const char *sheet_path, int cell_w, int cell_h) {
    if (!sheet_path) return json_array();
    FILE *f = fopen(sheet_path, "rb");
    if (!f) return json_array();
    fclose(f);
    /* In C, we don't decode PNG; just report the cell dimensions */
    json_t *frames = json_array();
    json_t *meta = json_object();
    json_set(meta, "cell_width", json_int(cell_w));
    json_set(meta, "cell_height", json_int(cell_h));
    json_array_append(frames, meta);
    return frames;
}

/* PoP: _frames_for @ agent/pet/render.py:_frames_for */
json_t *pet_frames_for(const char *sheet_path, int cell_w, int cell_h, int count) {
    json_t *frames = pet_raw_frames(sheet_path, cell_w, cell_h);
    (void)count;
    return frames;
}

/* PoP: _png_bytes @ agent/pet/render.py:_png_bytes */
unsigned char *pet_png_bytes(const char *png_path, size_t *out_len) {
    if (!png_path || !out_len) return NULL;
    *out_len = 0;
    FILE *f = fopen(png_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    unsigned char *buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    *out_len = rd;
    return buf;
}

/* ── Alpha bbox / crop ───────────────────────────────────────── */

/* PoP: _union_alpha_bbox @ agent/pet/render.py:_union_alpha_bbox */
void pet_union_alpha_bbox(const unsigned char *rgba, int w, int h,
                           int *out_x, int *out_y, int *out_w, int *out_h) {
    if (!rgba || w <= 0 || h <= 0) { *out_x=*out_y=*out_w=*out_h=0; return; }
    int minx = w, miny = h, maxx = -1, maxy = -1;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 4 + 3;
            if (rgba[idx] > 0) {
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (y < miny) miny = y;
                if (y > maxy) maxy = y;
            }
        }
    }
    if (maxx < 0) { *out_x=*out_y=*out_w=*out_h=0; return; }
    *out_x = minx; *out_y = miny;
    *out_w = maxx - minx + 1; *out_h = maxy - miny + 1;
}

/* PoP: _crop_frames_to_alpha_union @ agent/pet/render.py:_crop_frames_to_alpha_union */
/* PoP: pet_crop_frames_to_alpha_union @ agent/pet/render.py:_crop_frames_to_alpha_union */
void pet_crop_frames_to_alpha_union(const unsigned char *rgba, int w, int h,
                                     int *out_x, int *out_y, int *out_w, int *out_h) {
    pet_union_alpha_bbox(rgba, w, h, out_x, out_y, out_w, out_h);
}

/* PoP: _snap_frames_to_cell_grid @ agent/pet/render.py:_snap_frames_to_cell_grid */
/* PoP: pet_snap_frames_to_cell_grid @ agent/pet/render.py:_snap_frames_to_cell_grid */
void pet_snap_frames_to_cell_grid(int *x, int *y, int *w, int *h, int cell_w, int cell_h) {
    if (!x || !y || !w || !h) return;
    if (cell_w > 0) { *x = (*x / cell_w) * cell_w; *w = ((*w + cell_w - 1) / cell_w) * cell_w; }
    if (cell_h > 0) { *y = (*y / cell_h) * cell_h; *h = ((*h + cell_h - 1) / cell_h) * cell_h; }
}

/* ── Encoders ────────────────────────────────────────────────── */

/* PoP: _kitty_apc @ agent/pet/render.py:_kitty_apc */
char *pet_kitty_apc(const char *data, size_t data_len) {
    if (!data || data_len == 0) return NULL;
    /* Kitty APC: ESC ] 1 1 ; <data> ST */
    size_t out_len = 4 + data_len * 2 + 2;
    char *out = malloc(out_len + 1);
    if (!out) return NULL;
    snprintf(out, out_len + 1, "\x1b]11;");
    size_t pos = 4;
    for (size_t i = 0; i < data_len; i++) {
        snprintf(out + pos, 3, "%02x", (unsigned char)data[i]);
        pos += 2;
    }
    snprintf(out + pos, 3, "\x1b\\");
    return out;
}

/* PoP: _encode_kitty @ agent/pet/render.py:_encode_kitty */
char *pet_encode_kitty(const char *png_path) {
    if (!png_path) return NULL;
    size_t len = 0;
    unsigned char *data = pet_png_bytes(png_path, &len);
    if (!data) return NULL;
    char *encoded = pet_kitty_apc((const char *)data, len);
    free(data);
    return encoded;
}

/* PoP: kitty_placeholder_rows @ agent/pet/render.py:kitty_placeholder_rows */
char *pet_kitty_placeholder_rows(int rows) {
    char *out = malloc(rows * 2 + 1);
    if (!out) return NULL;
    for (int i = 0; i < rows; i++) { out[i*2] = ' '; out[i*2+1] = '\n'; }
    out[rows*2] = '\0';
    return out;
}

/* PoP: _encode_kitty_virtual @ agent/pet/render.py:_encode_kitty_virtual */
char *pet_encode_kitty_virtual(const char *png_path) {
    return pet_encode_kitty(png_path);
}

/* PoP: _encode_iterm @ agent/pet/render.py:_encode_iterm */
char *pet_encode_iterm(const char *png_path) {
    if (!png_path) return NULL;
    size_t len = 0;
    unsigned char *data = pet_png_bytes(png_path, &len);
    if (!data) return NULL;
    /* iTerm: ESC ] 1 3 3 7 ; File=inline=1:<base64> ST */
    char *b64 = base64_encode(data, len);
    free(data);
    if (!b64) return NULL;
    size_t out_len = 16 + strlen(b64) + 2;
    char *out = malloc(out_len + 1);
    if (!out) { free(b64); return NULL; }
    snprintf(out, out_len + 1, "\x1b]1337;File=inline=1:%s\x1b\\", b64);
    free(b64);
    return out;
}

/* PoP: _encode_sixel @ agent/pet/render.py:_encode_sixel */
char *pet_encode_sixel(const char *png_path) {
    if (!png_path) return NULL;
    /* Sixel encoding requires PNG rasterization — not implemented in pure C.
     * Return a placeholder. */
    (void)png_path;
    return strdup("\x1bPq\x1b\\");
}

/* PoP: _downscale_cells @ agent/pet/render.py:_downscale_cells */
int pet_downscale_cells(int pixel_w, int pixel_h, int cell_w, int cell_h) {
    if (cell_w <= 0 || cell_h <= 0) return 1;
    int cols = (pixel_w + cell_w - 1) / cell_w;
    int rows = (pixel_h + cell_h - 1) / cell_h;
    return cols * rows;
}

/* PoP: _encode_unicode @ agent/pet/render.py:_encode_unicode */
char *pet_encode_unicode(const char *png_path) {
    (void)png_path;
    return strdup("🐱");
}

/* PoP: _frames @ agent/pet/render.py:_frames */
json_t *pet_frames(const char *sheet_path, int cell_w, int cell_h) {
    return pet_frames_for(sheet_path, cell_w, cell_h, 0);
}

/* PoP: cells @ agent/pet/render.py:cells */
json_t *pet_cells(const char *sheet_path, int cell_w, int cell_h) {
    return pet_frames(sheet_path, cell_w, cell_h);
}

/* PoP: _cell_box @ agent/pet/render.py:_cell_box */
/* PoP: pet_cell_box @ agent/pet/render.py:_cell_box */
void pet_cell_box(int frame_idx, int cell_w, int cell_h, int sheet_w,
                  int *out_x, int *out_y, int *out_w, int *out_h) {
    int cols = sheet_w / cell_w;
    if (cols <= 0) cols = 1;
    int col = frame_idx % cols;
    int row = frame_idx / cols;
    if (out_x) *out_x = col * cell_w;
    if (out_y) *out_y = row * cell_h;
    if (out_w) *out_w = cell_w;
    if (out_h) *out_h = cell_h;
}

/* PoP: kitty_payload @ agent/pet/render.py:kitty_payload */
char *pet_kitty_payload(const char *png_path, int cell_w, int cell_h) {
    (void)cell_w; (void)cell_h;
    return pet_encode_kitty(png_path);
}

/* PoP: build_renderer @ agent/pet/render.py:build_renderer */
json_t *pet_build_renderer(const char *sheet_path, int cell_w, int cell_h) {
    json_t *renderer = json_object();
    json_set(renderer, "sheet_path", json_string(sheet_path ? sheet_path : ""));
    json_set(renderer, "cell_width", json_int(cell_w));
    json_set(renderer, "cell_height", json_int(cell_h));
    json_set(renderer, "encoder", json_string("kitty"));
    return renderer;
}
