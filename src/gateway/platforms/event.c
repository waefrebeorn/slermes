/*
 * event.c - C home for gateway/platforms/event.py.
 * Faithful port of the inbound message event: untrusted filename sanitising, the
 * Desktop attachment-ref strip behind slash-command detection, sender identity,
 * turn input/media absorption and interrupt marking.
 * PoP: w19_event_eea6c7_clean_attachment_filename @ gateway/platforms/event.py:clean_attachment_filename
 * PoP: w19_event_eea6c7_command_text @ gateway/platforms/event.py:MessageEvent._command_text
 * PoP: w19_event_eea6c7_is_command @ gateway/platforms/event.py:MessageEvent.is_command
 * PoP: w19_event_eea6c7_get_command @ gateway/platforms/event.py:MessageEvent.get_command
 * PoP: w19_event_eea6c7_get_command_args @ gateway/platforms/event.py:MessageEvent.get_command_args
 * PoP: w19_event_eea6c7_sender_id @ gateway/platforms/event.py:MessageEvent.sender_id
 * PoP: w19_event_eea6c7_absorb_reply_expected @ gateway/platforms/event.py:MessageEvent.absorb_reply_expected
 * PoP: w19_event_eea6c7_absorb_turn_input @ gateway/platforms/event.py:MessageEvent.absorb_turn_input
 * PoP: w19_event_eea6c7_extend_media @ gateway/platforms/event.py:MessageEvent.extend_media
 * PoP: w19_event_eea6c7_accept_interrupt @ gateway/platforms/event.py:MessageEvent.accept_interrupt
 */
#include "noc_w19_event_eea6c7.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define EV_MAX_FILENAME_CHARS 255

/* ---- UTF-8 <-> codepoint vectors (Python str indexes by codepoint) ---- */

typedef struct { uint32_t *cp; size_t n; } ev_cps;

static size_t ev_utf8_decode(const char *s, size_t i, uint32_t *out) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *out = c; return 1; }
    if ((c & 0xE0) == 0xC0 && s[i + 1]) {
        *out = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0 && s[i + 1] && s[i + 2]) {
        *out = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)((unsigned char)s[i + 1] & 0x3F) << 6)
             | ((unsigned char)s[i + 2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0 && s[i + 1] && s[i + 2] && s[i + 3]) {
        *out = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)((unsigned char)s[i + 1] & 0x3F) << 12)
             | ((uint32_t)((unsigned char)s[i + 2] & 0x3F) << 6)
             | ((unsigned char)s[i + 3] & 0x3F);
        return 4;
    }
    *out = c;
    return 1;
}

