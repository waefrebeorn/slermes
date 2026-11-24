/*
 * whatsapp_common.c - real C11 port of upstream Hermes gateway/platforms/whatsapp_common.py
 * transport-agnostic behavior: allow-list coercion, id normalization, broadcast/DM/group
 * mention detection, outbound sanitizing and the WhatsApp markdown conversion.
 * Collaborators (config extras, env carriers, lid mapping) are injected as values so the
 * module runs without a live adapter.
 * PoP: w19_whatsappco_87e380_header_to_bold @ gateway/platforms/whatsapp_common.py:_header_to_bold
 * PoP: w19_whatsappco_87e380_sanitize_outbound_text @ gateway/platforms/whatsapp_common.py:_sanitize_outbound_text
 * PoP: w19_whatsappco_87e380_effective_reply_prefix @ gateway/platforms/whatsapp_common.py:_effective_reply_prefix
 * PoP: w19_whatsappco_87e380_outgoing_chunk_limit @ gateway/platforms/whatsapp_common.py:_outgoing_chunk_limit
 * PoP: w19_whatsappco_87e380_normalize_whatsapp_id @ gateway/platforms/whatsapp_common.py:_normalize_whatsapp_id
 * PoP: w19_whatsappco_87e380_is_broadcast_chat @ gateway/platforms/whatsapp_common.py:_is_broadcast_chat
 * PoP: w19_whatsappco_87e380_coerce_allow_list @ gateway/platforms/whatsapp_common.py:_coerce_allow_list
 * PoP: w19_whatsappco_87e380_select_allowlist @ gateway/platforms/whatsapp_common.py:_select_allowlist
 * PoP: w19_whatsappco_87e380_bot_ids_from_message @ gateway/platforms/whatsapp_common.py:_bot_ids_from_message
 * PoP: w19_whatsappco_87e380_message_is_reply_to_bot @ gateway/platforms/whatsapp_common.py:_message_is_reply_to_bot
 * PoP: w19_whatsappco_87e380_message_mentions_bot @ gateway/platforms/whatsapp_common.py:_message_mentions_bot
 * PoP: w19_whatsappco_87e380_clean_bot_mention_text @ gateway/platforms/whatsapp_common.py:_clean_bot_mention_text
 * PoP: w19_whatsappco_87e380_format_message @ gateway/platforms/whatsapp_common.py:format_message
 */
#include "noc_w19_whatsappco_87e380.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Practical UX limit, not the ~65K protocol max. */
static const int MAX_MESSAGE_LENGTH = 4096;
#define BOX "\xE2\x94\x80"
static const char DEFAULT_REPLY_PREFIX[] =
    "\xE2\x98\xA4 *Hermes Agent*\n" BOX BOX BOX BOX BOX BOX BOX BOX BOX BOX BOX BOX "\n";

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python str() of a JSON scalar. */
static void scalar_str(const json_t *v, char *buf, size_t cap) {
    buf[0] = '\0';
    if (!v) return;
    if (v->type == JSON_STRING) snprintf(buf, cap, "%s", v->str_val ? v->str_val : "");
    else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, cap, "%lld", (long long)d);
        else snprintf(buf, cap, "%g", d);
    } else if (v->type == JSON_BOOL) snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
    else if (v->type == JSON_NULL) snprintf(buf, cap, "None");
}

/* == byte buffers (placeholders carry NULs, so every rule is length-delimited) == */
typedef struct { char *p; size_t n; } dbuf;
typedef struct { char *s; size_t n; } dseg;
typedef struct { dseg *v; size_t n, cap; } dvec;

static void dbuf_init(dbuf *b) {
    b->p = (char *)malloc(1);
    b->n = 0;
    if (b->p) b->p[0] = '\0';
}

