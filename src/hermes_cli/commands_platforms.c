/*
 * commands_platforms.c - C home for upstream Hermes hermes_cli/commands_platforms.py.
 *
 * PoP: w17_commandspl_3e5241_requires_argument @ hermes_cli/commands_platforms.py:_requires_argument
 * PoP: w17_commandspl_3e5241_sanitize_telegram_name @ hermes_cli/commands_platforms.py:_sanitize_telegram_name
 * PoP: w17_commandspl_3e5241_normalize_telegram_desc @ hermes_cli/commands_platforms.py:_normalize_telegram_desc
 * PoP: w17_commandspl_3e5241_truncate_desc @ hermes_cli/commands_platforms.py:_truncate_desc
 * PoP: w17_commandspl_3e5241_clamp_command_names @ hermes_cli/commands_platforms.py:_clamp_command_names
 * PoP: w17_commandspl_3e5241_sanitized_rank @ hermes_cli/commands_platforms.py:_sanitized_rank
 * PoP: w17_commandspl_3e5241_sanitize_slack_name @ hermes_cli/commands_platforms.py:_sanitize_slack_name
 *
 * Name/description normalization and the 32-char clamp are pure; the gateway
 * registry lookups are not needed by these helpers.
 */
#include "noc_w17_commandspl_3e5241.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_NAME_LIMIT 32

static char *dupstr(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* Advance one UTF-8 code point from byte offset i (start only at lead bytes). */
static size_t u8_next(const char *s, size_t i, size_t n) {
    unsigned char c = (unsigned char)s[i];
    size_t step = 1;
    if (c >= 0xF0) step = 4;
    else if (c >= 0xE0) step = 3;
    else if (c >= 0xC0) step = 2;
    if (i + step > n) step = 1;
    return i + step;
}
static size_t u8_count(const char *s) {
    size_t n = strlen(s), i = 0, c = 0;
    while (i < n) { i = u8_next(s, i, n); c++; }
    return c;
}
static char *u8_prefix(const char *s, size_t chars) {
    size_t n = strlen(s), i = 0, c = 0, end;
    char *d;
    while (i < n && c < chars) { i = u8_next(s, i, n); c++; }
    end = i;
    d = (char *)malloc(end + 1);
    if (d) { memcpy(d, s, end); d[end] = '\0'; }
    return d;
}

static int is_tg_valid(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}
/* ASCII alnum; any non-ASCII lead byte is treated as a Unicode letter/digit. */
static int is_alnum_like(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c >= 0x80;
}

json_t *w17_commandspl_3e5241_requires_argument(const char *args_hint) {
    const char *s = args_hint ? args_hint : "";
    while (*s && isspace((unsigned char)*s)) s++;
    return json_bool(*s == '<' ? true : false);
}

json_t *w17_commandspl_3e5241_sanitize_telegram_name(const char *raw) {
    const char *s = raw ? raw : "";
    size_t n = strlen(s), i;
    char *low, *name, *out;
    size_t j = 0, k;
    int lost = 0;
    low = (char *)malloc(n + 1);
    if (!low) return json_string("");
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        low[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
        if (low[i] == '-') low[i] = '_';
    }
    low[n] = '\0';
    name = (char *)malloc(n + 1);
    if (!name) { free(low); return json_string(""); }
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)low[i];
        if (is_tg_valid(c)) name[j++] = low[i];
        else if (is_alnum_like(c)) lost = 1;
    }
    name[j] = '\0';
    free(low);
    if (lost) { free(name); return json_string(""); }
    /* _TG_MULTI_UNDERSCORE.sub("_", name).strip("_") */
    out = (char *)malloc(j + 2);
    if (!out) { free(name); return json_string(""); }
    k = 0;
    for (i = 0; i < j; ++i) {
        if (name[i] == '_' && k > 0 && out[k - 1] == '_') continue;
        out[k++] = name[i];
    }
    while (k > 0 && out[k - 1] == '_') k--;
    i = 0;
    while (i < k && out[i] == '_') i++;
    memmove(out, out + i, k - i);
    out[k - i] = '\0';
    free(name);
    return json_string(out ? out : "");
}

/* Fold the five Unicode dashes BotFather rejects into ASCII '-'. */
json_t *w17_commandspl_3e5241_normalize_telegram_desc(const char *desc) {
    const char *s = desc ? desc : "";
    size_t n = strlen(s), i, p = 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return json_string("");
    for (i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        if (i + 2 < n && c == 0xE2 &&
            (((unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] >= 0x92 &&
              (unsigned char)s[i + 2] <= 0x95) ||
             ((unsigned char)s[i + 1] == 0x88 && (unsigned char)s[i + 2] == 0x92))) {
            out[p++] = '-';
            i += 3;
        } else {
            out[p++] = (char)c;
            i++;
        }
    }
    out[p] = '\0';
    return json_string(out);
}