static size_t ev_encode(uint32_t cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static ev_cps ev_from_utf8(const char *s) {
    ev_cps out;
    size_t i = 0, len;
    out.cp = NULL;
    out.n = 0;
    if (!s) return out;
    len = strlen(s);
    out.cp = (uint32_t *)malloc((len + 1) * sizeof(uint32_t));
    if (!out.cp) return out;
    while (i < len) {
        uint32_t cp;
        i += ev_utf8_decode(s, i, &cp);
        out.cp[out.n++] = cp;
    }
    return out;
}

static char *ev_to_utf8(const uint32_t *cp, size_t n) {
    char *out = (char *)malloc(n * 4 + 1);
    size_t i, at = 0;
    if (!out) return NULL;
    for (i = 0; i < n; i++) at += ev_encode(cp[i], out + at);
    out[at] = '\0';
    return out;
}

static int ev_isspace(uint32_t c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') return 1;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return 1;
    if (c >= 0x2000 && c <= 0x200A) return 1;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return 1;
    return 0;
}

/* unicodedata.category(ch) in {"Cc", "Cf"}: control and format characters. */
static int ev_is_cc_cf(uint32_t c) {
    if (c <= 0x1F) return 1;
    if (c >= 0x7F && c <= 0x9F) return 1;
    if (c == 0xAD) return 1;
    if (c >= 0x600 && c <= 0x605) return 1;
    if (c == 0x61C || c == 0x6DD || c == 0x70F || c == 0x8E2) return 1;
    if (c >= 0x890 && c <= 0x891) return 1;
    if (c == 0x180E) return 1;
    if (c >= 0x200B && c <= 0x200F) return 1;
    if (c >= 0x202A && c <= 0x202E) return 1;
    if (c >= 0x2060 && c <= 0x2064) return 1;
    if (c >= 0x2066 && c <= 0x206F) return 1;
    if (c == 0xFEFF) return 1;
    if (c >= 0xFFF9 && c <= 0xFFFB) return 1;
    if (c == 0x110BD || c == 0x110CD) return 1;
    if (c >= 0x13430 && c <= 0x1343F) return 1;
    if (c >= 0x1BCA0 && c <= 0x1BCA3) return 1;
    if (c >= 0x1D173 && c <= 0x1D17A) return 1;
    if (c == 0xE0001) return 1;
    if (c >= 0xE0020 && c <= 0xE007F) return 1;
    return 0;
}

/* ---------------- clean_attachment_filename ---------------- */

json_t *w19_event_eea6c7_clean_attachment_filename(json_t *name) {
    ev_cps src;
    uint32_t *base;
    size_t i, start = 0, bn = 0;
    char *text;
    if (!name || name->type != JSON_STRING || !name->str_val) return json_string("");
    src = ev_from_utf8(name->str_val);
    if (!src.cp) return json_string("");
    /* name.replace("\\", "/").rsplit("/", 1)[-1] */
    for (i = 0; i < src.n; i++)
        if (src.cp[i] == '/' || src.cp[i] == '\\') start = i + 1;
    base = (uint32_t *)malloc((src.n + 1) * sizeof(uint32_t));
    if (!base) { free(src.cp); return json_string(""); }
    for (i = start; i < src.n; i++)
        if (!ev_is_cc_cf(src.cp[i])) base[bn++] = src.cp[i];
    /* .strip() */
    while (bn > 0 && ev_isspace(base[0])) {
        memmove(base, base + 1, (bn - 1) * sizeof(uint32_t));
        bn--;
    }
    while (bn > 0 && ev_isspace(base[bn - 1])) bn--;
    free(src.cp);
    if (bn == 0 || (bn == 1 && base[0] == '.')
        || (bn == 2 && base[0] == '.' && base[1] == '.')) {
        free(base);
        return json_string("");
    }
    if (bn > EV_MAX_FILENAME_CHARS) {
        size_t dot = (size_t)-1, keep_len = 0, len;
        for (i = 0; i < bn; i++)
            if (base[i] == '.') dot = i;
        if (dot != (size_t)-1 && dot > 0 && bn - dot - 1 <= 16) keep_len = bn - dot;
        len = EV_MAX_FILENAME_CHARS - keep_len;
        memmove(base + len, base + bn - keep_len, keep_len * sizeof(uint32_t));
        bn = len + keep_len;
    }
    text = ev_to_utf8(base, bn);
    free(base);
    if (!text) return json_string("");
    {
        json_t *out = json_string(text);
        free(text);
        return out;
    }
}

/* ---------------- attachment-ref strip ---------------- */

static int ev_match_word_ci(const uint32_t *cp, size_t n, size_t at, const char *word) {
    size_t i, wl = strlen(word);
    if (at + wl > n) return 0;
    for (i = 0; i < wl; i++) {
        uint32_t c = cp[at + i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if (c != (uint32_t)(unsigned char)word[i]) return 0;
    }
    return 1;
}

/* ^(?:@(?:image|file|url):[^\n]+\n?)+ - anchored at `at`; returns the new index or `at`. */
static size_t ev_skip_attachment_refs(const uint32_t *cp, size_t n, size_t at) {
    for (;;) {
        size_t p = at, q;
        if (p >= n || cp[p] != '@') return at;
        p++;
        if (ev_match_word_ci(cp, n, p, "image")) p += 5;
        else if (ev_match_word_ci(cp, n, p, "file")) p += 4;
        else if (ev_match_word_ci(cp, n, p, "url")) p += 3;
        else return at;
        if (p >= n || cp[p] != ':') return at;
        p++;
        q = p;
        while (q < n && cp[q] != '\n') q++;
        if (q == p) return at;      /* [^\n]+ needs at least one character */
        if (q < n && cp[q] == '\n') q++;
        at = q;
    }
}

static uint32_t *ev_command_text_cps(const char *text, size_t *out_n) {
    ev_cps src = ev_from_utf8(text);
    size_t i = 0;
    uint32_t *cp;
    if (!src.cp) { *out_n = 0; return NULL; }
    cp = src.cp;
    while (i < src.n && ev_isspace(cp[i])) i++;
    i = ev_skip_attachment_refs(cp, src.n, i);
    while (i < src.n && ev_isspace(cp[i])) i++;
    if (i > 0) memmove(cp, cp + i, (src.n - i) * sizeof(uint32_t));
    *out_n = src.n - i;
    return cp;
}

json_t *w19_event_eea6c7_command_text(const char *text) {
    size_t n = 0;
    uint32_t *cp = ev_command_text_cps(text, &n);
    char *s;
    json_t *out;
    if (!cp) return json_string("");
    s = ev_to_utf8(cp, n);
    free(cp);
    if (!s) return json_string("");
    out = json_string(s);
    free(s);
    return out;
}

static int ev_is_command_cp(const uint32_t *cp, size_t n, int allow) {
    return allow && n > 0 && cp[0] == '/';
}

json_t *w19_event_eea6c7_is_command(const char *text, int allow_gateway_control) {
    size_t n = 0;
    uint32_t *cp = ev_command_text_cps(text, &n);
    int r = ev_is_command_cp(cp, n, allow_gateway_control);
    free(cp);
    return json_bool(r);
}

json_t *w19_event_eea6c7_get_command(const char *text, int allow_gateway_control) {
    size_t n = 0, i, tn = 0;
    uint32_t *cp = ev_command_text_cps(text, &n);
    uint32_t *tok;
    char *s;
    json_t *out;
    if (!cp || !ev_is_command_cp(cp, n, allow_gateway_control)) { free(cp); return json_null(); }
    for (i = 1; i < n && !ev_isspace(cp[i]); i++) tn++;
    tok = (uint32_t *)malloc((tn + 1) * sizeof(uint32_t));
    if (!tok) { free(cp); return json_null(); }
    for (i = 0; i < tn; i++) {
        uint32_t c = cp[1 + i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        tok[i] = c;
    }
    free(cp);
    for (i = 0; i < tn; i++)
        if (tok[i] == '@') { tn = i; break; }
    for (i = 0; i < tn; i++)
        if (tok[i] == '/') { free(tok); return json_null(); }
    s = ev_to_utf8(tok, tn);
    free(tok);
    if (!s) return json_null();
    out = json_string(s);
    free(s);
    return out;
}

/* iOS auto-correct: U+2014 U+2014 -> "--", U+2014 -> "--", U+2013 -> "-" */
static char *ev_map_dashes(const char *raw) {
    size_t n = strlen(raw), i = 0, at = 0;
    char *res = (char *)malloc(n * 2 + 1);
    if (!res) return NULL;
    while (i < n) {
        if (i + 5 < n
            && (unsigned char)raw[i] == 0xE2 && (unsigned char)raw[i + 1] == 0x80
            && (unsigned char)raw[i + 2] == 0x94
            && (unsigned char)raw[i + 3] == 0xE2 && (unsigned char)raw[i + 4] == 0x80
            && (unsigned char)raw[i + 5] == 0x94) {
            res[at++] = '-';
            res[at++] = '-';
            i += 6;
            continue;
        }
        if (i + 2 < n && (unsigned char)raw[i] == 0xE2 && (unsigned char)raw[i + 1] == 0x80
            && (unsigned char)raw[i + 2] == 0x94) {
            res[at++] = '-';
            res[at++] = '-';
            i += 3;
            continue;
        }
        if (i + 2 < n && (unsigned char)raw[i] == 0xE2 && (unsigned char)raw[i + 1] == 0x80
            && (unsigned char)raw[i + 2] == 0x93) {
            res[at++] = '-';
            i += 3;
            continue;
        }
        res[at++] = raw[i++];
    }
    res[at] = '\0';
    return res;
}

json_t *w19_event_eea6c7_get_command_args(const char *text, int allow_gateway_control) {
    size_t n = 0, i, start = 0, an;
    uint32_t *cp = ev_command_text_cps(text, &n);
    json_t *out;
    if (!cp) return text ? json_string(text) : json_null();
    if (!ev_is_command_cp(cp, n, allow_gateway_control)) {
        free(cp);
        return text ? json_string(text) : json_null();
    }
    while (start < n && ev_isspace(cp[start])) start++;
    i = start;
    while (i < n && !ev_isspace(cp[i])) i++;
    while (i < n && ev_isspace(cp[i])) i++;
    an = (i < n) ? n - i : 0;
    {
        char *raw = ev_to_utf8(cp + (n - an), an);
        char *mapped;
        free(cp);
        if (!raw) return json_string("");
        mapped = ev_map_dashes(raw);
        free(raw);
        if (!mapped) return json_string("");
        out = json_string(mapped);
        free(mapped);
        return out;
    }
}

/* ---------------- identity / absorption ---------------- */

json_t *w19_event_eea6c7_sender_id(json_t *user_id, json_t *source_user_id) {
    if (user_id && user_id->type != JSON_NULL) {
        if (user_id->type == JSON_STRING) return json_string(user_id->str_val ? user_id->str_val : "");
        return json_null();
    }
    if (source_user_id && source_user_id->type == JSON_STRING)
        return json_string(source_user_id->str_val ? source_user_id->str_val : "");
    return json_null();
}

json_t *w19_event_eea6c7_absorb_reply_expected(json_t *self_reply_expected, json_t *other_reply_expected) {
    int self_is_true = self_reply_expected && self_reply_expected->type == JSON_BOOL
                    && self_reply_expected->bool_val;
    int other_is_false = other_reply_expected && other_reply_expected->type == JSON_BOOL
                      && !other_reply_expected->bool_val;
    if (!self_is_true && !other_is_false)
        return other_reply_expected ? json_copy(other_reply_expected) : json_null();
    return self_reply_expected ? json_copy(self_reply_expected) : json_null();
}

static int ev_str_eq(json_t *a, json_t *b) {
    const char *sa = (a && a->type == JSON_STRING) ? a->str_val : NULL;
    const char *sb = (b && b->type == JSON_STRING) ? b->str_val : NULL;
    if (!sa && !sb) return 1;
    if (!sa || !sb) return 0;
    return strcmp(sa, sb) == 0;
}

json_t *w19_event_eea6c7_absorb_turn_input(json_t *self_user_id, json_t *self_user_name,
                                          json_t *self_source_user_id, json_t *other_user_id,
                                          json_t *other_user_name, json_t *other_source_user_id,
                                          json_t *self_reply_expected, json_t *other_reply_expected) {
    json_t *out = json_object();
    json_t *reply = w19_event_eea6c7_absorb_reply_expected(self_reply_expected, other_reply_expected);
    json_t *self_sender = w19_event_eea6c7_sender_id(self_user_id, self_source_user_id);
    json_t *other_sender = w19_event_eea6c7_sender_id(other_user_id, other_source_user_id);
    if (ev_str_eq(self_sender, other_sender)) {
        json_set(out, "user_id", json_copy(self_user_id));
        json_set(out, "user_name", json_copy(self_user_name));
    } else {
        json_set(out, "user_id", json_string(""));
        json_set(out, "user_name", json_string(""));
    }
    json_set(out, "reply_expected", reply);
    json_free(self_sender);
    json_free(other_sender);
    return out;
}

json_t *w19_event_eea6c7_extend_media(json_t *self_urls, json_t *self_types, json_t *self_names,
                                      json_t *other_urls, json_t *other_types, json_t *other_names) {
    json_t *out = json_object();
    json_t *urls = json_array();
    json_t *types = json_array();
    json_t *names = json_array();
    size_t i;
    int have_names = ((self_names && self_names->type == JSON_ARRAY && json_len(self_names) > 0)
                   || (other_names && other_names->type == JSON_ARRAY && json_len(other_names) > 0));
    for (i = 0; self_urls && i < json_len(self_urls); i++)
        json_append(urls, json_copy(json_get(self_urls, i)));
    for (i = 0; other_urls && i < json_len(other_urls); i++)
        json_append(urls, json_copy(json_get(other_urls, i)));
    for (i = 0; self_types && i < json_len(self_types); i++)
        json_append(types, json_copy(json_get(self_types, i)));
    for (i = 0; other_types && i < json_len(other_types); i++)
        json_append(types, json_copy(json_get(other_types, i)));
    if (have_names) {
        /* pad(msg) = (list(names) + [""] * len(urls))[:len(urls)] */
        size_t n_self = self_urls ? json_len(self_urls) : 0;
        size_t n_other = other_urls ? json_len(other_urls) : 0;
        for (i = 0; i < n_self; i++) {
            json_t *nm = (self_names && i < json_len(self_names)) ? json_get(self_names, i) : NULL;
            json_append(names, nm && nm->type == JSON_STRING ? json_string(nm->str_val) : json_string(""));
        }
        for (i = 0; i < n_other; i++) {
            json_t *nm = (other_names && i < json_len(other_names)) ? json_get(other_names, i) : NULL;
            json_append(names, nm && nm->type == JSON_STRING ? json_string(nm->str_val) : json_string(""));
        }
    } else {
        for (i = 0; self_names && i < json_len(self_names); i++)
            json_append(names, json_copy(json_get(self_names, i)));
        for (i = 0; other_names && i < json_len(other_names); i++)
            json_append(names, json_copy(json_get(other_names, i)));
    }
    json_set(out, "media_urls", urls);
    json_set(out, "media_types", types);
    json_set(out, "media_filenames", names);
    return out;
}

json_t *w19_event_eea6c7_accept_interrupt(json_t *events) {
    json_t *out = json_array();
    size_t n = events ? json_len(events) : 0;
    size_t idx = 0, guard = 0, i;
    char *marked = (char *)calloc(n + 1, 1);
    if (!marked) return out;
    while (idx < n && guard++ <= n + 2) {
        json_t *ev = json_get(events, idx);
        json_t *next;
        if (!ev || ev->type != JSON_OBJECT) break;
        if (marked[idx]) break;
        marked[idx] = 1;
        next = json_obj_get(ev, "rewritten_from");
        if (!next || next->type != JSON_NUMBER) break;
        idx = (size_t)next->num_val;
    }
    for (i = 0; i < n; i++) json_append(out, json_bool(marked[i] ? 1 : 0));
    free(marked);
    return out;
}
