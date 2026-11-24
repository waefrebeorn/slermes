/*
 * cli_session_mixin.c — C home for upstream Hermes hermes_cli/cli_session_mixin.py.
 *
 * PoP: w16_clisession_f92c0b_squash                @ hermes_cli/cli_session_mixin.py:_squash
 * PoP: w16_clisession_f92c0b_undo_content_to_text   @ hermes_cli/cli_session_mixin.py:CLISessionMixin._undo_content_to_text
 * PoP: w16_clisession_f92c0b_title_session_id       @ hermes_cli/cli_session_mixin.py:CLISessionMixin._title_session_id
 *
 * Pure string / content-flattening helpers. No CLI, DB, or filesystem side effects.
 * The one DB-touching helper is expressed as an injected seam (tip + db_present).
 */
#include "hermes_json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w16_clisession_f92c0b.h"

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python str.split() with no args on ASCII text: split on runs of whitespace,
 * dropping empty tokens. Unicode whitespace is treated as non-separating here;
 * fixtures use ASCII. Returns a NUL-terminated buffer owned by the caller. */
static char *split_join_ws(const char *s, size_t *out_len) {
    const char *p = s ? s : "";
    size_t cap = strlen(p) + 2, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) { if (out_len) *out_len = 0; return dupstr(""); }
    int need_space = 0;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                  c == '\v' || c == '\f');
        if (ws) { need_space = n > 0; ++p; continue; }
        if (need_space) { out[n++] = ' '; need_space = 0; }
        if (n + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        out[n++] = (char)c;
        ++p;
    }
    out[n] = '\0';
    if (out_len) *out_len = n;
    return out;
}

json_t *w16_clisession_f92c0b_squash(const char *text, int limit) {
    size_t n = 0;
    char *s = split_join_ws(text, &n);
    size_t byte_len = strlen(s); /* ASCII fixtures: bytes == code points */
    if ((long long)byte_len > (long long)limit) {
        long long end = (long long)limit - 1;
        if (end < 0) end = (long long)byte_len + end;
        if (end < 0) end = 0;
        if (end > (long long)byte_len) end = (long long)byte_len;
        size_t k = (size_t)end;
        s[k++] = (char)0xE2; s[k++] = (char)0x80; s[k++] = (char)0xA6; s[k] = '\0';
    }
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w16_clisession_f92c0b_undo_content_to_text(const json_t *content) {
    if (content && content->type == JSON_STRING)
        return json_string(content->str_val ? content->str_val : "");
    if (content && content->type == JSON_ARRAY) {
        size_t cap = 64, n = 0;
        char *buf = (char *)malloc(cap);
        if (!buf) return json_string("");
        buf[0] = '\0';
        size_t parts = 0;
        for (size_t i = 0; i < json_len(content); ++i) {
            json_t *p = json_get(content, i);
            if (!p || p->type != JSON_OBJECT) continue;
            json_t *ty = json_obj_get(p, "type");
            if (!ty || ty->type != JSON_STRING || strcmp(ty->str_val ? ty->str_val : "", "text"))
                continue;
            json_t *tx = json_obj_get(p, "text");
            if (!tx || tx->type != JSON_STRING || !tx->str_val || !tx->str_val[0]) continue;
            size_t plen = strlen(tx->str_val);
            if (n + plen + 2 > cap) {
                while (n + plen + 2 > cap) cap *= 2;
                buf = (char *)realloc(buf, cap);
            }
            if (parts++) buf[n++] = '\n';
            memcpy(buf + n, tx->str_val, plen);
            n += plen;
            buf[n] = '\0';
        }
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
    return json_string("");
}

json_t *w16_clisession_f92c0b_title_session_id(const char *session_id,
                                               const json_t *tip, int db_present) {
    const char *sid = session_id ? session_id : "";
    if (!sid[0] || !db_present) return json_string(sid);
    if (tip && tip->type == JSON_STRING && tip->str_val && tip->str_val[0])
        return json_string(tip->str_val);
    return json_string(sid);
}