static void dbuf_push(dbuf *b, const char *s, size_t n) {
    if (!b->p || !s || n == 0) return;
    char *q = (char *)realloc(b->p, b->n + n + 1);
    if (!q) return;
    b->p = q;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void dbuf_putc(dbuf *b, char c) { dbuf_push(b, &c, 1); }

static char *dbuf_take(dbuf *b) {
    char *p = b->p;
    b->p = NULL;
    b->n = 0;
    return p;
}

static void dbuf_free(dbuf *b) {
    if (b->p) free(b->p);
    b->p = NULL;
    b->n = 0;
}

static void dvec_init(dvec *v) { v->v = NULL; v->n = 0; v->cap = 0; }

static void dvec_push(dvec *v, const char *s, size_t n) {
    if (v->n == v->cap) {
        size_t cap = v->cap ? v->cap * 2 : 8;
        dseg *q = (dseg *)realloc(v->v, cap * sizeof(dseg));
        if (!q) return;
        v->v = q;
        v->cap = cap;
    }
    char *copy = (char *)malloc(n + 1);
    if (!copy) return;
    memcpy(copy, s, n);
    copy[n] = '\0';
    v->v[v->n].s = copy;
    v->v[v->n].n = n;
    v->n++;
}

static void dvec_free(dvec *v) {
    for (size_t i = 0; i < v->n; i++) free(v->v[i].s);
    free(v->v);
    v->v = NULL;
    v->n = 0;
    v->cap = 0;
}

/* "\0<TAG><idx>\0" as raw bytes (a NUL cannot be part of a C string). */
static size_t make_placeholder(char *buf, const char *tag, size_t idx) {
    size_t k = 0;
    buf[k++] = '\0';
    size_t tl = strlen(tag);
    memcpy(buf + k, tag, tl);
    k += tl;
    char num[24];
    int nl = snprintf(num, sizeof(num), "%d", (int)idx);
    memcpy(buf + k, num, (size_t)nl);
    k += (size_t)nl;
    buf[k++] = '\0';
    return k;
}

/* == sanitize =============================================================== */
static const struct { unsigned char seq[3]; int len; } INVISIBLE[] = {
    {{0xE2, 0x80, 0x8B}, 3},  /* U+200B zero width space */
    {{0xE2, 0x81, 0xA0}, 3},  /* U+2060 word joiner */
    {{0xE2, 0x81, 0xA3}, 3},  /* U+2063 invisible separator */
    {{0xEF, 0xBB, 0xBF}, 3},  /* U+FEFF zero width no-break space */
};

static int odd_space_at(const unsigned char *s, size_t n, size_t i, int *len) {
    if (i + 1 < n && s[i] == 0xC2 && s[i + 1] == 0xA0) { *len = 2; return 1; }   /* U+00A0 */
    if (i + 2 < n && s[i] == 0xE1 && s[i + 1] == 0x9A && s[i + 2] == 0x80) { *len = 3; return 1; } /* U+1680 */
    if (i + 2 < n && s[i] == 0xE1 && s[i + 1] == 0xA0 && s[i + 2] == 0x8E) { *len = 3; return 1; } /* U+180E */
    if (i + 2 < n && s[i] == 0xE2 && s[i + 1] == 0x80
            && s[i + 2] >= 0x80 && s[i + 2] <= 0x8A) { *len = 3; return 1; }   /* U+2000..U+200A */
    if (i + 2 < n && s[i] == 0xE2 && s[i + 1] == 0x80 && s[i + 2] == 0xAF) { *len = 3; return 1; } /* U+202F */
    if (i + 2 < n && s[i] == 0xE2 && s[i + 1] == 0x81 && s[i + 2] == 0x9F) { *len = 3; return 1; } /* U+205F */
    if (i + 2 < n && s[i] == 0xE3 && s[i + 1] == 0x80 && s[i + 2] == 0x80) { *len = 3; return 1; } /* U+3000 */
    return 0;
}

static char *sanitize_c(const char *content) {
    if (!content || !content[0]) return dupstr(content ? content : "");
    const unsigned char *s = (const unsigned char *)content;
    size_t n = strlen(content);
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        int skip = 0;
        for (size_t k = 0; k < sizeof(INVISIBLE) / sizeof(INVISIBLE[0]); k++) {
            if (i + 3 <= n && memcmp(s + i, INVISIBLE[k].seq, 3) == 0) { i += 3; skip = 1; break; }
        }
        if (skip) continue;
        int len = 0;
        if (odd_space_at(s, n, i, &len)) { dbuf_putc(&out, ' '); i += (size_t)len; continue; }
        dbuf_putc(&out, (char)s[i]);
        i++;
    }
    return dbuf_take(&out);
}

