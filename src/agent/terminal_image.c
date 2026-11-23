/*
 * terminal_image.c — C11 port of the pure string/color/grid helpers in upstream
 * Hermes agent/terminal_image.py.
 *
 * PoP: w7_terminalima_83613a_kitty_color_hex @ agent/terminal_image.py:kitty_color_hex
 * PoP: w7_terminalima_83613a_cell_box @ agent/terminal_image.py:cell_box
 * PoP: w7_terminalima_83613a_kitty_apc @ agent/terminal_image.py:_kitty_apc
 * PoP: w7_terminalima_83613a_grid_for @ agent/terminal_image.py:_grid_for
 * PoP: w7_terminalima_83613a_kitty_placeholder_rows @ agent/terminal_image.py:kitty_placeholder_rows
 * PoP: w7_terminalima_83613a_detect_graphics @ agent/terminal_image.py:detect_terminal_graphics
 * PoP: w7_terminalima_83613a_supports_kitty_placeholders @ agent/terminal_image.py:supports_kitty_placeholders
 *
 * PIL-dependent encoders (encode_kitty/iterm/sixel/unicode) need a decoded
 * image and have no pure C analogue; the deterministic escape/grid layer is
 * the gate's semantic core. Env reads use an explicit seam.
 */
#include "noc_w7_terminalima_83613a.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CELL_W 8
#define CELL_H 16
#define PLACEHOLDER "\xf4\x8e\xbb\xae" /* U+10EEEE */

/* Row diacritics by index, verbatim from kitty's gen/rowcolumn-diacritics.txt. */
static const unsigned int ROWCOL_DIACRITICS[] = {
    0x0305, 0x030D, 0x030E, 0x0310, 0x0312, 0x033D, 0x033E, 0x033F, 0x0346, 0x034A,
    0x034B, 0x034C, 0x0350, 0x0351, 0x0352, 0x0357, 0x035B, 0x0363, 0x0364, 0x0365,
    0x0366, 0x0367, 0x0368, 0x0369, 0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F,
    0x0483, 0x0484, 0x0485, 0x0486, 0x0487, 0x0592, 0x0593, 0x0594, 0x0595, 0x0597,
    0x0598, 0x0599, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1, 0x05A8, 0x05A9,
    0x05AB, 0x05AC, 0x05AF, 0x05C4, 0x0610, 0x0611, 0x0612, 0x0613, 0x0614, 0x0615,
    0x0616, 0x0617, 0x0657, 0x0658, 0x0659, 0x065A, 0x065B, 0x065D, 0x065E, 0x06D6,
    0x06D7, 0x06D8, 0x06D9, 0x06DA, 0x06DB, 0x06DC, 0x06DF, 0x06E0, 0x06E1, 0x06E2,
    0x06E4, 0x06E7, 0x06E8, 0x06EB, 0x06EC, 0x0730, 0x0732, 0x0733, 0x0735, 0x0736,
    0x073A, 0x073D, 0x073F, 0x0740, 0x0741, 0x0743, 0x0745, 0x0747, 0x0749, 0x074A,
    0x07EB, 0x07EC, 0x07ED, 0x07EE, 0x07EF, 0x07F0, 0x07F1, 0x07F3, 0x0816, 0x0817,
    0x0818, 0x0819, 0x081B, 0x081C, 0x081D, 0x081E, 0x081F, 0x0820, 0x0821, 0x0822,
    0x0823, 0x0825, 0x0826, 0x0827, 0x0829, 0x082A, 0x082B, 0x082C, 0x082D, 0x0951,
    0x0953, 0x0954, 0x0F82, 0x0F83, 0x0F86, 0x0F87, 0x135D, 0x135E, 0x135F, 0x17DD,
    0x193A, 0x1A17, 0x1A75, 0x1A76, 0x1A77, 0x1A78, 0x1A79, 0x1A7A, 0x1A7B, 0x1A7C,
    0x1B6B, 0x1B6D, 0x1B6E, 0x1B6F, 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1CD0, 0x1CD1,
    0x1CD2, 0x1CDA, 0x1CDB, 0x1CE0, 0x1DC0, 0x1DC1, 0x1DC3, 0x1DC4, 0x1DC5, 0x1DC6,
    0x1DC7, 0x1DC8, 0x1DC9, 0x1DCB, 0x1DCC, 0x1DD1, 0x1DD2, 0x1DD3, 0x1DD4, 0x1DD5,
    0x1DD6, 0x1DD7, 0x1DD8, 0x1DD9, 0x1DDA, 0x1DDB, 0x1DDC, 0x1DDD, 0x1DDE, 0x1DDF,
    0x1DE0, 0x1DE1, 0x1DE2, 0x1DE3, 0x1DE4, 0x1DE5, 0x1DE6, 0x1DFE, 0x20D0, 0x20D1,
    0x20D4, 0x20D5, 0x20D6, 0x20D7, 0x20DB, 0x20DC, 0x20E1, 0x20E7, 0x20E9, 0x20F0,
    0x2CEF, 0x2CF0, 0x2CF1, 0x2DE0, 0x2DE1, 0x2DE2, 0x2DE3, 0x2DE4, 0x2DE5, 0x2DE6,
    0x2DE7, 0x2DE8, 0x2DE9, 0x2DEA, 0x2DEB, 0x2DEC, 0x2DED, 0x2DEE, 0x2DEF, 0x2DF0,
    0x2DF1, 0x2DF2, 0x2DF3, 0x2DF4, 0x2DF5, 0x2DF6, 0x2DF7, 0x2DF8, 0x2DF9, 0x2DFA,
    0x2DFB, 0x2DFC, 0x2DFD, 0x2DFE, 0x2DFF, 0xA66F, 0xA67C, 0xA67D, 0xA6F0, 0xA6F1,
    0xA8E0, 0xA8E1, 0xA8E2, 0xA8E3, 0xA8E4, 0xA8E5, 0xA8E6, 0xA8E7, 0xA8E8, 0xA8E9,
    0xA8EA, 0xA8EB, 0xA8EC, 0xA8ED, 0xA8EE, 0xA8EF, 0xA8F0, 0xA8F1, 0xAAB0, 0xAAB2,
    0xAAB3, 0xAAB7, 0xAAB8, 0xAABE, 0xAABF, 0xAAC1, 0xFE20, 0xFE21, 0xFE22, 0xFE23,
    0xFE24, 0xFE25, 0xFE26, 0x10A0F, 0x10A38, 0x1D185, 0x1D186, 0x1D187, 0x1D188, 0x1D189,
    0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243, 0x1D244,
};

