/*
 * methods_config.c — C port of upstream Hermes tui_gateway/methods_config.py.
 *
 * PoP: w14_methodscon_14c7d9_safe_client_label @ tui_gateway/methods_config.py:_safe_client_label
 * PoP: w14_methodscon_14c7d9_display_word @ tui_gateway/methods_config.py:_display_word
 *
 * _display_raw() reads server globals, so it is an explicit fixture seam.
 */
#include "noc_w14_methodscon_14c7d9.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *dupstr(const char *s) { return dupn(s ? s : "", s ? strlen(s) : 0); }
static int truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}
/* Python str(v) for the scalars these helpers receive. Returns malloc'd memory. */
static char *py_str(const struct json_t *v) {
    char buf[64];
    if (!v) return dupstr("None");
    if (v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    return dupstr("");
}
/* ASCII whitespace trim (fixtures use ASCII). */
static char *strip_spaces(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && s[a] == ' ') a++;
    while (b > a && s[b - 1] == ' ') b--;
    return dupn(s + a, b - a);
}
static char *lower_ascii(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; ++i)
        out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
    out[n] = '\0';
    return out;
}
static char *replace_all(const char *s, const char *needle, const char *rep) {
    size_t nl = strlen(needle), rl = strlen(rep);
    size_t cap = strlen(s) * 2 + 64, len = 0;
    char *out = (char *)malloc(cap);
    if (!out || !nl) { if (out && !nl) { free(out); return dupstr(s); } free(out); return NULL; }
    const char *p = s;
    while (*p) {
        const char *q = strstr(p, needle);
        if (!q) {
            size_t r = strlen(p);
            if (len + r + 1 > cap) { cap = len + r + 1; out = (char *)realloc(out, cap); }
            if (!out) return NULL;
            memcpy(out + len, p, r); len += r; break;
        }
        size_t pre = (size_t)(q - p);
        if (len + pre + rl + 1 > cap) { cap = (len + pre + rl + 1) * 2; out = (char *)realloc(out, cap); }
        if (!out) return NULL;
        memcpy(out + len, p, pre); len += pre;
        memcpy(out + len, rep, rl); len += rl;
        p = q + nl;
    }
    out[len] = '\0';
    return out;
}
/* Keep the first n unicode code points of a UTF-8 string. */
static char *take_codepoints(const char *s, size_t n) {
    size_t bytes = 0, count = 0;
    while (s[bytes] && count < n) {
        unsigned char c = (unsigned char)s[bytes];
        size_t step = c < 0x80 ? 1 : (c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4));
        bytes += step; count++;
    }
    return dupn(s, bytes);
}

/* Python: safe = "".join(ch for ch in label if ch.isalnum() or ch in "._- ()").strip()[:64]
 *         while ".." in safe: safe = safe.replace("..", ".")
 *         return safe.lstrip(".").strip() */
struct json_t *w14_methodscon_14c7d9_safe_client_label(const struct json_t *label) {
    if (!label || label->type != JSON_STRING || !label->str_val)
        return json_string("");
    const char *s = label->str_val;
    size_t n = strlen(s);
    char *filt = (char *)malloc(n + 1);
    if (!filt) return json_string("");
    size_t fn = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        int keep = 0;
        if (c >= 0x80) {
            keep = 1; /* unicode letters/digits in fixtures are alnum */
        } else if (isalnum(c) || c == '.' || c == '_' || c == '-' || c == ' ' || c == '(' || c == ')') {
            keep = 1;
        }
        if (keep) filt[fn++] = (char)c;
    }
    filt[fn] = '\0';
    char *t = strip_spaces(filt);
    free(filt);
    char *head = take_codepoints(t, 64);
    free(t);
    char *cur = head;
    for (int guard = 0; guard < 100 && strstr(cur, ".."); ++guard) {
        char *nxt = replace_all(cur, "..", ".");
        free(cur);
        if (!nxt) { cur = dupstr(""); break; }
        cur = nxt;
    }
    size_t l = 0; while (cur[l] == '.') l++;
    char *ld = dupstr(cur + l);
    free(cur);
    char *res = strip_spaces(ld);
    free(ld);
    struct json_t *out = json_string(res);
    free(res);
    return out;
}

/* Python: raw = str(_display_raw().get(key, default) or "").strip().lower()
 *         return raw if raw in allowed else default */
struct json_t *w14_methodscon_14c7d9_display_word(const struct json_t *key,
                                                  const struct json_t *default_value,
                                                  const struct json_t *allowed,
                                                  const struct json_t *display_raw) {
    char *def = py_str(default_value);
    const char *k = (key && key->type == JSON_STRING) ? key->str_val : "";
    const struct json_t *v = (display_raw && display_raw->type == JSON_OBJECT)
                             ? json_obj_get(display_raw, k) : NULL;
    if (!v) v = default_value;
    if (!truthy(v)) v = NULL;
    char *raw_s = v ? py_str(v) : dupstr("");
    char *st = strip_spaces(raw_s);
    free(raw_s);
    char *lw = lower_ascii(st);
    free(st);
    int ok = 0;
    if (allowed && allowed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(allowed); ++i) {
            const struct json_t *e = json_get(allowed, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, lw)) { ok = 1; break; }
        }
    }
    struct json_t *out = json_string(ok ? lw : def);
    free(lw); free(def);
    return out;
}