json_t *w19_whatsappco_87e380_sanitize_outbound_text(const char *content) {
    char *s = sanitize_c(content);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* == header -> bold ========================================================== */
static char *strip_copy(const char *s, size_t n) {
    size_t a = 0, b = n;
    while (a < b && is_ws(s[a])) a++;
    while (b > a && is_ws(s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

json_t *w19_whatsappco_87e380_header_to_bold(const char *inner) {
    char *s = strip_copy(inner ? inner : "", inner ? strlen(inner) : 0);
    while (s && strlen(s) > 1 && s[0] == '*' && s[strlen(s) - 1] == '*') {
        size_t len = strlen(s);
        char *next = strip_copy(s + 1, len - 2);
        free(s);
        s = next;
    }
    dbuf out;
    dbuf_init(&out);
    dbuf_putc(&out, '*');
    dbuf_push(&out, s ? s : "", s ? strlen(s) : 0);
    dbuf_putc(&out, '*');
    free(s);
    char *body = dbuf_take(&out);
    json_t *res = json_string(body ? body : "");
    free(body);
    return res;
}

/* == reply prefix =========================================================== */
static char *unescape_nl(const char *s) {
    dbuf out;
    dbuf_init(&out);
    for (size_t i = 0; s && s[i]; i++) {
        if (s[i] == '\\' && s[i + 1] == 'n') { dbuf_putc(&out, '\n'); i++; }
        else dbuf_putc(&out, s[i]);
    }
    return dbuf_take(&out);
}

json_t *w19_whatsappco_87e380_effective_reply_prefix(const char *mode,
                                                    const char *reply_prefix,
                                                    const char *env_prefix) {
    const char *m = (mode && mode[0]) ? mode : "self-chat";   /* (secret or "self-chat") */
    const char *out;
    if (strcmp(m, "self-chat") != 0) out = "";
    else if (reply_prefix != NULL) {
        char *v = unescape_nl(reply_prefix);
        json_t *res = json_string(v ? v : "");
        free(v);
        return res;
    } else if (env_prefix != NULL) {
        char *v = unescape_nl(env_prefix);
        json_t *res = json_string(v ? v : "");
        free(v);
        return res;
    } else out = DEFAULT_REPLY_PREFIX;
    return json_string(out);
}

json_t *w19_whatsappco_87e380_outgoing_chunk_limit(int prefix_len) {
    int limit = MAX_MESSAGE_LENGTH - prefix_len;
    return json_number((double)(limit > 1024 ? limit : 1024));
}

/* == ids ==================================================================== */
static char *normalize_c(const char *value) {
    if (!value || !value[0]) return dupstr("");
    char *stripped = strip_copy(value, strlen(value));
    dbuf out;
    dbuf_init(&out);
    for (size_t i = 0; stripped[i]; i++) {
        if (stripped[i] == ':') {
            size_t j = i + 1;
            while (stripped[j] >= '0' && stripped[j] <= '9') j++;
            if (j > i + 1 && stripped[j] == '@') { i = j - 1; continue; }  /* drop the device */
        }
        dbuf_putc(&out, stripped[i]);
    }
    free(stripped);
    return dbuf_take(&out);
}

json_t *w19_whatsappco_87e380_normalize_whatsapp_id(const char *value) {
    char *s = normalize_c(value);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

static char *lower_copy(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return dupstr("");
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

static int ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && memcmp(s + a - b, suffix, b) == 0;
}

int w19_whatsappco_87e380_is_broadcast_chat(const char *chat_id) {
    char *stripped = strip_copy(chat_id ? chat_id : "", chat_id ? strlen(chat_id) : 0);
    char *cid = lower_copy(stripped, strlen(stripped));
    free(stripped);
    int hit = !strcmp(cid, "status@broadcast") || ends_with(cid, "@broadcast")
           || ends_with(cid, "@newsletter");
    free(cid);
    return hit;
}

/* == allow lists ============================================================ */
static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void set_add(json_t *arr, const char *value) {
    if (!value || !value[0]) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, value)) return;
    }
    json_append(arr, json_string(value));
}

static json_t *sorted_array(json_t *set_arr) {
    size_t n = json_len(set_arr);
    const char **items = n ? (const char **)malloc(n * sizeof(char *)) : NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(set_arr, i);
        items[i] = (e && e->type == JSON_STRING) ? e->str_val : "";
    }
    if (n) qsort(items, n, sizeof(char *), cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_string(items[i]));
    free(items);
    return out;
}

json_t *w19_whatsappco_87e380_coerce_allow_list(json_t *raw) {
    json_t *set_arr = json_array();
    if (!raw || raw->type == JSON_NULL) return sorted_array(set_arr);   /* set() */

    json_t *parsed = NULL;
    json_t *vals = raw;
    if (raw->type == JSON_STRING && raw->str_val) {
        const char *p = raw->str_val;
        while (is_ws(*p)) p++;
        if (*p == '[') {
            parsed = json_parse(raw->str_val, NULL);
            if (parsed && parsed->type == JSON_ARRAY) vals = parsed;
            else if (parsed) { json_free(parsed); parsed = NULL; }
        }
    }

    if (vals->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(vals); i++) {
            json_t *e = json_get(vals, i);
            char buf[256];
            if (e && e->type == JSON_STRING) {
                char *st = strip_copy(e->str_val ? e->str_val : "", e->str_val ? strlen(e->str_val) : 0);
                set_add(set_arr, st);
                free(st);
            } else {
                scalar_str(e, buf, sizeof(buf));
                char *st = strip_copy(buf, strlen(buf));
                set_add(set_arr, st);
                free(st);
            }
        }
    } else {
        /* str(raw).split(",") */
        char buf[4096];
        scalar_str(raw, buf, sizeof(buf));
        const char *start = buf;
        for (const char *p = buf;; p++) {
            if (*p == ',' || *p == '\0') {
                char *piece = strip_copy(start, (size_t)(p - start));
                set_add(set_arr, piece);
                free(piece);
                if (!*p) break;
                start = p + 1;
            }
        }
    }
    if (parsed) json_free(parsed);
    return sorted_array(set_arr);
}