static long long to_i64(const char *s) {
    if (!s || !*s) return 0;
    return strtoll(s, NULL, 10);
}

typedef struct { char *buf; size_t len, cap; } SB;
static void sb_add(SB *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = (char *)realloc(b->buf, cap);
        if (!p) return;
        b->buf = p; b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
static void sb_puts(SB *b, const char *s) { if (s) sb_add(b, s, strlen(s)); }

/* UTF-8 encode one code point into a 5-byte buffer. */
static size_t utf8(unsigned int cp, char out[5]) {
    if (cp < 0x80) { out[0]=(char)cp; return 1; }
    if (cp < 0x800) { out[0]=(char)(0xC0|(cp>>6)); out[1]=(char)(0x80|(cp&0x3F)); return 2; }
    if (cp < 0x10000) { out[0]=(char)(0xE0|(cp>>12)); out[1]=(char)(0x80|((cp>>6)&0x3F)); out[2]=(char)(0x80|(cp&0x3F)); return 3; }
    out[0]=(char)(0xF0|(cp>>18)); out[1]=(char)(0x80|((cp>>12)&0x3F));
    out[2]=(char)(0x80|((cp>>6)&0x3F)); out[3]=(char)(0x80|(cp&0x3F)); return 4;
}

char *w7_terminalima_83613a_kitty_color_hex(const char *image_id) {
    unsigned int v = (unsigned int)to_i64(image_id) & 0xFFFFFFu;
    char out[16];
    snprintf(out, sizeof(out), "#%06x", v);
    return (char *)strdup(out);
}

json_t *w7_terminalima_83613a_cell_box(const char *width, const char *height) {
    long long w = to_i64(width), h = to_i64(height);
    long long cols = w / CELL_W < 1 ? 1 : w / CELL_W;
    long long rows = h / CELL_H < 1 ? 1 : h / CELL_H;
    json_t *out = json_array();
    json_append(out, json_number((double)cols));
    json_append(out, json_number((double)rows));
    return out;
}

char *w7_terminalima_83613a_kitty_apc(const char *ctrl, const char *data) {
    if (!data) data = "";
    size_t n = strlen(data);
    size_t pieces = n ? (n + 4095) / 4096 : 1;
    SB b = {0};
    for (size_t i = 0; i < pieces; i++) {
        sb_puts(&b, "\x1b_G");
        if (i == 0) { sb_puts(&b, ctrl ? ctrl : ""); sb_puts(&b, ","); }
        sb_puts(&b, "m=");
        sb_puts(&b, i + 1 < pieces ? "1;" : "0;");
        size_t off = i * 4096, len = n - off < 4096 ? n - off : 4096;
        sb_add(&b, data + off, len);
        sb_puts(&b, "\x1b\\");
    }
    if (!b.buf) b.buf = (char *)strdup("");
    return b.buf;
}

json_t *w7_terminalima_83613a_grid_for(const char *width, const char *height,
                                       const char *max_cols, const char *max_rows) {
    long long w = to_i64(width), h = to_i64(height);
    long long mc = to_i64(max_cols), mr = to_i64(max_rows);
    long long natural = (w + CELL_W - 1) / CELL_W; /* -(-width // CELL_W) for w>=0 */
    long long cols = natural < mc ? natural : mc;
    if (cols < 1) cols = 1;
    double e1 = ((double)cols * (double)h / (double)w) * CELL_W / CELL_H;
    long long rows = (long long)nearbyint(e1);
    if (rows < 1) rows = 1;
    if (rows > mr) {
        rows = mr;
        double e2 = ((double)rows * (double)w / (double)h) * CELL_H / CELL_W;
        long long capped = (long long)nearbyint(e2);
        cols = cols < capped ? cols : capped;
        if (cols < 1) cols = 1;
    }
    json_t *out = json_array();
    json_append(out, json_number((double)cols));
    json_append(out, json_number((double)rows));
    return out;
}

json_t *w7_terminalima_83613a_kitty_placeholder_rows(const char *cols, const char *rows) {
    long long c = to_i64(cols), r = to_i64(rows);
    if (c < 1) c = 1;
    if (r < 1) r = 1;
    json_t *out = json_array();
    for (long long i = 0; i < r; i++) {
        SB b = {0};
        sb_puts(&b, PLACEHOLDER);
        char dia[5];
        size_t idx = (size_t)(i < (long long)(sizeof(ROWCOL_DIACRITICS)/sizeof(ROWCOL_DIACRITICS)[0]) ? i : (long long)(sizeof(ROWCOL_DIACRITICS)/sizeof(ROWCOL_DIACRITICS)[0]) - 1);
        size_t dn = utf8(ROWCOL_DIACRITICS[idx], dia);
        sb_add(&b, dia, dn);
        for (long long k = 1; k < c; k++) sb_puts(&b, PLACEHOLDER);
        if (!b.buf) b.buf = (char *)strdup("");
        json_append(out, json_string(b.buf));
        free(b.buf);
    }
    return out;
}

static void lower_copy(char *dst, size_t cap, const char *s) {
    size_t i = 0;
    if (!s) s = "";
    for (; s[i] && i + 1 < cap; i++) dst[i] = (char)tolower((unsigned char)s[i]);
    dst[i] = '\0';
}
static bool contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}
static bool wezterm(const char *tp, const char *wp) {
    return (!strcmp(tp, "wezterm")) || (wp && *wp);
}