/* desc if len <= limit else desc[:limit-3] + "..." (character-aware). */
json_t *w17_commandspl_3e5241_truncate_desc(const char *desc, int limit) {
    const char *s = desc ? desc : "";
    size_t clen = u8_count(s);
    char *prefix, *out;
    size_t cut, plen;
    if ((size_t)limit >= clen) return json_string(s);
    if (limit - 3 >= 0) cut = (size_t)(limit - 3);
    else cut = clen > (size_t)(3 - limit) ? clen - (size_t)(3 - limit) : 0;
    if (cut > clen) cut = clen;
    prefix = u8_prefix(s, cut);
    plen = strlen(prefix ? prefix : "");
    out = (char *)malloc(plen + 4);
    if (!out) { free(prefix); return json_string(""); }
    memcpy(out, prefix ? prefix : "", plen);
    memcpy(out + plen, "...", 4);
    free(prefix);
    return json_string(out);
}

static int in_reserved(const json_t *arr, const char *name) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; ++i) {
        json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}
static int is_used(const json_t *out, const json_t *reserved, const char *name) {
    size_t j;
    if (in_reserved(reserved, name)) return 1;
    for (j = 0; j < json_len(out); ++j) {
        json_t *prev = json_get(out, j);
        json_t *pv = prev && prev->type == JSON_ARRAY ? json_get(prev, 0) : NULL;
        if (pv && pv->type == JSON_STRING && !strcmp(pv->str_val, name)) return 1;
    }
    return 0;
}

json_t *w17_commandspl_3e5241_clamp_command_names(const json_t *entries, const json_t *reserved) {
    json_t *out = json_array();
    size_t i;
    if (!entries || entries->type != JSON_ARRAY) return out;
    for (i = 0; i < entries->c.count; ++i) {
        json_t *e = entries->c.items[i];
        json_t *namev, *newrow;
        char *name, *candidate = NULL, *prefix = NULL;
        size_t j;
        int found = 0;
        if (!e || e->type != JSON_ARRAY || e->c.count < 2) continue;
        namev = e->c.items[0];
        name = dupstr(namev && namev->type == JSON_STRING ? namev->str_val : "");
        if (u8_count(name) > CMD_NAME_LIMIT) {
            size_t k;
            candidate = u8_prefix(name, CMD_NAME_LIMIT);
            if (is_used(out, reserved, candidate)) {
                prefix = u8_prefix(name, CMD_NAME_LIMIT - 1);
                free(candidate);
                candidate = NULL;
                for (k = 0; k < 10; ++k) {
                    char *tryc = (char *)malloc(strlen(prefix) + 2);
                    if (!tryc) break;
                    snprintf(tryc, strlen(prefix) + 2, "%s%c", prefix, (char)('0' + k));
                    if (!is_used(out, reserved, tryc)) {
                        candidate = tryc;
                        found = 1;
                        break;
                    }
                    free(tryc);
                }
                if (!found) { free(prefix); free(name); continue; }
            }
            free(name);
            name = candidate;
            candidate = NULL;
        }
        free(prefix);
        free(candidate);
        if (is_used(out, reserved, name)) { free(name); continue; }
        newrow = json_array();
        json_append(newrow, json_string(name));
        for (j = 1; j < e->c.count; ++j)
            json_append(newrow, json_copy(e->c.items[j]));
        json_append(out, newrow);
        free(name);
    }
    return out;
}

static int in_object(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

json_t *w17_commandspl_3e5241_sanitized_rank(const json_t *raw_names) {
    json_t *out = json_object();
    size_t i;
    int rank = 0;
    if (!raw_names || raw_names->type != JSON_ARRAY) return out;
    for (i = 0; i < raw_names->c.count; ++i) {
        json_t *v = raw_names->c.items[i];
        const char *raw = v && v->type == JSON_STRING ? v->str_val : "";
        json_t *nv = w17_commandspl_3e5241_sanitize_telegram_name(raw);
        const char *name = nv && nv->type == JSON_STRING ? nv->str_val : "";
        if (*name && !in_object(out, name)) {
            json_set(out, name, json_number((double)rank++));
        }
        json_free(nv);
    }
    return out;
}

json_t *w17_commandspl_3e5241_sanitize_slack_name(const char *raw) {
    const char *s = raw ? raw : "";
    size_t n = strlen(s), i, p = 0, k;
    char *low, *out;
    low = (char *)malloc(n + 1);
    out = (char *)malloc(n + 1);
    if (!low || !out) { free(low); free(out); return json_string(""); }
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            low[p++] = (char)c;
    }
    low[p] = '\0';
    i = 0;
    k = p;
    while (i < k && (low[i] == '-' || low[i] == '_')) i++;
    while (k > i && (low[k - 1] == '-' || low[k - 1] == '_')) k--;
    for (p = 0; p < k - i && p < CMD_NAME_LIMIT; ++p) out[p] = low[i + p];
    out[p] = '\0';
    free(low);
    return json_string(out);
}