json_t *w19_whatsappco_87e380_select_allowlist(json_t *extra, json_t *config_keys,
                                               json_t *env_keys, json_t *env) {
    json_t *out = json_object();
    if (config_keys && config_keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(config_keys); i++) {
            json_t *k = json_get(config_keys, i);
            if (!k || k->type != JSON_STRING) continue;
            if (extra && extra->type == JSON_OBJECT) {
                json_t *v = json_obj_get(extra, k->str_val);
                if (v) {   /* key presence, even when explicitly empty */
                    json_set(out, "source", json_string("config"));
                    json_set(out, "raw", json_copy(v));
                    return out;
                }
            }
        }
    }
    if (env_keys && env_keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(env_keys); i++) {
            json_t *k = json_get(env_keys, i);
            if (!k || k->type != JSON_STRING) continue;
            json_t *v = (env && env->type == JSON_OBJECT) ? json_obj_get(env, k->str_val) : NULL;
            if (py_truthy(v)) {
                json_set(out, "source", json_string(k->str_val));
                json_set(out, "raw", json_copy(v));
                return out;
            }
        }
    }
    json_set(out, "source", json_null());
    json_set(out, "raw", json_null());
    return out;
}

/* == bot ids / mentions ===================================================== */
static json_t *bot_ids_sorted(json_t *data) {
    json_t *set_arr = json_array();
    json_t *ids = data ? json_obj_get(data, "botIds") : NULL;
    if (ids && ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ids); i++) {
            json_t *c = json_get(ids, i);
            char buf[256];
            scalar_str(c, buf, sizeof(buf));
            char *nid = normalize_c(buf);
            set_add(set_arr, nid);
            free(nid);
        }
    }
    json_t *sorted = sorted_array(set_arr);
    json_free(set_arr);
    return sorted;
}