const char *w7_terminalima_83613a_detect_graphics(const char *term, const char *term_program,
                                                  const char *kitty_window_id,
                                                  const char *iterm_session_id,
                                                  const char *wezterm_pane) {
    char t[512], tp[512];
    lower_copy(t, sizeof(t), term);
    lower_copy(tp, sizeof(tp), term_program);
    if (!strcmp(tp, "vscode")) return "unicode";
    if ((kitty_window_id && *kitty_window_id) || contains(t, "kitty") || contains(t, "ghostty") ||
        !strcmp(tp, "ghostty") || wezterm(tp, wezterm_pane)) return "kitty";
    if (!strcmp(tp, "iterm.app") || (iterm_session_id && *iterm_session_id)) return "iterm";
    if (!strcmp(tp, "mintty") || contains(t, "foot") || contains(t, "mlterm") || contains(t, "sixel")) return "sixel";
    return "unicode";
}

bool w7_terminalima_83613a_supports_kitty_placeholders(const char *term, const char *term_program,
                                                       const char *kitty_window_id,
                                                       const char *iterm_session_id,
                                                       const char *wezterm_pane) {
    const char *g = w7_terminalima_83613a_detect_graphics(term, term_program,
                                                          kitty_window_id, iterm_session_id, wezterm_pane);
    char tp[512];
    lower_copy(tp, sizeof(tp), term_program);
    return !strcmp(g, "kitty") && !wezterm(tp, wezterm_pane);
}

