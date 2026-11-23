/*
 * session_local_title.c — real C11 port of upstream Hermes gateway/session_local_title.py.
 * PoP: w14_sessionloc_0c70e8_validate_title @ gateway/session_local_title.py:validate_title
 * Ported: validate_title + the sanitize_title core it delegates to (title
 * sanitization regexes, whitespace collapse, MAX_TITLE_LENGTH=100). The store
 * and authority-resolution paths are seam-scoped out.
 */
#include "hermes_json.h"
#include "noc_w14_sessionloc_0c70e8.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TITLE_LENGTH 100

static uint32_t dec_utf8(const unsigned char *s, size_t n, size_t *used) {
    unsigned char c = s[0];
    if (c < 0x80) { *used = 1; return c; }
    if ((c & 0xE0) == 0xC0 && n >= 2 && (s[1] & 0xC0) == 0x80) {
        *used = 2; return ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && n >= 3 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *used = 3; return ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) |
                         (uint32_t)(s[2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && n >= 4 && (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *used = 4; return ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
                         ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);
    }
    *used = 1; return c;
}

static size_t enc_utf8(uint32_t cp, char *out) {
    unsigned char *o = (unsigned char *)out;
    if (cp < 0x80) { o[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) { o[0] = (unsigned char)(0xC0 | (cp >> 6)); o[1] = (unsigned char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        o[0] = (unsigned char)(0xE0 | (cp >> 12));
        o[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (unsigned char)(0x80 | (cp & 0x3F)); return 3;
    }
    o[0] = (unsigned char)(0xF0 | (cp >> 18));
    o[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    o[3] = (unsigned char)(0x80 | (cp & 0x3F)); return 4;
}

static int is_ws_cp(uint32_t c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F) ||
           c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}
static int is_removed_cp(uint32_t c) {
    return (c >= 0x00 && c <= 0x08) || c == 0x0B || c == 0x0C || (c >= 0x0E && c <= 0x1F) || c == 0x7F;
}
static int is_invisible_cp(uint32_t c) {
    return (c >= 0x200B && c <= 0x200F) || (c >= 0x2028 && c <= 0x202E) ||
           (c >= 0x2060 && c <= 0x2069) || c == 0xFEFF || c == 0xFFFC || (c >= 0xFFF9 && c <= 0xFFFB);
}

/* Mirror of sanitize_title(): NULL means the cleaned title is None; *too_long
 * marks the ValueError branch. */
static char *sanitize_title_c(const char *title, int *too_long) {
    size_t n, i = 0, m = 0, j, r, cnt;
    char *buf, *res;
    int in_ws;
    if (!title || !title[0]) return NULL;
    n = strlen(title);
    buf = (char *)malloc(n * 4 + 8);
    if (!buf) return NULL;
    while (i < n) {
        size_t u;
        uint32_t cp = dec_utf8((const unsigned char *)title + i, n - i, &u);
        i += u;
        if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;       /* surrogate -> U+FFFD */
        else if (is_removed_cp(cp) || is_invisible_cp(cp)) continue;
        m += enc_utf8(cp, buf + m);
    }
    buf[m] = 0;
    /* re.sub(r"\s+", " ", cleaned) */
    res = (char *)malloc(m + 1);
    if (!res) { free(buf); return NULL; }
    r = 0; j = 0; in_ws = 0;
    while (j < m) {
        size_t u;
        uint32_t cp = dec_utf8((const unsigned char *)buf + j, m - j, &u);
        j += u;
        if (is_ws_cp(cp)) { if (!in_ws) { res[r++] = ' '; in_ws = 1; } }
        else { in_ws = 0; r += enc_utf8(cp, res + r); }
    }
    res[r] = 0;
    free(buf);
    buf = res; m = r;
    /* .strip() over the now-single-space whitespace */
    { size_t b = 0, e = m;
      while (b < e && buf[b] == ' ') b++;
      while (e > b && buf[e - 1] == ' ') e--;
      if (e > b) memmove(buf, buf + b, e - b);
      buf[e - b] = 0; m = e - b; }
    if (m == 0) { free(buf); return NULL; }
    /* len(cleaned) > MAX_TITLE_LENGTH -> ValueError (code points, not bytes) */
    cnt = 0; j = 0;
    while (j < m) { size_t u; dec_utf8((const unsigned char *)buf + j, m - j, &u); j += u; cnt++; }
    if (cnt > MAX_TITLE_LENGTH) { *too_long = 1; free(buf); return NULL; }
    return buf;
}

struct json_t *w14_sessionloc_0c70e8_validate_title(const struct json_t *title) {
    struct json_t *out = json_object();
    char *clean;
    int too_long = 0;
    /* validate_title only calls sanitize_title when isinstance(title, str). */
    if (!title || title->type != JSON_STRING) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "reason", json_string("invalid_params"));
        return out;
    }
    clean = sanitize_title_c(title->str_val ? title->str_val : "", &too_long);
    if (!clean || too_long) {
        free(clean);
        json_set(out, "ok", json_bool(0));
        json_set(out, "reason", json_string("invalid_params"));
        return out;
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "title", json_string(clean));
    free(clean);
    return out;
}