json_t *w19_whatsappco_87e380_bot_ids_from_message(json_t *data) {
    return bot_ids_sorted(data);
}

int w19_whatsappco_87e380_message_is_reply_to_bot(json_t *data) {
    json_t *quoted = data ? json_obj_get(data, "quotedParticipant") : NULL;
    char buf[256];
    scalar_str(quoted, buf, sizeof(buf));
    char *normalized = normalize_c(buf);
    int hit = 0;
    if (normalized[0]) {
        json_t *ids = bot_ids_sorted(data);
        for (size_t i = 0; i < json_len(ids); i++) {
            json_t *e = json_get(ids, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, normalized)) { hit = 1; break; }
        }
        json_free(ids);
    }
    free(normalized);
    return hit;
}

int w19_whatsappco_87e380_message_mentions_bot(json_t *data) {
    json_t *ids = bot_ids_sorted(data);
    if (json_len(ids) == 0) { json_free(ids); return 0; }

    int hit = 0;
    json_t *mentioned = data ? json_obj_get(data, "mentionedIds") : NULL;
    if (mentioned && mentioned->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mentioned) && !hit; i++) {
            char buf[256];
            scalar_str(json_get(mentioned, i), buf, sizeof(buf));
            char *nid = normalize_c(buf);
            for (size_t k = 0; k < json_len(ids); k++) {
                json_t *e = json_get(ids, k);
                if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, nid)) { hit = 1; break; }
            }
            free(nid);
        }
    }
    if (!hit) {
        json_t *body = data ? json_obj_get(data, "body") : NULL;
        char bodybuf[4096];
        scalar_str(body, bodybuf, sizeof(bodybuf));
        char *lower = lower_copy(bodybuf, strlen(bodybuf));
        for (size_t k = 0; k < json_len(ids) && !hit; k++) {
            json_t *e = json_get(ids, k);
            if (!e || e->type != JSON_STRING || !e->str_val) continue;
            char bare[256];
            size_t bi = 0;
            while (e->str_val[bi] && e->str_val[bi] != '@' && bi + 1 < sizeof(bare)) {
                bare[bi] = (char)tolower((unsigned char)e->str_val[bi]);
                bi++;
            }
            bare[bi] = '\0';
            if (!bare[0]) continue;
            char at[260];
            snprintf(at, sizeof(at), "@%s", bare);
            if (strstr(lower, at) || strstr(lower, bare)) hit = 1;
        }
        free(lower);
    }
    json_free(ids);
    return hit;
}

/* re.sub(rf"@{escape(bare)}\b[,:\-]*\s*", "", cleaned) */
static char *remove_bot_mentions(const char *text, const char *bare) {
    dbuf out;
    dbuf_init(&out);
    size_t bl = strlen(bare);
    size_t n = strlen(text);
    size_t i = 0;
    while (i < n) {
        if (text[i] == '@' && i + 1 + bl <= n && memcmp(text + i + 1, bare, bl) == 0) {
            size_t after = i + 1 + bl;
            char prev = bl ? bare[bl - 1] : '\0';
            int prev_word = isalnum((unsigned char)prev) || prev == '_';
            char next = after < n ? text[after] : '\0';
            int next_word = next && (isalnum((unsigned char)next) || next == '_');
            int boundary = prev_word ? !next_word : next_word;
            if (boundary) {
                size_t j = after;
                while (j < n && (text[j] == ',' || text[j] == ':' || text[j] == '-')) j++;
                while (j < n && is_ws(text[j])) j++;
                i = j;
                continue;
            }
        }
        dbuf_putc(&out, text[i]);
        i++;
    }
    return dbuf_take(&out);
}

json_t *w19_whatsappco_87e380_clean_bot_mention_text(const char *text, json_t *data) {
    if (!text || !text[0]) return json_string(text ? text : "");
    char *cleaned = dupstr(text);
    json_t *ids = bot_ids_sorted(data);
    for (size_t i = 0; i < json_len(ids); i++) {
        json_t *e = json_get(ids, i);
        if (!e || e->type != JSON_STRING || !e->str_val) continue;
        char bare[256];
        size_t bi = 0;
        while (e->str_val[bi] && e->str_val[bi] != '@' && bi + 1 < sizeof(bare)) {
            bare[bi] = e->str_val[bi];
            bi++;
        }
        bare[bi] = '\0';
        if (!bare[0]) continue;
        char *next = remove_bot_mentions(cleaned, bare);
        free(cleaned);
        cleaned = next;
    }
    json_free(ids);
    char *stripped = strip_copy(cleaned, strlen(cleaned));
    free(cleaned);
    json_t *out = stripped[0] ? json_string(stripped) : json_string(text);
    free(stripped);
    return out;
}

/* == markdown -> WhatsApp ==================================================== */
static dbuf stash_fenced(const char *s, size_t n, const char *tag, dvec *saved) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        if (i + 2 < n && s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') {
            size_t j = i + 3;
            long close = -1;
            for (; j + 2 < n; j++)
                if (s[j] == '`' && s[j + 1] == '`' && s[j + 2] == '`') { close = (long)j; break; }
            if (close >= 0) {
                char ph[40];
                size_t pn = make_placeholder(ph, tag, saved->n);
                dvec_push(saved, s + i, (size_t)close + 3 - i);
                dbuf_push(&out, ph, pn);
                i = (size_t)close + 3;
                continue;
            }
        }
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

static dbuf stash_code(const char *s, size_t n, const char *tag, dvec *saved) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        if (s[i] == '`') {
            size_t j = i + 1;
            while (j < n && s[j] != '`' && s[j] != '\n') j++;
            if (j < n && s[j] == '`' && j > i + 1) {
                char ph[40];
                size_t pn = make_placeholder(ph, tag, saved->n);
                dvec_push(saved, s + i, j + 1 - i);
                dbuf_push(&out, ph, pn);
                i = j + 1;
                continue;
            }
        }
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

/* (?<!\*)\*(?!\s|\*)([^*\n]*?\S[^*\n]*?)\*(?!\*) -> _\1_
 * The group is a star-free run, one \S char (which may itself be a star left over from a
 * "**" the lookahead rejected), then another star-free run: so the content may hold at
 * most one star and no newline, and the closing star is the first candidate whose
 * following star does not repeat. */
static dbuf rule_italic(const char *s, size_t n) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        int matched = 0;
        if (s[i] == '*' && (i == 0 || s[i - 1] != '*')
                && i + 1 < n && !is_ws(s[i + 1]) && s[i + 1] != '*') {
            for (size_t j = i + 2; j < n; j++) {
                if (s[j] == '\n') break;
                if (s[j] != '*') continue;
                int stars = 0, has_nonspace = 0;
                for (size_t k = i + 1; k < j; k++) {
                    if (s[k] == '*') stars++;
                    if (!is_ws(s[k])) has_nonspace = 1;
                }
                if (stars > 1) break;
                if (has_nonspace && (j + 1 >= n || s[j + 1] != '*')) {
                    dbuf_putc(&out, '_');
                    dbuf_push(&out, s + i + 1, j - i - 1);
                    dbuf_putc(&out, '_');
                    i = j + 1;
                    matched = 1;
                    break;
                }
            }
        }
        if (matched) continue;
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

/* \*\*(.+?)\*\* -> *\1*, __(.+?)__ -> *\1*, ~~(.+?)~~ -> ~\1~ */
static dbuf rule_paired(const char *s, size_t n, char ch, char out_ch) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        if (s[i] == ch && i + 1 < n && s[i + 1] == ch) {
            long close = -1;
            for (size_t j = i + 3; j + 1 < n; j++) {
                if (s[j] == '\n') break;
                if (s[j] == ch && s[j + 1] == ch) { close = (long)j; break; }
            }
            if (close >= 0) {
                dbuf_putc(&out, out_ch);
                dbuf_push(&out, s + i + 2, (size_t)close - (i + 2));
                dbuf_putc(&out, out_ch);
                i = (size_t)close + 2;
                continue;
            }
        }
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

/* ^#{1,6}\s+(.+)$ (MULTILINE) with the header_to_bold callback */
static dbuf rule_headers(const char *s, size_t n) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        if ((i == 0 || s[i - 1] == '\n') && s[i] == '#') {
            size_t k = i;
            int h = 0;
            while (k < n && s[k] == '#' && h < 6) { k++; h++; }
            if (h >= 1 && k < n && is_ws(s[k])) {
                size_t j = k;
                while (j < n && is_ws(s[j])) j++;
                size_t e = j;
                while (e < n && s[e] != '\n') e++;
                if (e > j) {
                    char *inner = strip_copy(s + j, e - j);
                    json_t *bold = w19_whatsappco_87e380_header_to_bold(inner);
                    free(inner);
                    if (bold && bold->type == JSON_STRING && bold->str_val)
                        dbuf_push(&out, bold->str_val, strlen(bold->str_val));
                    json_free(bold);
                    i = e;
                    continue;
                }
            }
        }
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

/* \[([^\]]+)\]\(([^)]+)\) -> \1 (\2) */
static dbuf rule_links(const char *s, size_t n) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        if (s[i] == '[') {
            size_t p = i + 1;
            while (p < n && s[p] != ']') p++;
            if (p < n && p > i + 1 && p + 1 < n && s[p + 1] == '(') {
                size_t q = p + 2;
                while (q < n && s[q] != ')') q++;
                if (q < n && q > p + 2) {
                    dbuf_push(&out, s + i + 1, p - i - 1);
                    dbuf_push(&out, " (", 2);
                    dbuf_push(&out, s + p + 2, q - p - 2);
                    dbuf_putc(&out, ')');
                    i = q + 1;
                    continue;
                }
            }
        }
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

static dbuf restore_tag(const char *s, size_t n, const char *tag, dvec *saved) {
    dbuf out;
    dbuf_init(&out);
    size_t i = 0;
    while (i < n) {
        int replaced = 0;
        for (size_t idx = 0; idx < saved->n; idx++) {
            char ph[40];
            size_t pn = make_placeholder(ph, tag, idx);
            if (i + pn <= n && memcmp(s + i, ph, pn) == 0) {
                dbuf_push(&out, saved->v[idx].s, saved->v[idx].n);
                i += pn;
                replaced = 1;
                break;
            }
        }
        if (replaced) continue;
        dbuf_putc(&out, s[i]);
        i++;
    }
    return out;
}

json_t *w19_whatsappco_87e380_format_message(const char *content) {
    if (!content || !content[0]) return json_string(content ? content : "");

    char *san = sanitize_c(content);
    dvec fences, codes;
    dvec_init(&fences);
    dvec_init(&codes);

    dbuf b1 = stash_fenced(san, strlen(san), "FENCE", &fences);
    free(san);
    dbuf b2 = stash_code(b1.p ? b1.p : "", b1.n, "CODE", &codes);
    dbuf_free(&b1);

    dbuf b3 = rule_italic(b2.p ? b2.p : "", b2.n);
    dbuf_free(&b2);
    dbuf b4 = rule_paired(b3.p ? b3.p : "", b3.n, '*', '*');
    dbuf_free(&b3);
    dbuf b5 = rule_paired(b4.p ? b4.p : "", b4.n, '_', '*');
    dbuf_free(&b4);
    dbuf b6 = rule_paired(b5.p ? b5.p : "", b5.n, '~', '~');
    dbuf_free(&b5);
    dbuf b7 = rule_headers(b6.p ? b6.p : "", b6.n);
    dbuf_free(&b6);
    dbuf b8 = rule_links(b7.p ? b7.p : "", b7.n);
    dbuf_free(&b7);

    dbuf r1 = restore_tag(b8.p ? b8.p : "", b8.n, "FENCE", &fences);
    dbuf_free(&b8);
    dbuf r2 = restore_tag(r1.p ? r1.p : "", r1.n, "CODE", &codes);
    dbuf_free(&r1);

    json_t *out = json_string(r2.p ? r2.p : "");
    dbuf_free(&r2);
    dvec_free(&fences);
    dvec_free(&codes);
    return out;
}
