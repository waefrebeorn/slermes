/*
 * adapter.c - C11 port of the pure logic of upstream
 * plugins/platforms/telegram/adapter.py (415 KB, 369 defs), split across the
 * thirteen differential gates w27_adapter_a96ad6_p1 .. _p13.
 *
 * Ported here: the value-only decision core of the adapter -- chat-type
 * normalization, metadata/DM-topic route resolution, send/typing thread-id
 * derivation, exception classification (thread-not-found, bad-request, reply
 * anchor missing, conflict, auth, network, connect/pool timeout, rich
 * capability), rich-rendering shape predicates, per-chunk reply routing,
 * Update -> event envelope normalization, keyboard rows, media notes and size
 * policy, extra/env bool+list coercion, entity UTF-16 spans, bot-mention
 * extraction/matching, own/other-bot sender checks, batching delay policy and
 * DM-topic config lookup.
 *
 * Seamed (not ported, I/O or transport by nature): the Bot/PTB client, every
 * sendMessage/sendRichMessage/edit call, polling (getUpdates, generation
 * fences, heartbeats, reconnect ladders), webhook setup, file/media download
 * and upload, the SQLite session store, the rich-sent text index, dm-topic
 * persistence writes, i18n string tables (t()), the MarkdownV2/rich-markdown
 * converters (their own modules), sha256 menu fingerprints, the
 * _apply_yaml_config env bridges, and _build_message_event (SessionSource +
 * prompt composition).
 *
 * Every ported symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same function applied to the keys named
 * after its upstream parameters; a NULL json_t* is None.
 *
 * Representation notes (the three places JSON is not Python):
 *  - a Python set is returned as a sorted array (extra_str_set and the
 *    bot-mention extractor return sets upstream);
 *  - a JSON number stands for a Python int; a non-integral number is a float
 *    and is therefore rejected by the id-like / message-id parsers;
 *  - isinstance(exc, PTBClass) rungs need python-telegram-bot. Where the
 *    classifier is purely name/text based the port matches exactly; the
 *    TimedOut isinstance rung is a seam and only the "timed out" text rung is
 *    ported, which is what a build without the SDK can decide.
 */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w27_adapter_a96ad6_p1.h"
#include "noc_w27_adapter_a96ad6_p2.h"
#include "noc_w27_adapter_a96ad6_p3.h"
#include "noc_w27_adapter_a96ad6_p4.h"
#include "noc_w27_adapter_a96ad6_p5.h"
#include "noc_w27_adapter_a96ad6_p6.h"
#include "noc_w27_adapter_a96ad6_p7.h"
#include "noc_w27_adapter_a96ad6_p8.h"
#include "noc_w27_adapter_a96ad6_p9.h"
#include "noc_w27_adapter_a96ad6_p10.h"
#include "noc_w27_adapter_a96ad6_p11.h"
#include "noc_w27_adapter_a96ad6_p12.h"
#include "noc_w27_adapter_a96ad6_p13.h"

#define W27_GENERAL_TOPIC_THREAD_ID "1"

/* ------------------------------------------------------------------ */
/* Python value helpers                                               */
/* ------------------------------------------------------------------ */

/* None is an absent json_t* or an explicit null node. */
static int jd_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* Python truthiness of a JSON value. */
static int jd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* obj.get(key): NULL for a missing key or a non-object. */
static const json_t *jd_get(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, k);
}

static const char *jd_text(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* str value of a key, or NULL when absent/not a string. */
static const char *jd_get_text(const json_t *o, const char *k) {
    return jd_text(jd_get(o, k));
}

static char *jd_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *jd_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    int n;
    char *buf;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return jd_dup(""); }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return jd_dup(""); }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static int jd_is_py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace (the module's inputs are ASCII here). */
static char *jd_strip(const char *s) {
    size_t a = 0, b;
    char *p;
    if (!s) return jd_dup("");
    b = strlen(s);
    while (a < b && jd_is_py_space((unsigned char)s[a])) a++;
    while (b > a && jd_is_py_space((unsigned char)s[b - 1])) b--;
    p = (char *)malloc(b - a + 1);
    if (!p) return jd_dup("");
    memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}

/* .strip().lower() for ASCII, as used by the chat-type/boolean helpers. */
static char *jd_strip_lower(const char *s) {
    char *p = jd_strip(s);
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++) p[i] = (char)tolower((unsigned char)p[i]);
    return p;
}

/* .lower() without stripping (chat/entity type names). */
/* First case-insensitive occurrence of needle in hay, or NULL. */
static const char *jd_find_ci(const char *hay, const char *needle) {
    size_t nl;
    const char *p;
    if (!hay || !needle) return NULL;
    nl = strlen(needle);
    if (!nl) return hay;
    for (p = hay; *p; p++) {
        size_t i;
        for (i = 0; i < nl; i++) {
            unsigned char a = (unsigned char)p[i];
            if (!a) return NULL;
            if (tolower(a) != tolower((unsigned char)needle[i])) break;
        }
        if (i == nl) return p;
    }
    return NULL;
}

static char *jd_lower_dup(const char *s) {
    char *p = jd_dup(s);
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++) p[i] = (char)tolower((unsigned char)p[i]);
    return p;
}

static int jd_streq(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

/* Python str(value) for the scalar shapes these functions stringify. */
static char *jd_str_of(const json_t *v) {
    if (jd_is_none(v)) return jd_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return jd_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d) return jd_fmt("%lld", (long long)d);
        return jd_fmt("%.17g", d);
    }
    case JSON_STRING: return jd_dup(v->str_val ? v->str_val : "");
    default: {
        char *s = json_serialize(v);
        char *r = jd_dup(s ? s : "");
        free(s);
        return r;
    }
    }
}

/* Python bool(...) as JSON. */
static json_t *jd_bool(int b) { return json_bool(b != 0); }

static int jd_name_is(const json_t *err, const char *want_lower) {
    const char *name = jd_get_text(err, "name");
    char *low;
    int hit;
    if (!name) return 0;
    low = jd_strip_lower(name);
    hit = low && strcmp(low, want_lower) == 0;
    free(low);
    return hit;
}

static const char *jd_err_text(const json_t *err) {
    const char *t = jd_get_text(err, "text");
    return t ? t : "";
}

static int jd_contains_ci(const char *hay, const char *needle) {
    size_t nl;
    const char *p;
    if (!hay || !needle) return 0;
    nl = strlen(needle);
    if (!nl) return 1;
    for (p = hay; *p; p++) {
        size_t i;
        for (i = 0; i < nl; i++) {
            unsigned char a = (unsigned char)p[i];
            if (!a) return 0;
            if (tolower(a) != tolower((unsigned char)needle[i])) break;
        }
        if (i == nl) return 1;
    }
    return 0;
}

/* html.escape(text) with quote=True: & < > " ' */
static char *jd_html_escape(const char *s) {
    size_t i, n = 0, cap;
    char *out;
    if (!s) s = "";
    cap = strlen(s) * 6 + 1;
    out = (char *)malloc(cap);
    if (!out) return jd_dup("");
    for (i = 0; s[i]; i++) {
        const char *rep = NULL;
        size_t rl;
        switch (s[i]) {
        case '&': rep = "&amp;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '"': rep = "&quot;"; break;
        case '\'': rep = "&#x27;"; break;
        default: out[n++] = s[i]; continue;
        }
        rl = strlen(rep);
        memcpy(out + n, rep, rl);
        n += rl;
    }
    out[n] = '\0';
    return out;
}

/* Python float(raw) for a config/env string; 1 on success. */
static int jd_parse_float(const char *raw, double *out) {
    char *clean, *tmp, *end;
    size_t i, j = 0, n;
    double d;
    if (!raw) return 0;
    n = strlen(raw);
    tmp = (char *)malloc(n + 1);
    clean = (char *)malloc(n + 1);
    if (!tmp || !clean) { free(tmp); free(clean); return 0; }
    {
        size_t a = 0, b = n;
        while (a < b && jd_is_py_space((unsigned char)raw[a])) a++;
        while (b > a && jd_is_py_space((unsigned char)raw[b - 1])) b--;
        memcpy(tmp, raw + a, b - a);
        tmp[b - a] = '\0';
    }
    if (!tmp[0]) { free(tmp); free(clean); return 0; }
    /* PEP 515 underscores: only between digits */
    for (i = 0; tmp[i]; i++) {
        if (tmp[i] == '_') {
            if (i == 0 || !isdigit((unsigned char)tmp[i - 1]) || !isdigit((unsigned char)tmp[i + 1])) {
                free(tmp); free(clean); return 0;
            }
            continue;
        }
        clean[j++] = tmp[i];
    }
    clean[j] = '\0';
    free(tmp);
    d = strtod(clean, &end);
    if (end == clean) { free(clean); return 0; }
    while (*end && jd_is_py_space((unsigned char)*end)) end++;
    if (*end) { free(clean); return 0; }
    free(clean);
    *out = d;
    return 1;
}

/* UTF-8 decode of one code point; returns bytes consumed (0 = invalid). */
static int jd_utf8_next(const unsigned char *s, uint32_t *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return *cp >= 0x80 ? 2 : 0;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return (*cp >= 0x800 && !(*cp >= 0xD800 && *cp <= 0xDFFF)) ? 3 : 0;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
              ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return (*cp >= 0x10000 && *cp <= 0x10FFFF) ? 4 : 0;
    }
    return 0;
}

/* Python text.encode("utf-16-le") slice [off*2:(off+len)*2].decode(): offset and
 * length are UTF-16 code units; a boundary inside a surrogate pair is a
 * UnicodeDecodeError -> "". An offset past the end yields "" like the empty slice. */
static char *jd_utf16_slice(const char *src, long off, long len) {
    size_t i = 0, n, units = 0;
    size_t start_byte = 0, end_byte = 0;
    int start_ok = 0, end_ok = 0;
    char *out;
    if (!src) src = "";
    n = strlen(src);
    if (off < 0 || len <= 0) return jd_dup("");
    while (i < n) {
        uint32_t cp = 0;
        int adv = jd_utf8_next((const unsigned char *)src + i, &cp);
        size_t w = (size_t)(cp > 0xFFFF ? 2 : 1);
        if (adv <= 0) { adv = 1; w = 1; }
        if (!start_ok && units == (size_t)off) { start_byte = i; start_ok = 1; }
        if (!end_ok && units == (size_t)(off + len)) { end_byte = i; end_ok = 1; }
        units += w;
        i += (size_t)adv;
    }
    if (!start_ok && units == (size_t)off) { start_byte = i; start_ok = 1; }
    if (!end_ok && units == (size_t)(off + len)) { end_byte = i; end_ok = 1; }
    if (!start_ok) return jd_dup("");
    if (!end_ok) {
        if ((size_t)(off + len) > units) end_byte = i;
        else return jd_dup("");
    }
    if (end_byte < start_byte) return jd_dup("");
    out = (char *)malloc(end_byte - start_byte + 1);
    if (!out) return jd_dup("");
    memcpy(out, src + start_byte, end_byte - start_byte);
    out[end_byte - start_byte] = '\0';
    return out;
}

/* _is_id_like: not a bool, and a str or int. A JSON number stands for a Python
 * int only when it is integral (1.5 is a float there, so not id-like). */
static int jd_is_id_like(const json_t *v) {
    if (jd_is_none(v)) return 0;
    if (v->type == JSON_STRING) return 1;
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val;
    return 0;
}

/* parse_telegram_message_id: a positive int, or None. */
static int jd_parse_message_id(const json_t *v, long *out) {
    if (jd_is_none(v)) return 0;
    if (v->type == JSON_BOOL) return 0;
    if (v->type == JSON_NUMBER) {
        if (v->num_val != (double)(long long)v->num_val) return 0;
        if (v->num_val <= 0) return 0;
        *out = (long)v->num_val;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *t;
        long val;
        size_t i;
        if (!s) return 0;
        t = jd_strip(s);
        if (!t || !t[0]) { free(t); return 0; }
        for (i = 0; t[i]; i++)
            if (!(t[i] >= '0' && t[i] <= '9')) { free(t); return 0; }
        val = atol(t);
        free(t);
        if (val <= 0) return 0;
        *out = val;
        return 1;
    }
    return 0;
}

static json_t *jd_num_or_null(int has, long v) {
    return has ? json_number((double)v) : json_null();
}

/* ------------------------------------------------------------------ */
/* Gate p1                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p1_bold_label_html @ adapter.py:_bold_label_html */
json_t *w27_adapter_a96ad6_p1_bold_label_html(const json_t *req) {
    const char *line = jd_get_text(req, "line");
    const char *colon;
    char *out;
    if (!line) line = "";
    colon = strchr(line, ':');
    if (!colon) {
        char *escaped = jd_html_escape(line);
        out = jd_fmt("<b>%s</b>", escaped);
        free(escaped);
    } else {
        char *label = (char *)malloc((size_t)(colon - line) + 1);
        char *el, *er;
        if (!label) return json_string("");
        memcpy(label, line, (size_t)(colon - line));
        label[colon - line] = '\0';
        el = jd_html_escape(label);
        er = jd_html_escape(colon + 1);
        out = jd_fmt("<b>%s:</b>%s", el, er);
        free(label); free(el); free(er);
    }
    {
        json_t *r = json_string(out ? out : "");
        free(out);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p1_flood_cap_result @ adapter.py:_flood_cap_result */
json_t *w27_adapter_a96ad6_p1_flood_cap_result(const json_t *req) {
    const json_t *w = jd_get(req, "wait");
    double wait = (w && w->type == JSON_NUMBER) ? w->num_val : 0.0;
    char *err = jd_fmt("flood_control:%.17g", wait);
    json_t *o = json_object();
    json_set(o, "success", json_bool(0));
    json_set(o, "error", json_string(err));
    json_set(o, "retry_after", json_number(wait));
    free(err);
    return o;
}

/* PoP: w27_adapter_a96ad6_p1_env_float_clamped @ adapter.py:TelegramAdapter._env_float_clamped */
json_t *w27_adapter_a96ad6_p1_env_float_clamped(const json_t *req) {
    const char *name = jd_get_text(req, "name");
    const json_t *env = jd_get(req, "env");
    const json_t *dflt = jd_get(req, "default");
    const json_t *vmin = jd_get(req, "min_value");
    const json_t *vmax = jd_get(req, "max_value");
    double value = 0.0;
    int ok = 0;
    const char *raw = NULL;
    if (env && env->type == JSON_OBJECT && name) {
        const json_t *r = json_obj_get(env, name);
        if (r && r->type == JSON_STRING) raw = r->str_val;
        else if (r && r->type == JSON_NUMBER) { value = r->num_val; ok = 1; }
    }
    if (!ok) {
        if (raw) ok = jd_parse_float(raw, &value);
        if (!ok) value = (dflt && dflt->type == JSON_NUMBER) ? dflt->num_val : 0.0;
    }
    if (!isfinite(value)) value = (dflt && dflt->type == JSON_NUMBER) ? dflt->num_val : 0.0;
    if (vmin && vmin->type == JSON_NUMBER && value < vmin->num_val) value = vmin->num_val;
    if (vmax && vmax->type == JSON_NUMBER && value > vmax->num_val) value = vmax->num_val;
    return json_number(value);
}

/* PoP: w27_adapter_a96ad6_p1_notification_kwargs @ adapter.py:TelegramAdapter._notification_kwargs */
json_t *w27_adapter_a96ad6_p1_notification_kwargs(const json_t *req) {
    const char *mode = jd_get_text(req, "mode");
    const json_t *md = jd_get(req, "metadata");
    json_t *o;
    if (!mode) mode = "important";
    if (strcmp(mode, "important") != 0 || jd_truthy(jd_get(md, "notify")) ||
        jd_truthy(jd_get(md, "is_approval_prompt"))) {
        return json_object();
    }
    o = json_object();
    json_set(o, "disable_notification", json_bool(1));
    return o;
}

/* PoP: w27_adapter_a96ad6_p1_normalize_chat_type @ adapter.py:TelegramAdapter._normalize_chat_type */
json_t *w27_adapter_a96ad6_p1_normalize_chat_type(const json_t *req) {
    const json_t *ct = jd_get(req, "chat_type");
    int is_forum = jd_truthy(jd_get(req, "is_forum"));
    char *raw, *norm, *res;
    if (jd_is_none(ct) || !jd_truthy(ct)) raw = jd_dup("dm");
    else raw = jd_str_of(ct);
    norm = jd_strip_lower(raw);
    if (!norm || !norm[0]) { free(norm); norm = jd_dup("dm"); }
    if (!strcmp(norm, "private")) res = jd_dup("dm");
    else if (!strcmp(norm, "supergroup")) res = jd_dup(is_forum ? "forum" : "group");
    else res = jd_dup(norm);
    free(raw); free(norm);
    {
        json_t *r = json_string(res);
        free(res);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p1_gateway_chat_type @ adapter.py:TelegramAdapter._gateway_chat_type */
json_t *w27_adapter_a96ad6_p1_gateway_chat_type(const json_t *req) {
    const json_t *chat = jd_get(req, "chat");
    int is_forum = jd_truthy(jd_get(req, "is_forum"));
    char *ct_raw, *norm, *res;
    if (!jd_is_none(chat) && jd_truthy(chat)) {
        char *full = jd_str_of(jd_get(chat, "type"));
        char *dot = strrchr(full, '.');
        ct_raw = jd_dup(dot ? dot + 1 : full);
        free(full);
    } else {
        ct_raw = jd_dup("");
    }
    norm = jd_strip_lower(ct_raw);
    if (!norm || !norm[0]) { free(norm); norm = jd_dup("dm"); }
    if (!strcmp(norm, "private")) res = jd_dup("dm");
    else if (!strcmp(norm, "supergroup")) res = jd_dup(is_forum ? "forum" : "group");
    else res = jd_dup(norm);
    free(norm);
    if (!(strcmp(res, "group") == 0 || strcmp(res, "forum") == 0 || strcmp(res, "channel") == 0)) {
        free(res);
        res = jd_dup("dm");
    }
    free(ct_raw);
    {
        json_t *r = json_string(res);
        free(res);
        return r;
    }
}

/* ------------------------------------------------------------------ */
/* Gate p2                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p2_metadata_thread_id @ adapter.py:TelegramAdapter._metadata_thread_id */
json_t *w27_adapter_a96ad6_p2_metadata_thread_id(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const json_t *tid = jd_get(md, "thread_id");
    if (!jd_truthy(tid)) tid = jd_get(md, "message_thread_id");
    if (jd_is_none(tid)) return json_null();
    {
        char *s = jd_str_of(tid);
        json_t *r = json_string(s);
        free(s);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p2_metadata_direct_messages_topic_id @ adapter.py:TelegramAdapter._metadata_direct_messages_topic_id */
json_t *w27_adapter_a96ad6_p2_metadata_direct_messages_topic_id(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const json_t *tid = jd_get(md, "direct_messages_topic_id");
    if (!jd_truthy(tid)) tid = jd_get(md, "telegram_direct_messages_topic_id");
    if (jd_is_none(tid)) return json_null();
    {
        char *s = jd_str_of(tid);
        json_t *r = json_string(s);
        free(s);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p2_metadata_reply_to_message_id @ adapter.py:TelegramAdapter._metadata_reply_to_message_id */
json_t *w27_adapter_a96ad6_p2_metadata_reply_to_message_id(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    long v = 0;
    int has = jd_parse_message_id(jd_get(md, "telegram_reply_to_message_id"), &v);
    return jd_num_or_null(has, v);
}

/* PoP: w27_adapter_a96ad6_p2_dm_topic_fallback @ adapter.py:TelegramAdapter._dm_topic_fallback */
json_t *w27_adapter_a96ad6_p2_dm_topic_fallback(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    return jd_bool(jd_truthy(md) && jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback")));
}

/* PoP: w27_adapter_a96ad6_p2_dm_topic_missing_anchor_error @ adapter.py:TelegramAdapter._dm_topic_missing_anchor_error */
json_t *w27_adapter_a96ad6_p2_dm_topic_missing_anchor_error(const json_t *req) {
    (void)req;
    return json_string("Telegram DM topic delivery requires a reply anchor; "
                       "refusing to send outside the requested topic");
}

/* PoP: w27_adapter_a96ad6_p2_is_private_dm_topic_send @ adapter.py:TelegramAdapter._is_private_dm_topic_send */
json_t *w27_adapter_a96ad6_p2_is_private_dm_topic_send(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const json_t *thread_id = jd_get(req, "thread_id");
    const json_t *direct = jd_get(md, "direct_messages_topic_id");
    if (!jd_truthy(direct)) direct = jd_get(md, "telegram_direct_messages_topic_id");
    if (!jd_is_none(direct)) {
        long v = 0;
        int anchor = jd_parse_message_id(jd_get(md, "telegram_reply_to_message_id"), &v);
        return jd_bool(jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback")) && anchor);
    }
    if (jd_truthy(md) && jd_truthy(jd_get(md, "telegram_dm_topic_created_for_send"))) return jd_bool(0);
    return jd_bool(jd_truthy(thread_id) && jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback")));
}

/* ------------------------------------------------------------------ */
/* Gate p3                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p3_reply_to_message_id_for_send @ adapter.py:TelegramAdapter._reply_to_message_id_for_send */
json_t *w27_adapter_a96ad6_p3_reply_to_message_id_for_send(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const char *mode = jd_get_text(req, "reply_to_mode");
    long v = 0;
    if (jd_parse_message_id(jd_get(req, "reply_to"), &v)) return json_number((double)v);
    if (jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback")) && !jd_streq(mode, "off")) {
        int has = jd_parse_message_id(jd_get(md, "telegram_reply_to_message_id"), &v);
        return jd_num_or_null(has, v);
    }
    return json_null();
}

/* _message_thread_id_for_send's numeric half: 1 is the forum General topic,
 * which sendMessage rejects, so it is dropped. */
static int jd_thread_id_for_send(const json_t *thread_id, long *out) {
    char *s, *stripped = NULL;
    int ok;
    if (!jd_truthy(thread_id)) return 0;
    s = jd_str_of(thread_id);
    /* str(thread_id) == "1" is compared on the RAW text: " 1" is not the General
     * topic, and int(" 1") is 1. Stripping first turned " 1" into "1" and dropped
     * the topic. Only the integer parse tolerates surrounding whitespace. */
    ok = s && s[0] && strcmp(s, W27_GENERAL_TOPIC_THREAD_ID) != 0;
    if (ok) {
        char *end = NULL;
        long v;
        stripped = jd_strip(s);
        v = stripped ? strtol(stripped, &end, 10) : 0;
        if (!stripped || !end || *end || v < 0) ok = 0;
        else *out = v;
        free(stripped);
    }
    free(s);
    return ok;
}

/* _direct_topic_kwargs: None when no direct topic id. */
static json_t *jd_direct_topic_kwargs(const json_t *md) {
    const json_t *direct = jd_get(md, "direct_messages_topic_id");
    if (!jd_truthy(direct)) direct = jd_get(md, "telegram_direct_messages_topic_id");
    if (jd_is_none(direct)) return NULL;
    {
        char *s = jd_str_of(direct);
        char *t = jd_strip(s);
        long v = t ? strtol(t, NULL, 10) : 0;
        json_t *o = json_object();
        json_set(o, "message_thread_id", json_null());
        json_set(o, "direct_messages_topic_id", json_number((double)v));
        free(s); free(t);
        return o;
    }
}

/* PoP: w27_adapter_a96ad6_p3_thread_kwargs_for_send @ adapter.py:TelegramAdapter._thread_kwargs_for_send */
json_t *w27_adapter_a96ad6_p3_thread_kwargs_for_send(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const char *mode = jd_get_text(req, "reply_to_mode");
    const json_t *thread_id = jd_get(req, "thread_id");
    const json_t *reply_arg = jd_get(req, "reply_to_message_id");
    int fallback = jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback"));
    long reply = 0;
    int has_reply = jd_parse_message_id(reply_arg, &reply);
    long tid = 0;
    json_t *o;

    if (fallback && !jd_streq(mode, "off")) {
        if (!has_reply) has_reply = jd_parse_message_id(jd_get(md, "telegram_reply_to_message_id"), &reply);
        if (!has_reply) {
            if (jd_thread_id_for_send(thread_id, &tid)) {
                o = json_object();
                json_set(o, "message_thread_id", json_number((double)tid));
                return o;
            }
            o = jd_direct_topic_kwargs(md);
            return o ? o : json_object();
        }
    } else if (!fallback) {
        o = jd_direct_topic_kwargs(md);
        if (o) return o;
    }
    o = json_object();
    if (jd_thread_id_for_send(thread_id, &tid)) json_set(o, "message_thread_id", json_number((double)tid));
    else json_set(o, "message_thread_id", json_null());
    return o;
}

/* PoP: w27_adapter_a96ad6_p3_direct_topic_kwargs @ adapter.py:TelegramAdapter._direct_topic_kwargs */
json_t *w27_adapter_a96ad6_p3_direct_topic_kwargs(const json_t *req) {
    json_t *o = jd_direct_topic_kwargs(jd_get(req, "metadata"));
    return o ? o : json_null();
}

/* PoP: w27_adapter_a96ad6_p3_thread_kwargs_for_draft @ adapter.py:TelegramAdapter._thread_kwargs_for_draft */
json_t *w27_adapter_a96ad6_p3_thread_kwargs_for_draft(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const char *mode = jd_get_text(req, "reply_to_mode");
    const json_t *thread_id = jd_get(md, "thread_id");
    json_t *full, *out;
    size_t i, n;
    if (!jd_truthy(thread_id)) thread_id = jd_get(md, "message_thread_id");
    {
        long anchor = 0;
        int has_anchor = jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback")) && !jd_streq(mode, "off") &&
                         jd_parse_message_id(jd_get(md, "telegram_reply_to_message_id"), &anchor);
        json_t *sub = json_object();
        json_set(sub, "metadata", json_copy(md ? md : json_null()));
        json_set(sub, "thread_id", json_copy(thread_id ? thread_id : json_null()));
        if (mode) json_set(sub, "reply_to_mode", json_string(mode));
        if (has_anchor) json_set(sub, "reply_to_message_id", json_number((double)anchor));
        full = w27_adapter_a96ad6_p3_thread_kwargs_for_send(sub);
        json_free(sub);
    }
    out = json_object();
    n = json_object_size(full);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(full, i);
        json_t *v = json_object_get_at(full, i);
        if (!v || v->type == JSON_NULL) continue;
        json_set(out, k, json_copy(v));
    }
    json_free(full);
    return out;
}

/* PoP: w27_adapter_a96ad6_p3_message_thread_id_for_send @ adapter.py:TelegramAdapter._message_thread_id_for_send */
json_t *w27_adapter_a96ad6_p3_message_thread_id_for_send(const json_t *req) {
    long v = 0;
    if (!jd_thread_id_for_send(jd_get(req, "thread_id"), &v)) return json_null();
    return json_number((double)v);
}

/* PoP: w27_adapter_a96ad6_p3_message_thread_id_for_typing @ adapter.py:TelegramAdapter._message_thread_id_for_typing */
json_t *w27_adapter_a96ad6_p3_message_thread_id_for_typing(const json_t *req) {
    const json_t *thread_id = jd_get(req, "thread_id");
    char *s, *t;
    long v;
    if (!jd_truthy(thread_id)) return json_null();
    s = jd_str_of(thread_id);
    t = jd_strip(s);
    v = t ? strtol(t, NULL, 10) : 0;
    free(s); free(t);
    return json_number((double)v);
}

/* ------------------------------------------------------------------ */
/* Gate p4                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p4_is_thread_not_found_error @ adapter.py:TelegramAdapter._is_thread_not_found_error */
json_t *w27_adapter_a96ad6_p4_is_thread_not_found_error(const json_t *req) {
    return jd_bool(jd_contains_ci(jd_err_text(jd_get(req, "error")), "thread not found"));
}

/* PoP: w27_adapter_a96ad6_p4_is_bad_request_error @ adapter.py:TelegramAdapter._is_bad_request_error */
json_t *w27_adapter_a96ad6_p4_is_bad_request_error(const json_t *req) {
    const json_t *err = jd_get(req, "error");
    const char *name = jd_get_text(err, "name");
    char *low;
    int hit = 0;
    if (!name) return jd_bool(0);
    low = jd_strip_lower(name);
    if (low) {
        size_t n = strlen(low), m = strlen("badrequest");
        hit = strcmp(low, "badrequest") == 0 || (n >= m && strcmp(low + n - m, "badrequest") == 0);
    }
    free(low);
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p4_is_reply_anchor_missing_error @ adapter.py:TelegramAdapter._is_reply_anchor_missing_error */
json_t *w27_adapter_a96ad6_p4_is_reply_anchor_missing_error(const json_t *req) {
    const char *t = jd_err_text(jd_get(req, "error"));
    return jd_bool(jd_contains_ci(t, "message to be replied not found") ||
                   jd_contains_ci(t, "reply message not found"));
}

/* PoP: w27_adapter_a96ad6_p4_should_retry_without_dm_topic_reply_anchor @ adapter.py:TelegramAdapter._should_retry_without_dm_topic_reply_anchor */
json_t *w27_adapter_a96ad6_p4_should_retry_without_dm_topic_reply_anchor(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const json_t *err = jd_get(req, "error");
    const json_t *anchor = jd_get(req, "reply_to_message_id");
    const char *name;
    char *low;
    int bad = 0, hit;
    if (!jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback"))) return jd_bool(0);
    if (jd_is_none(anchor)) return jd_bool(0);
    name = jd_get_text(err, "name");
    low = name ? jd_strip_lower(name) : NULL;
    if (low) {
        size_t n = strlen(low), m = strlen("badrequest");
        bad = strcmp(low, "badrequest") == 0 || (n >= m && strcmp(low + n - m, "badrequest") == 0);
    }
    free(low);
    if (!bad) return jd_bool(0);
    hit = jd_contains_ci(jd_err_text(err), "message to be replied not found") ||
          jd_contains_ci(jd_err_text(err), "reply message not found");
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p4_looks_like_polling_conflict @ adapter.py:TelegramAdapter._looks_like_polling_conflict */
json_t *w27_adapter_a96ad6_p4_looks_like_polling_conflict(const json_t *req) {
    const json_t *err = jd_get(req, "error");
    const char *t = jd_err_text(err);
    return jd_bool(jd_name_is(err, "conflict") ||
                   jd_contains_ci(t, "terminated by other getupdates request") ||
                   jd_contains_ci(t, "another bot instance is running"));
}

/* PoP: w27_adapter_a96ad6_p4_looks_like_auth_error @ adapter.py:TelegramAdapter._looks_like_auth_error */
json_t *w27_adapter_a96ad6_p4_looks_like_auth_error(const json_t *req) {
    const json_t *err = jd_get(req, "error");
    return jd_bool(jd_name_is(err, "invalidtoken") || jd_name_is(err, "forbidden"));
}


/* ------------------------------------------------------------------ */
/* Python string-shape helpers used from gate p8/p9 on               */
/* ------------------------------------------------------------------ */

/* text[:n] over code points (the module slices captions/emoji by character). */
static char *jd_cp_prefix(const char *s, size_t n) {
    size_t i = 0, count = 0, bytes = 0;
    if (!s) return jd_dup("");
    while (s[i] && count < n) {
        uint32_t cp = 0;
        int adv = jd_utf8_next((const unsigned char *)s + i, &cp);
        if (adv <= 0) adv = 1;
        i += (size_t)adv;
        bytes = i;
        count++;
    }
    {
        char *p = (char *)malloc(bytes + 1);
        if (!p) return jd_dup("");
        memcpy(p, s, bytes);
        p[bytes] = '\0';
        return p;
    }
}

static size_t jd_cp_len(const char *s) {
    size_t i = 0, count = 0;
    if (!s) return 0;
    while (s[i]) {
        uint32_t cp = 0;
        int adv = jd_utf8_next((const unsigned char *)s + i, &cp);
        if (adv <= 0) adv = 1;
        i += (size_t)adv;
        count++;
    }
    return count;
}

/* Split `content` into lines at '\n' (trailing '\r' dropped), like str.splitlines
 * for the shapes this module feeds it. */
static char **jd_split_lines(const char *content, size_t *count) {
    size_t cap = 8, n = 0, i = 0, start = 0, len;
    char **out;
    if (!content) content = "";
    len = strlen(content);
    out = (char **)malloc(cap * sizeof(char *));
    if (!out) { *count = 0; return NULL; }
    while (1) {
        int at_end = (i == len);
        if (at_end || content[i] == '\n') {
            size_t e = i;
            char *line;
            if (e > start && content[e - 1] == '\r') e--;
            line = (char *)malloc(e - start + 1);
            if (!line) break;
            memcpy(line, content + start, e - start);
            line[e - start] = '\0';
            if (n + 1 >= cap) {
                char **grown;
                cap *= 2;
                grown = (char **)realloc(out, cap * sizeof(char *));
                if (!grown) { free(line); break; }
                out = grown;
            }
            out[n++] = line;
            if (at_end) break;
            start = i + 1;
        }
        i++;
    }
    *count = n;
    return out;
}

static void jd_free_lines(char **lines, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

/* TABLE_SEPARATOR_RE.match(line) from gateway.platforms.helpers:
 * ^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$ */
static int jd_cell_dashes(const char *s, size_t *pi) {
    size_t i = *pi;
    int dashes = 0;
    if (s[i] == ':') i++;
    while (s[i] == '-') { i++; dashes++; }
    if (!dashes) return 0;
    if (s[i] == ':') i++;
    *pi = i;
    return 1;
}

static int jd_table_separator_line(const char *line) {
    size_t i = 0;
    int seps = 0;
    if (!line) return 0;
    while (line[i] && jd_is_py_space((unsigned char)line[i])) i++;
    if (line[i] == '|') i++;
    while (line[i] && jd_is_py_space((unsigned char)line[i])) i++;
    if (!jd_cell_dashes(line, &i)) return 0;
    while (line[i] && jd_is_py_space((unsigned char)line[i])) i++;
    while (line[i] == '|') {
        size_t j = i + 1;
        while (line[j] && jd_is_py_space((unsigned char)line[j])) j++;
        if (!jd_cell_dashes(line, &j)) break;   /* the trailing '|' is not a cell */
        while (line[j] && jd_is_py_space((unsigned char)line[j])) j++;
        i = j;
        seps++;
    }
    if (seps < 1) return 0;
    if (line[i] == '|') {
        i++;
        while (line[i] && jd_is_py_space((unsigned char)line[i])) i++;
    }
    return line[i] == '\0';
}

/* (?m)^\s*[-*]\s+\[[ xX]\]\s+ */
static int jd_task_list_shape(const char *content) {
    size_t nlines = 0, li;
    char **lines = jd_split_lines(content, &nlines);
    int hit = 0;
    for (li = 0; li < nlines && !hit; li++) {
        const char *s = lines[li];
        size_t i = 0;
        while (s[i] && jd_is_py_space((unsigned char)s[i])) i++;
        if (s[i] != '-' && s[i] != '*') continue;
        i++;
        if (!jd_is_py_space((unsigned char)s[i])) continue;
        while (s[i] && jd_is_py_space((unsigned char)s[i])) i++;
        if (s[i] != '[') continue;
        i++;
        if (!(s[i] == ' ' || s[i] == 'x' || s[i] == 'X')) continue;
        i++;
        if (s[i] != ']') continue;
        i++;
        if (!jd_is_py_space((unsigned char)s[i])) continue;
        hit = 1;
    }
    jd_free_lines(lines, nlines);
    return hit;
}

/* (?m)^<details\b|^</details>|^<summary\b|^</summary> (case sensitive) */
static int jd_details_line_shape(const char *content) {
    size_t nlines = 0, li;
    char **lines = jd_split_lines(content, &nlines);
    int hit = 0;
    for (li = 0; li < nlines && !hit; li++) {
        const char *s = lines[li];
        if (!strncmp(s, "<details", 8)) {
            char c = s[8];
            if (!(isalnum((unsigned char)c) || c == '_')) hit = 1;
        } else if (!strncmp(s, "</details>", 10)) {
            hit = 1;
        } else if (!strncmp(s, "<summary", 8)) {
            char c = s[8];
            if (!(isalnum((unsigned char)c) || c == '_')) hit = 1;
        } else if (!strncmp(s, "</summary>", 10)) {
            hit = 1;
        }
    }
    jd_free_lines(lines, nlines);
    return hit;
}

/* The five math alternations of _RICH_MATH_IN_DETAILS_RE, DOTALL|IGNORECASE. */
/* case-insensitive n-byte compare, for the re.IGNORECASE patterns */
static int jd_ncmp_ci(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb) return 1;
        if (!ca) return 0;
    }
    return 0;
}

static int jd_math_in(const char *s) {
    static const char *const CMDS[] = {"sum", "frac", "alpha", "beta", "gamma", "delta", "theta",
                                       "lambda", "mu", "pi", "sigma", "int", "prod", "sqrt", "lim",
                                       "infty"};
    static const char *const ENVS[] = {"equation", "align", "matrix", "cases"};
    size_t i;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        if (s[i] == '$' && s[i + 1] == '$') {
            if (strstr(s + i + 2, "$$")) return 1;
            continue;
        }
        if (s[i] != '\\') continue;
        if (s[i + 1] == '[' && strstr(s + i + 2, "\\]")) return 1;
        if (s[i + 1] == '(' && strstr(s + i + 2, "\\)")) return 1;
        {
            size_t c;
            /* the upstream pattern is compiled re.IGNORECASE, so \BEGIN{ALIGN}
             * counts as math exactly like \begin{align} */
            for (c = 0; c < sizeof(CMDS) / sizeof(CMDS[0]); c++) {
                size_t l = strlen(CMDS[c]);
                if (!jd_ncmp_ci(s + i + 1, CMDS[c], l)) return 1;
            }
        }
        if (!jd_ncmp_ci(s + i + 1, "begin{", 6)) {
            size_t e;
            for (e = 0; e < sizeof(ENVS) / sizeof(ENVS[0]); e++) {
                size_t l = strlen(ENVS[e]);
                if (!jd_ncmp_ci(s + i + 7, ENVS[e], l) && s[i + 7 + l] == '}') return 1;
            }
        }
    }
    return 0;
}

/* _RICH_DETAILS_RE.findall: every <details ...> ... </details> block, non-greedy. */
static int jd_details_math(const char *content) {
    size_t i = 0;
    if (!content) return 0;
    while (content[i]) {
        if (content[i] == '<' && jd_contains_ci(content + i, "<details")) {
            const char *open = content + i;
            const char *gt = strchr(open, '>');
            size_t blen;
            char *block;
            int hit;
            if (!gt) return 0;
            {
                /* non-greedy: the first </details> after the tag */
                const char *close = jd_find_ci(gt, "</details>");
                if (!close) return 0;
                blen = (size_t)(close - open) + strlen("</details>");
            }
            block = (char *)malloc(blen + 1);
            if (!block) return 0;
            memcpy(block, open, blen);
            block[blen] = '\0';
            hit = jd_math_in(block);
            free(block);
            if (hit) return 1;
            i = (size_t)(open - content) + blen;
            continue;
        }
        i++;
    }
    return 0;
}

/* _RICH_CJK_RE: Japanese, CJK Ext A / Unified / Compat, Hangul, Ext B. */
static int jd_has_cjk(const char *s) {
    size_t i = 0;
    if (!s) return 0;
    while (s[i]) {
        uint32_t cp = 0;
        int adv = jd_utf8_next((const unsigned char *)s + i, &cp);
        if (adv <= 0) { i++; continue; }
        i += (size_t)adv;
        if ((cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
            (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xAC00 && cp <= 0xD7AF) ||
            (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x323AF)) {
            return 1;
        }
    }
    return 0;
}

/* int() over the shapes file_size / status_code take; 0 on TypeError/ValueError. */
static long long jd_int_or_zero(const json_t *v) {
    char *s, *t;
    char *end = NULL;
    long long r;
    if (jd_is_none(v)) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return (long long)v->num_val; /* int() truncates toward zero */
    case JSON_STRING:
        s = jd_strip(v->str_val);
        if (!s || !s[0]) { free(s); return 0; }
        t = s;
        if (*t == '+' || *t == '-') t++;
        if (!*t) { free(s); return 0; }
        {
            const char *q;
            for (q = t; *q; q++)
                if (!isdigit((unsigned char)*q)) { free(s); return 0; }
        }
        r = strtol(s, &end, 10);
        free(s);
        return r;
    default: return 0; /* list/dict/None -> TypeError */
    }
}

/* ------------------------------------------------------------------ */
/* Gate p5                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p5_looks_like_network_error @ adapter.py:TelegramAdapter._looks_like_network_error */
json_t *w27_adapter_a96ad6_p5_looks_like_network_error(const json_t *req) {
    const json_t *err = jd_get(req, "error");
    char *low = jd_get_text(err, "name") ? jd_strip_lower(jd_get_text(err, "name")) : NULL;
    int result = 0;
    if (low) {
        if (!strcmp(low, "badrequest") || !strcmp(low, "invalidtoken") ||
            !strcmp(low, "forbidden") || !strcmp(low, "retryafter")) {
            result = 0;
        } else if (!strcmp(low, "networkerror") || !strcmp(low, "timedout") ||
                   !strcmp(low, "connectionerror")) {
            result = 1;
        } else {
            result = jd_truthy(jd_get(err, "os_error"));
        }
    } else {
        result = jd_truthy(jd_get(err, "os_error"));
    }
    free(low);
    return jd_bool(result);
}

/* PoP: w27_adapter_a96ad6_p5_exception_graph_matches @ adapter.py:TelegramAdapter._exception_graph_matches */
json_t *w27_adapter_a96ad6_p5_exception_graph_matches(const json_t *req) {
    const json_t *graph = jd_get(req, "graph");
    const char *name_marker = jd_get_text(req, "name_marker");
    const json_t *markers = jd_get(req, "text_markers");
    size_t i, n = json_len(graph);
    char *nm = name_marker ? jd_strip_lower(name_marker) : NULL;
    int hit = 0;
    for (i = 0; i < n && !hit; i++) {
        const json_t *g = json_get(graph, i);
        const char *gname = jd_get_text(g, "name");
        const char *gtext = jd_err_text(g);
        char *glow = gname ? jd_strip_lower(gname) : NULL;
        if (nm && glow && strstr(glow, nm)) { hit = 1; free(glow); break; }
        free(glow);
        {
            size_t m, mn = json_len(markers);
            for (m = 0; m < mn; m++) {
                const char *mk = jd_text(json_get(markers, m));
                char *lmk;
                int found;
                if (!mk) continue;
                lmk = jd_strip_lower(mk);
                found = lmk && jd_contains_ci(gtext, lmk);
                free(lmk);
                if (found) { hit = 1; break; }
            }
        }
    }
    free(nm);
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p5_looks_like_connect_timeout @ adapter.py:TelegramAdapter._looks_like_connect_timeout */
json_t *w27_adapter_a96ad6_p5_looks_like_connect_timeout(const json_t *req) {
    const json_t *graph = jd_get(req, "graph");
    size_t i, n = json_len(graph);
    int hit = 0;
    for (i = 0; i < n && !hit; i++) {
        const json_t *g = json_get(graph, i);
        const char *gname = jd_get_text(g, "name");
        const char *gtext = jd_err_text(g);
        char *glow = gname ? jd_strip_lower(gname) : NULL;
        if (glow && strstr(glow, "connecttimeout")) { hit = 1; free(glow); break; }
        free(glow);
        if (jd_contains_ci(gtext, "connect timeout") || jd_contains_ci(gtext, "connect timed out")) hit = 1;
    }
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p5_looks_like_pool_timeout @ adapter.py:TelegramAdapter._looks_like_pool_timeout */
json_t *w27_adapter_a96ad6_p5_looks_like_pool_timeout(const json_t *req) {
    const json_t *graph = jd_get(req, "graph");
    size_t i, n = json_len(graph);
    int hit = 0;
    for (i = 0; i < n && !hit; i++) {
        const json_t *g = json_get(graph, i);
        const char *gname = jd_get_text(g, "name");
        const char *gtext = jd_err_text(g);
        char *glow = gname ? jd_strip_lower(gname) : NULL;
        if (glow && strstr(glow, "pooltimeout")) { hit = 1; free(glow); break; }
        free(glow);
        if (jd_contains_ci(gtext, "pool timeout")) hit = 1;
        else if (jd_contains_ci(gtext, "connection pool") && jd_contains_ci(gtext, "occupied")) hit = 1;
    }
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p5_coerce_bool_extra @ adapter.py:TelegramAdapter._coerce_bool_extra */
json_t *w27_adapter_a96ad6_p5_coerce_bool_extra(const json_t *req) {
    const json_t *extra = jd_get(req, "extra");
    const char *key = jd_get_text(req, "key");
    const json_t *dflt = jd_get(req, "default");
    int default_bool = jd_truthy(dflt);
    const json_t *value = (extra && extra->type == JSON_OBJECT && key) ? json_obj_get(extra, key) : NULL;
    if (jd_is_none(value)) return jd_bool(default_bool);
    if (value->type == JSON_STRING) {
        char *low = jd_strip_lower(value->str_val);
        int result;
        if (low && (!strcmp(low, "true") || !strcmp(low, "1") || !strcmp(low, "yes") || !strcmp(low, "on")))
            result = 1;
        else if (low && (!strcmp(low, "false") || !strcmp(low, "0") || !strcmp(low, "no") || !strcmp(low, "off")))
            result = 0;
        else result = default_bool;
        free(low);
        return jd_bool(result);
    }
    return jd_bool(jd_truthy(value));
}

/* ------------------------------------------------------------------ */
/* Gate p6                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p6_details_math_crash_shape @ adapter.py:TelegramAdapter._has_telegram_desktop_details_math_crash_shape */
json_t *w27_adapter_a96ad6_p6_details_math_crash_shape(const json_t *req) {
    const char *content = jd_get_text(req, "content");
    if (!content || !content[0]) return jd_bool(0);
    return jd_bool(jd_details_math(content));
}

/* PoP: w27_adapter_a96ad6_p6_cjk_rich_garble_shape @ adapter.py:TelegramAdapter._has_telegram_desktop_cjk_rich_garble_shape */
json_t *w27_adapter_a96ad6_p6_cjk_rich_garble_shape(const json_t *req) {
    const char *content = jd_get_text(req, "content");
    return jd_bool(jd_truthy(jd_get(req, "content")) && jd_has_cjk(content));
}

/* PoP: w27_adapter_a96ad6_p6_needs_rich_rendering @ adapter.py:TelegramAdapter._needs_rich_rendering */
json_t *w27_adapter_a96ad6_p6_needs_rich_rendering(const json_t *req) {
    const char *content = jd_get_text(req, "content");
    size_t nlines = 0, li;
    char **lines;
    int hit = 0;
    if (!content || !content[0]) return jd_bool(0);
    lines = jd_split_lines(content, &nlines);
    for (li = 0; li < nlines && !hit; li++)
        if (jd_table_separator_line(lines[li])) hit = 1;
    jd_free_lines(lines, nlines);
    if (hit) return jd_bool(1);
    if (jd_task_list_shape(content)) return jd_bool(1);
    if (jd_details_line_shape(content)) return jd_bool(1);
    return jd_bool(strstr(content, "$$") != NULL);
}

/* PoP: w27_adapter_a96ad6_p6_is_rich_capability_error @ adapter.py:TelegramAdapter._is_rich_capability_error */
json_t *w27_adapter_a96ad6_p6_is_rich_capability_error(const json_t *req) {
    const json_t *exc = jd_get(req, "error");
    const char *name = jd_get_text(exc, "name");
    const char *s = jd_err_text(exc);
    const json_t *code = jd_get(exc, "error_code");
    char *low;
    int hit = 0;
    low = name ? jd_strip_lower(name) : NULL;
    if (low && (!strcmp(low, "endpointnotfound") || !strcmp(low, "invalidtoken"))) hit = 1;
    free(low);
    if (!hit) {
        low = name ? jd_strip_lower(name) : NULL;
        if (low && (!strcmp(low, "attributeerror") || !strcmp(low, "typeerror") ||
                    !strcmp(low, "notimplementederror")))
            hit = 1;
        free(low);
    }
    if (!hit && code && code->type == JSON_NUMBER && code->num_val == 404) hit = 1;
    if (!hit) {
        char *sl = jd_strip_lower(s);
        if (sl) {
            int methodish = strstr(sl, "method") || strstr(sl, "endpoint");
            int notfound = strstr(sl, "not found") || strstr(sl, "does not exist");
            if (methodish && notfound) hit = 1;
            if (!hit && strstr(sl, "no such method")) hit = 1;
        }
        free(sl);
    }
    return jd_bool(hit);
}

static int jd_bad_request_name(const json_t *err) {
    const char *name = jd_get_text(err, "name");
    char *low;
    int hit = 0;
    if (!name) return 0;
    low = jd_strip_lower(name);
    if (low) {
        size_t n = strlen(low), m = strlen("badrequest");
        hit = strcmp(low, "badrequest") == 0 || (n >= m && strcmp(low + n - m, "badrequest") == 0);
    }
    free(low);
    return hit;
}

/* PoP: w27_adapter_a96ad6_p6_is_rich_fallback_error @ adapter.py:TelegramAdapter._is_rich_fallback_error */
json_t *w27_adapter_a96ad6_p6_is_rich_fallback_error(const json_t *req) {
    const json_t *exc = jd_get(req, "error");
    const char *s = jd_err_text(exc);
    if (jd_bad_request_name(exc)) return jd_bool(1);
    {
        const char *name = jd_get_text(exc, "name");
        const json_t *code = jd_get(exc, "error_code");
        char *low = name ? jd_strip_lower(name) : NULL;
        int cap = 0;
        if (low && (!strcmp(low, "endpointnotfound") || !strcmp(low, "invalidtoken") ||
                    !strcmp(low, "attributeerror") || !strcmp(low, "typeerror") ||
                    !strcmp(low, "notimplementederror")))
            cap = 1;
        free(low);
        if (!cap && code && code->type == JSON_NUMBER && code->num_val == 404) cap = 1;
        if (!cap) {
            char *sl = jd_strip_lower(s);
            if (sl) {
                if ((strstr(sl, "method") || strstr(sl, "endpoint")) &&
                    (strstr(sl, "not found") || strstr(sl, "does not exist")))
                    cap = 1;
                if (!cap && strstr(sl, "no such method")) cap = 1;
            }
            free(sl);
        }
        if (cap) return jd_bool(1);
    }
    return jd_bool(jd_contains_ci(s, "unsupported") || jd_contains_ci(s, "not implemented"));
}

/* ------------------------------------------------------------------ */
/* Gate p7                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p7_is_timed_out @ adapter.py:TelegramAdapter._is_timed_out */
json_t *w27_adapter_a96ad6_p7_is_timed_out(const json_t *req) {
    /* The PTB TimedOut isinstance() rung needs the SDK (seam); the message-text
     * rung is what a build without python-telegram-bot can decide. */
    return jd_bool(jd_contains_ci(jd_err_text(jd_get(req, "error")), "timed out"));
}

/* reply_to_mode policy: "off" never threads, "all" always, "first" only chunk 0. */
static int jd_should_thread_reply(const char *reply_to, long chunk_index, const char *mode) {
    if (!reply_to || !reply_to[0]) return 0;
    if (!mode) mode = "first";
    if (!strcmp(mode, "off")) return 0;
    if (!strcmp(mode, "all")) return 1;
    return chunk_index == 0;
}

/* PoP: w27_adapter_a96ad6_p7_should_thread_reply @ adapter.py:TelegramAdapter._should_thread_reply */
json_t *w27_adapter_a96ad6_p7_should_thread_reply(const json_t *req) {
    const char *reply_to = jd_get_text(req, "reply_to");
    const json_t *ci = jd_get(req, "chunk_index");
    const char *mode = jd_get_text(req, "reply_to_mode");
    long idx = (ci && ci->type == JSON_NUMBER) ? (long)ci->num_val : 0;
    return jd_bool(jd_should_thread_reply(reply_to, idx, mode));
}

/* PoP: w27_adapter_a96ad6_p7_chunk_reply_routing @ adapter.py:TelegramAdapter._chunk_reply_routing */
json_t *w27_adapter_a96ad6_p7_chunk_reply_routing(const json_t *req) {
    const json_t *md = jd_get(req, "metadata");
    const char *mode = jd_get_text(req, "reply_to_mode");
    const json_t *ci = jd_get(req, "chunk_index");
    long index = (ci && ci->type == JSON_NUMBER) ? (long)ci->num_val : 0;
    long meta_reply = 0, direct_anchor = 0;
    int has_meta_reply = jd_parse_message_id(jd_get(md, "telegram_reply_to_message_id"), &meta_reply);
    const json_t *thread_id = jd_get(req, "thread_id");
    const json_t *direct = jd_get(md, "direct_messages_topic_id");
    int fallback = jd_truthy(jd_get(md, "telegram_dm_topic_reply_fallback"));
    int private_send;
    int anchor_off;
    long reply_source = 0;
    int has_source = 0;
    int should_thread = 0;
    json_t *out;

    if (!jd_truthy(direct)) direct = jd_get(md, "telegram_direct_messages_topic_id");
    if (!jd_is_none(direct)) {
        private_send = fallback && has_meta_reply;
    } else if (jd_truthy(md) && jd_truthy(jd_get(md, "telegram_dm_topic_created_for_send"))) {
        private_send = 0;
    } else {
        private_send = jd_truthy(thread_id) && fallback;
    }
    anchor_off = private_send && jd_streq(mode, "off") && fallback;

    has_source = jd_parse_message_id(jd_get(req, "reply_to"), &reply_source);
    if (!has_source && private_send && has_meta_reply) {
        reply_source = meta_reply;
        has_source = 1;
    }
    (void)direct_anchor;
    if (private_send) should_thread = has_source && !jd_streq(mode, "off");
    else {
        char *src = has_source ? jd_fmt("%ld", reply_source) : NULL;
        should_thread = jd_should_thread_reply(src, index, mode);
        free(src);
    }
    out = json_array();
    json_append(out, json_bool(private_send));
    json_append(out, json_bool(anchor_off));
    json_append(out, (should_thread && has_source) ? json_number((double)reply_source) : json_null());
    return out;
}

/* PoP: w27_adapter_a96ad6_p7_compute_single_send_routing @ adapter.py:TelegramAdapter._compute_single_send_routing */
json_t *w27_adapter_a96ad6_p7_compute_single_send_routing(const json_t *req) {
    const json_t *routing = w27_adapter_a96ad6_p7_chunk_reply_routing(req);
    int private_send = jd_truthy(json_get(routing, 0));
    int anchor_off = jd_truthy(json_get(routing, 1));
    const json_t *reply = json_get(routing, 2);
    const json_t *md = jd_get(req, "metadata");
    const char *mode = jd_get_text(req, "reply_to_mode");
    json_t *sub = json_object();
    json_t *kwargs, *out = NULL;
    int has_reply = !jd_is_none(reply);

    json_set(sub, "metadata", json_copy(md ? md : json_null()));
    json_set(sub, "thread_id", json_copy(jd_get(req, "thread_id") ? jd_get(req, "thread_id") : json_null()));
    if (mode) json_set(sub, "reply_to_mode", json_string(mode));
    if (has_reply) json_set(sub, "reply_to_message_id", json_copy(reply));
    kwargs = w27_adapter_a96ad6_p3_thread_kwargs_for_send(sub);
    json_free(sub);
    if (private_send && !has_reply && !anchor_off && !jd_truthy(jd_get(kwargs, "direct_messages_topic_id"))) {
        json_free(kwargs);
        json_free((json_t *)routing);
        return json_null();
    }
    out = json_array();
    json_append(out, has_reply ? json_copy(reply) : json_null());
    json_append(out, kwargs);
    json_free((json_t *)routing);
    return out;
}

/* PoP: w27_adapter_a96ad6_p7_send_retry_is_final @ adapter.py:TelegramAdapter._send_retry_is_final */
json_t *w27_adapter_a96ad6_p7_send_retry_is_final(const json_t *req) {
    const json_t *raw = jd_get(req, "raw_response");
    if (!raw || raw->type != JSON_OBJECT) return jd_bool(0);
    return jd_bool(jd_truthy(jd_get(raw, "delivery_uncertain")));
}

/* ------------------------------------------------------------------ */
/* Gate p8                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p8_bot_commands @ adapter.py:TelegramAdapter._bot_commands */
json_t *w27_adapter_a96ad6_p8_bot_commands(const json_t *req) {
    const json_t *cmds = jd_get(req, "menu_commands");
    size_t i, n = json_len(cmds);
    json_t *out = json_array();
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(cmds, i);
        const json_t *name = json_get(row, 0);
        const json_t *desc = json_len(row) > 1 ? json_get(row, 1) : NULL;
        char *desc_str;
        char *trimmed;
        json_t *pair = json_array();
        if (jd_truthy(desc)) desc_str = jd_str_of(desc);
        else desc_str = jd_dup("");
        trimmed = jd_cp_prefix(desc_str, 256);
        json_append(pair, json_copy(name ? name : json_null()));
        json_append(pair, json_string(trimmed));
        json_append(out, pair);
        free(desc_str); free(trimmed);
    }
    return out;
}

/* PoP: w27_adapter_a96ad6_p8_is_id_like @ adapter.py:TelegramAdapter._is_id_like */
json_t *w27_adapter_a96ad6_p8_is_id_like(const json_t *req) {
    return jd_bool(jd_is_id_like(jd_get(req, "value")));
}

/* shared: reaction event payload */
static json_t *jd_reaction_event(const json_t *update) {
    const json_t *mr = jd_get(update, "message_reaction");
    const json_t *chat, *mid, *new_reaction;
    const char *chat_id_s = NULL;
    json_t *payload, *emojis, *customs, *out;
    int chat_id_ok, mid_ok;
    size_t i, n;
    if (jd_is_none(mr)) return NULL;
    chat = jd_get(mr, "chat");
    new_reaction = jd_get(mr, "new_reaction");
    if (!jd_truthy(new_reaction)) new_reaction = NULL;
    if (new_reaction && new_reaction->type != JSON_ARRAY) return NULL;
    {
        const json_t *cid = jd_get(chat, "id");
        chat_id_ok = jd_is_id_like(cid);
        if (chat_id_ok) chat_id_s = jd_text(cid);
    }
    mid = jd_get(mr, "message_id");
    mid_ok = jd_is_id_like(mid);
    if (!chat_id_ok || !mid_ok) return NULL;
    emojis = json_array();
    customs = json_array();
    n = new_reaction ? json_len(new_reaction) : 0;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(new_reaction, i);
        const json_t *emoji = jd_get(r, "emoji");
        const json_t *custom = jd_get(r, "custom_emoji_id");
        if (emoji && emoji->type == JSON_STRING && emoji->str_val && emoji->str_val[0]) {
            char *e = jd_cp_prefix(emoji->str_val, 64);
            json_append(emojis, json_string(e));
            free(e);
        }
        if (jd_is_id_like(custom)) {
            char *cs = jd_str_of(custom);
            char *cut = jd_cp_prefix(cs, 128);
            json_append(customs, json_string(cut));
            free(cs); free(cut);
        }
    }
    payload = json_object();
    json_set(payload, "emojis", emojis);
    json_set(payload, "custom_emoji_ids", customs);
    {
        char *cs = chat_id_s ? json_serialize(chat) : NULL;
        char *cid_text;
        if (chat_id_s) cid_text = jd_dup(chat_id_s);
        else {
            const json_t *cid = jd_get(chat, "id");
            cid_text = jd_str_of(cid);
        }
        {
            char *cut = jd_cp_prefix(cid_text, 128);
            json_set(payload, "chat_id", json_string(cut));
            free(cut);
        }
        free(cid_text);
        free(cs);
    }
    {
        char *ms = jd_str_of(mid);
        char *cut = jd_cp_prefix(ms, 128);
        json_set(payload, "message_id", json_string(cut));
        free(ms); free(cut);
    }
    json_set(payload, "thread_id", json_null());
    out = json_object();
    json_set(out, "platform", json_string("telegram"));
    json_set(out, "event_type", json_string("reaction"));
    json_set(out, "payload", payload);
    return out;
}

/* shared: message_edited event payload */
static json_t *jd_message_edited_event(const json_t *update) {
    const json_t *msg = jd_get(update, "edited_message");
    const json_t *chat, *mid, *text, *thread_raw, *edit_date;
    char *chat_text = NULL;
    json_t *payload, *out;
    char *text_out = NULL;
    char *thread_out = NULL;
    char *edited_at = NULL;
    if (jd_is_none(msg)) return NULL;
    chat = jd_get(msg, "chat");
    {
        const json_t *cid = jd_get(chat, "id");
        if (!jd_is_id_like(cid)) return NULL;
        chat_text = jd_str_of(cid);
    }
    mid = jd_get(msg, "message_id");
    if (!jd_is_id_like(mid)) {
        free(chat_text);
        return NULL;
    }
    text = jd_get(msg, "text");
    if (!jd_truthy(text)) text = jd_get(msg, "caption");
    if (text && text->type == JSON_STRING) text_out = jd_cp_prefix(text->str_val, 8192);
    thread_raw = jd_get(msg, "message_thread_id");
    if (jd_is_id_like(thread_raw) &&
        jd_truthy(jd_get(msg, "is_topic_message"))) {
        char *ts = jd_str_of(thread_raw);
        thread_out = jd_cp_prefix(ts, 128);
        free(ts);
    }
    edit_date = jd_get(msg, "edit_date");
    if (edit_date && edit_date->type == JSON_STRING) {
        edited_at = jd_cp_prefix(edit_date->str_val, 64);
    }
    payload = json_object();
    {
        char *cut = jd_cp_prefix(chat_text, 128);
        json_set(payload, "chat_id", json_string(cut));
        free(cut);
    }
    {
        char *ms = jd_str_of(mid);
        char *cut = jd_cp_prefix(ms, 128);
        json_set(payload, "message_id", json_string(cut));
        free(ms); free(cut);
    }
    json_set(payload, "thread_id", thread_out ? json_string(thread_out) : json_null());
    json_set(payload, "text", text_out ? json_string(text_out) : json_null());
    json_set(payload, "edited_at", edited_at ? json_string(edited_at) : json_null());
    out = json_object();
    json_set(out, "platform", json_string("telegram"));
    json_set(out, "event_type", json_string("message_edited"));
    json_set(out, "payload", payload);
    free(chat_text); free(text_out); free(thread_out); free(edited_at);
    return out;
}

/* PoP: w27_adapter_a96ad6_p8_normalize_reaction_event @ adapter.py:TelegramAdapter._normalize_reaction_event */
json_t *w27_adapter_a96ad6_p8_normalize_reaction_event(const json_t *req) {
    json_t *ev = jd_reaction_event(jd_get(req, "update"));
    return ev ? ev : json_null();
}

/* PoP: w27_adapter_a96ad6_p8_normalize_message_edited_event @ adapter.py:TelegramAdapter._normalize_message_edited_event */
json_t *w27_adapter_a96ad6_p8_normalize_message_edited_event(const json_t *req) {
    json_t *ev = jd_message_edited_event(jd_get(req, "update"));
    return ev ? ev : json_null();
}

/* PoP: w27_adapter_a96ad6_p8_normalize_platform_event @ adapter.py:TelegramAdapter._normalize_platform_event */
json_t *w27_adapter_a96ad6_p8_normalize_platform_event(const json_t *req) {
    const json_t *update = jd_get(req, "update");
    if (!jd_is_none(jd_get(update, "message_reaction"))) {
        json_t *ev = jd_reaction_event(update);
        return ev ? ev : json_null();
    }
    if (!jd_is_none(jd_get(update, "edited_message"))) {
        json_t *ev = jd_message_edited_event(update);
        return ev ? ev : json_null();
    }
    return json_null();
}

/* ------------------------------------------------------------------ */
/* Gate p9                                                            */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p9_rows_of_two @ adapter.py:TelegramAdapter._rows_of_two */
json_t *w27_adapter_a96ad6_p9_rows_of_two(const json_t *req) {
    const json_t *buttons = jd_get(req, "buttons");
    size_t i, n = json_len(buttons);
    json_t *rows = json_array();
    for (i = 0; i < n; i += 2) {
        json_t *row = json_array();
        json_append(row, json_copy(json_get(buttons, i)));
        if (i + 1 < n) json_append(row, json_copy(json_get(buttons, i + 1)));
        json_append(rows, row);
    }
    return rows;
}

/* PoP: w27_adapter_a96ad6_p9_ea_escape @ adapter.py:TelegramAdapter._ea_escape */
json_t *w27_adapter_a96ad6_p9_ea_escape(const json_t *req) {
    const char *text = jd_get_text(req, "text");
    char *esc = jd_html_escape(text);
    json_t *r = json_string(esc);
    free(esc);
    return r;
}

/* PoP: w27_adapter_a96ad6_p9_missing_media_path_error @ adapter.py:TelegramAdapter._missing_media_path_error */
json_t *w27_adapter_a96ad6_p9_missing_media_path_error(const json_t *req) {
    const char *label = jd_get_text(req, "label");
    const char *path = jd_get_text(req, "path");
    char *err;
    if (!label) label = "";
    if (!path) path = "";
    err = jd_fmt("%s file not found: %s", label, path);
    if (!strncmp(path, "/workspace/", 11) || !strncmp(path, "/output/", 8) ||
        !strncmp(path, "/outputs/", 9)) {
        size_t n = strlen(err);
        size_t extra = strlen(" (path may only exist inside the Docker sandbox. "
                              "Bind-mount a host directory and emit the host-visible path in MEDIA: "
                              "for gateway file delivery.)");
        char *grown = (char *)realloc(err, n + extra + 1);
        if (grown) {
            err = grown;
            memcpy(err + n,
                   " (path may only exist inside the Docker sandbox. "
                   "Bind-mount a host directory and emit the host-visible path in MEDIA: "
                   "for gateway file delivery.)",
                   extra + 1);
        }
    }
    {
        json_t *r = json_string(err);
        free(err);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p9_media_too_large_note @ adapter.py:TelegramAdapter._telegram_media_too_large_note */
json_t *w27_adapter_a96ad6_p9_media_too_large_note(const json_t *req) {
    const char *label = jd_get_text(req, "label");
    const json_t *size_v = jd_get(req, "file_size");
    const json_t *max_v = jd_get(req, "max_bytes");
    long long max_bytes = (max_v && max_v->type == JSON_NUMBER) ? (long long)max_v->num_val : 0;
    long long limit_mb = max_bytes / (1024 * 1024);
    char *size_text;
    char *err;
    json_t *r;
    if (limit_mb < 1) limit_mb = 1;
    if (!label) label = "";
    if (jd_is_none(size_v)) {
        size_text = jd_fmt("%.1f MB", 0.0);
    } else if (size_v->type == JSON_STRING) {
        char *s = jd_strip(size_v->str_val);
        char *q = s;
        int ok = 1;
        if (q && (*q == '+' || *q == '-')) q++;
        if (!q || !*q) ok = 0;
        else {
            for (; *q; q++)
                if (!isdigit((unsigned char)*q)) { ok = 0; break; }
        }
        if (ok) size_text = jd_fmt("%.1f MB", (double)strtol(s, NULL, 10) / (1024.0 * 1024.0));
        else size_text = jd_dup("unknown size");
        free(s);
    } else if (size_v && (size_v->type == JSON_NUMBER || size_v->type == JSON_BOOL)) {
        long long n = (size_v->type == JSON_BOOL) ? (size_v->bool_val ? 1 : 0) : (long long)size_v->num_val;
        size_text = jd_fmt("%.1f MB", (double)n / (1024.0 * 1024.0));
    } else {
        size_text = jd_dup("unknown size");
    }
    err = jd_fmt("[Telegram %s skipped: file size %s exceeds the %lld MB limit. "
                 "Ask the user to send a smaller file.]",
                 label, size_text, limit_mb);
    free(size_text);
    r = json_string(err);
    free(err);
    return r;
}

/* PoP: w27_adapter_a96ad6_p9_int_or_zero @ adapter.py:TelegramAdapter._int_or_zero */
json_t *w27_adapter_a96ad6_p9_int_or_zero(const json_t *req) {
    const json_t *v = jd_get(req, "value");
    if (!jd_truthy(v)) return json_number(0);
    return json_number((double)jd_int_or_zero(v));
}

/* PoP: w27_adapter_a96ad6_p9_media_size_allowed @ adapter.py:TelegramAdapter._telegram_media_size_allowed */
json_t *w27_adapter_a96ad6_p9_media_size_allowed(const json_t *req) {
    const json_t *source = jd_get(req, "source");
    const char *label = jd_get_text(req, "label");
    const json_t *max_v = jd_get(req, "max_doc_bytes");
    long long max_bytes = (max_v && max_v->type == JSON_NUMBER && max_v->num_val != 0)
                              ? (long long)max_v->num_val
                              : 20LL * 1024 * 1024;
    const json_t *fs = jd_get(source, "file_size");
    long long size = jd_truthy(fs) ? jd_int_or_zero(fs) : 0;
    json_t *out = json_array();
    if (size <= 0 || size <= max_bytes) {
        json_append(out, json_bool(1));
        json_append(out, json_null());
        return out;
    }
    json_append(out, json_bool(0));
    {
        json_t *sub = json_object();
        json_t *note;
        json_set(sub, "label", json_string(label ? label : ""));
        json_set(sub, "file_size", json_number((double)size));
        json_set(sub, "max_bytes", json_number((double)max_bytes));
        note = w27_adapter_a96ad6_p9_media_too_large_note(sub);
        json_append(out, note);
        json_free(sub);
    }
    return out;
}

/* PoP: w27_adapter_a96ad6_p9_caption_1024 @ adapter.py:TelegramAdapter._caption_1024 */
json_t *w27_adapter_a96ad6_p9_caption_1024(const json_t *req) {
    const json_t *caption = jd_get(req, "caption");
    char *cut;
    json_t *r;
    if (!jd_truthy(caption)) return json_null();
    cut = jd_cp_prefix(caption->type == JSON_STRING ? caption->str_val : "", 1024);
    r = json_string(cut);
    free(cut);
    return r;
}


/* ------------------------------------------------------------------ */
/* Gate p10                                                           */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p10_is_transient_typing_error @ adapter.py:TelegramAdapter._is_transient_typing_error */
json_t *w27_adapter_a96ad6_p10_is_transient_typing_error(const json_t *req) {
    const json_t *exc = jd_get(req, "error");
    const json_t *retry_after = jd_get(exc, "retry_after");
    const json_t *sc = jd_get(exc, "status_code");
    const json_t *code = jd_get(exc, "code");
    const char *text = jd_err_text(exc);
    if (!jd_is_none(retry_after)) return jd_bool(1);
    if (jd_is_none(sc) || !jd_truthy(sc)) sc = code;
    if (sc && sc->type == JSON_NUMBER) {
        double v = sc->num_val;
        if (v == (double)(long long)v && (v == 429 || v >= 500)) return jd_bool(1);
    }
    if (jd_contains_ci(text, "too many requests") || jd_contains_ci(text, "rate limit") ||
        jd_contains_ci(text, "timed out") || jd_contains_ci(text, "timeout") ||
        jd_contains_ci(text, "temporar"))
        return jd_bool(1);
    /* isinstance(exc, (OSError, TimeoutError, ConnectionError)) -> the flag,
     * or a class name from that family. */
    {
        const char *nm = jd_get_text(exc, "name");
        char *low = nm ? jd_strip_lower(nm) : NULL;
        int oserr = jd_truthy(jd_get(exc, "os_error"));
        if (!oserr && low) {
            oserr = !strcmp(low, "oserror") || !strcmp(low, "timeouterror") ||
                    !strcmp(low, "connectionerror") || !strcmp(low, "brokenpipeerror") ||
                    !strcmp(low, "connectionreseterror") || !strcmp(low, "connectionabortederror");
        }
        free(low);
        return jd_bool(oserr);
    }
}

/* extra_or_secret(extra, key, env_name, <sentinel>) -> the winning JSON value.
 * Returns NULL for the sentinel/None; *out_env is 1 when the env rung won. */
static json_t *jd_extra_or_secret(const json_t *extra, const char *key, const char *env_name,
                                  const json_t *env, int blank_is_unset, int *from_env) {
    if (from_env) *from_env = 0;
    if (env_name && env && env->type == JSON_OBJECT) {
        const json_t *ev = json_obj_get(env, env_name);
        if (!jd_is_none(ev)) {
            char *s = jd_str_of(ev);
            char *t = jd_strip(s);
            int nonblank = t && t[0];
            free(s); free(t);
            if (nonblank) {
                if (from_env) *from_env = 1;
                return (json_t *)ev;
            }
        }
    }
    {
        const json_t *v = (extra && extra->type == JSON_OBJECT && key) ? json_obj_get(extra, key) : NULL;
        if (jd_is_none(v)) return NULL;
        if (blank_is_unset && v->type == JSON_STRING) {
            char *t = jd_strip(v->str_val);
            int blank = !t || !t[0];
            free(t);
            if (blank) return NULL;
        }
        return (json_t *)v;
    }
}

/* PoP: w27_adapter_a96ad6_p10_extra_bool @ adapter.py:TelegramAdapter._extra_bool */
json_t *w27_adapter_a96ad6_p10_extra_bool(const json_t *req) {
    const json_t *extra = jd_get(req, "extra");
    const json_t *env = jd_get(req, "env");
    const char *key = jd_get_text(req, "key");
    const char *env_name = jd_get_text(req, "env_name");
    const json_t *dflt = jd_get(req, "default");
    const json_t *fallbacks = jd_get(req, "fallback_keys");
    json_t *configured = jd_extra_or_secret(extra, key, env_name, env, 1, NULL);
    size_t i, n = json_len(fallbacks);
    for (i = 0; i < n && !configured; i++) {
        const char *alt = jd_text(json_get(fallbacks, i));
        if (!alt) continue;
        configured = (json_t *)(extra && extra->type == JSON_OBJECT ? json_obj_get(extra, alt) : NULL);
    }
    if (!configured) configured = (json_t *)dflt;
    if (configured && configured->type == JSON_BOOL) return jd_bool(configured->bool_val);
    {
        char *s = jd_str_of(configured), *t;
        int result;
        t = jd_strip_lower(s);
        result = t && (!strcmp(t, "true") || !strcmp(t, "1") || !strcmp(t, "yes") || !strcmp(t, "on"));
        free(s); free(t);
        return jd_bool(result);
    }
}

/* str sorts of a set: the port returns the set as a sorted array (documented). */
static int jd_cmp_str(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return strcmp(*x, *y);
}

/* PoP: w27_adapter_a96ad6_p10_extra_str_set @ adapter.py:TelegramAdapter._extra_str_set */
json_t *w27_adapter_a96ad6_p10_extra_str_set(const json_t *req) {
    const json_t *extra = jd_get(req, "extra");
    const json_t *env = jd_get(req, "env");
    const char *key = jd_get_text(req, "key");
    const char *env_name = jd_get_text(req, "env_name");
    json_t *raw = jd_extra_or_secret(extra, key, env_name, env, 0, NULL);
    char **items = NULL;
    size_t count = 0, cap = 0;
    json_t *out;

#define W27_ADD(txt) do {                                                        \
        const char *w27_t = (txt);                                               \
        if (w27_t && w27_t[0]) {                                                 \
            if (count + 1 >= cap) {                                              \
                cap = cap ? cap * 2 : 8;                                         \
                items = (char **)realloc(items, cap * sizeof(char *));           \
            }                                                                    \
            items[count++] = jd_dup(w27_t);                                      \
        }                                                                        \
    } while (0)

    if (!raw) raw = json_string("");

    if (raw->type == JSON_ARRAY) {
        size_t i, n = json_len(raw);
        for (i = 0; i < n; i++) {
            char *s = jd_str_of(json_get(raw, i));
            char *t = jd_strip(s);
            W27_ADD(t);
            free(s); free(t);
        }
    } else {
        int done = 0;
        if (raw->type == JSON_STRING && raw->str_val) {
            const char *p = raw->str_val;
            while (*p && jd_is_py_space((unsigned char)*p)) p++;
            if (*p == '[') {
                json_t *parsed = json_parse(raw->str_val, NULL);
                if (parsed && parsed->type == JSON_ARRAY) {
                    size_t i, n = json_len(parsed);
                    for (i = 0; i < n; i++) {
                        char *s = jd_str_of(json_get(parsed, i));
                        char *t = jd_strip(s);
                        W27_ADD(t);
                        free(s); free(t);
                    }
                    done = 1;
                }
                json_free(parsed);
            }
        }
        if (!done) {
            char *s = jd_str_of(raw);
            char *copy = jd_dup(s);
            char *tok = copy;
            while (tok) {
                char *comma = strchr(tok, ',');
                char *t;
                if (comma) *comma = '\0';
                t = jd_strip(tok);
                W27_ADD(t);
                free(t);
                tok = comma ? comma + 1 : NULL;
            }
            free(copy);
            free(s);
        }
    }
#undef W27_ADD
    /* de-duplicate, like set() */
    {
        size_t i, w = 0;
        for (i = 0; i < count; i++) {
            size_t j;
            int dup = 0;
            for (j = 0; j < w; j++)
                if (!strcmp(items[j], items[i])) { dup = 1; break; }
            if (dup) free(items[i]);
            else items[w++] = items[i];
        }
        count = w;
    }
    if (count > 1) qsort(items, count, sizeof(char *), jd_cmp_str);
    out = json_array();
    {
        size_t i;
        for (i = 0; i < count; i++) {
            json_append(out, json_string(items[i]));
            free(items[i]);
        }
    }
    free(items);
    return out;
}

/* PoP: w27_adapter_a96ad6_p10_chat_type_str @ adapter.py:TelegramAdapter._chat_type_str */
json_t *w27_adapter_a96ad6_p10_chat_type_str(const json_t *req) {
    const json_t *chat = jd_get(req, "chat");
    char *full, *res;
    if (jd_is_none(chat) || !jd_truthy(chat)) return json_string("");
    full = jd_str_of(jd_get(chat, "type"));
    {
        char *dot = strrchr(full, '.');
        res = jd_dup(dot ? dot + 1 : full);
    }
    free(full);
    {
        char *low = jd_lower_dup(res);
        json_t *r;
        free(res);
        r = json_string(low ? low : "");
        free(low);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p10_chat_id_str @ adapter.py:TelegramAdapter._chat_id_str */
json_t *w27_adapter_a96ad6_p10_chat_id_str(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *chat = jd_get(msg, "chat");
    char *s;
    json_t *r;
    if (jd_is_none(chat)) return json_string("");
    /* str(getattr(getattr(msg, "chat", None), "id", "")): the DEFAULT applies to
     * a missing key (-> ""), while a key that is present and None stringifies to
     * "None". Treating both as missing is the bug the fuzz found, in one
     * direction; treating both as "None" is the same bug in the other. */
    if (!jd_get(chat, "id")) return json_string("");
    s = jd_str_of(jd_get(chat, "id"));
    if (!s) s = jd_dup("");
    r = json_string(s);
    free(s);
    return r;
}

/* PoP: w27_adapter_a96ad6_p10_topic_id_or_general @ adapter.py:TelegramAdapter._topic_id_or_general */
json_t *w27_adapter_a96ad6_p10_topic_id_or_general(const json_t *req) {
    const json_t *tid = jd_get(req, "thread_id");
    if (jd_is_none(tid)) return json_string(W27_GENERAL_TOPIC_THREAD_ID);
    {
        char *s = jd_str_of(tid);
        json_t *r = json_string(s);
        free(s);
        return r;
    }
}

/* ------------------------------------------------------------------ */
/* Gate p11                                                           */
/* ------------------------------------------------------------------ */

/* message.chat.type split on "." + lower, no strip (as _chat_type_str). */
static char *jd_chat_type_of(const json_t *chat) {
    if (jd_is_none(chat) || !jd_truthy(chat)) return jd_dup("");
    {
        char *full = jd_str_of(jd_get(chat, "type"));
        char *dot = strrchr(full, '.');
        char *cut = jd_dup(dot ? dot + 1 : full);
        char *low = jd_lower_dup(cut);
        free(full); free(cut);
        return low ? low : jd_dup("");
    }
}

/* PoP: w27_adapter_a96ad6_p11_is_group_chat @ adapter.py:TelegramAdapter._is_group_chat */
json_t *w27_adapter_a96ad6_p11_is_group_chat(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *chat = jd_get(msg, "chat");
    char *ct;
    int hit;
    if (jd_is_none(chat) || !jd_truthy(chat)) return jd_bool(0);
    ct = jd_chat_type_of(chat);
    hit = !strcmp(ct, "group") || !strcmp(ct, "supergroup") || !strcmp(ct, "channel");
    free(ct);
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p11_effective_message_thread_id @ adapter.py:TelegramAdapter._effective_message_thread_id */
json_t *w27_adapter_a96ad6_p11_effective_message_thread_id(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *chat = jd_get(msg, "chat");
    const json_t *raw = jd_get(msg, "message_thread_id");
    char *ct = jd_chat_type_of(chat);
    int is_group = !strcmp(ct, "group") || !strcmp(ct, "supergroup");
    int is_forum_group = is_group && jd_get(chat, "is_forum") && jd_get(chat, "is_forum")->type == JSON_BOOL &&
                         jd_get(chat, "is_forum")->bool_val;
    int is_topic = jd_truthy(jd_get(msg, "is_topic_message"));
    json_t *out = NULL;
    if (!jd_is_none(raw)) {
        int routable = is_forum_group || (is_group && is_topic) ||
                       (!strcmp(ct, "private") && is_topic);
        if (routable) {
            char *s = jd_str_of(raw);
            out = json_string(s);
            free(s);
        } else {
            out = json_null();
        }
    } else if (is_forum_group) {
        out = json_string(W27_GENERAL_TOPIC_THREAD_ID);
    } else {
        out = json_null();
    }
    free(ct);
    return out;
}

/* PoP: w27_adapter_a96ad6_p11_is_reply_to_bot @ adapter.py:TelegramAdapter._is_reply_to_bot */
json_t *w27_adapter_a96ad6_p11_is_reply_to_bot(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *bot = jd_get(req, "bot");
    const json_t *replied = jd_get(msg, "reply_to_message");
    const json_t *thread_id = jd_get(msg, "message_thread_id");
    if (jd_is_none(bot)) return jd_bool(0);
    if (jd_is_none(replied) || !jd_truthy(replied)) return jd_bool(0);
    if (!jd_is_none(jd_get(replied, "forum_topic_created")) && !jd_is_none(thread_id)) {
        const json_t *rmid = jd_get(replied, "message_id");
        int same = 0;
        if (rmid && thread_id) {
            if (rmid->type == JSON_NUMBER && thread_id->type == JSON_NUMBER)
                same = rmid->num_val == thread_id->num_val;
            else if (rmid->type == JSON_STRING && thread_id->type == JSON_STRING)
                same = !strcmp(rmid->str_val, thread_id->str_val);
        }
        if (same) return jd_bool(0);
    }
    {
        const json_t *ru = jd_get(replied, "from_user");
        const json_t *bid = jd_get(bot, "id");
        if (jd_is_none(ru) || !jd_truthy(ru) || jd_is_none(bid)) return jd_bool(0);
        {
            const json_t *uid = jd_get(ru, "id");
            if (jd_is_none(uid)) return jd_bool(0);
            if (uid->type == JSON_NUMBER && bid->type == JSON_NUMBER) return jd_bool(uid->num_val == bid->num_val);
            if (uid->type == JSON_STRING && bid->type == JSON_STRING) return jd_bool(!strcmp(uid->str_val, bid->str_val));
            return jd_bool(0);
        }
    }
}

/* PoP: w27_adapter_a96ad6_p11_entity_type @ adapter.py:TelegramAdapter._entity_type */
json_t *w27_adapter_a96ad6_p11_entity_type(const json_t *req) {
    const json_t *entity = jd_get(req, "entity");
    char *full, *cut, *low;
    json_t *r;
    if (jd_is_none(entity)) return json_string("");
    full = jd_str_of(jd_get(entity, "type"));
    {
        char *dot = strrchr(full, '.');
        cut = jd_dup(dot ? dot + 1 : full);
    }
    free(full);
    low = jd_strip_lower(cut);
    free(cut);
    r = json_string(low ? low : "");
    free(low);
    return r;
}

/* PoP: w27_adapter_a96ad6_p11_entity_span @ adapter.py:TelegramAdapter._entity_span */
json_t *w27_adapter_a96ad6_p11_entity_span(const json_t *req) {
    const char *src = jd_get_text(req, "source_text");
    const json_t *entity = jd_get(req, "entity");
    long offset = -1, length = 0;
    const json_t *off = jd_get(entity, "offset");
    const json_t *len = jd_get(entity, "length");
    if (!jd_is_none(off)) offset = jd_int_or_zero(off);
    if (!jd_is_none(len)) length = jd_int_or_zero(len);
    if (offset < 0 || length <= 0) return json_null();
    {
        char *s = jd_utf16_slice(src ? src : "", offset, length);
        json_t *r = json_string(s);
        free(s);
        return r;
    }
}

/* PoP: w27_adapter_a96ad6_p11_telegram_entity_text @ adapter.py:TelegramAdapter._telegram_entity_text */
json_t *w27_adapter_a96ad6_p11_telegram_entity_text(const json_t *req) {
    const char *src = jd_get_text(req, "source_text");
    long offset = jd_int_or_zero(jd_get(req, "offset"));
    long length = jd_int_or_zero(jd_get(req, "length"));
    char *s;
    json_t *r;
    if (offset < 0 || length <= 0) return json_string("");
    s = jd_utf16_slice(src ? src : "", offset, length);
    r = json_string(s);
    free(s);
    return r;
}

/* _FOREIGN_BOT_HANDLE_RE.fullmatch("[a-z0-9_]{2,29}bot", IGNORECASE) */
static int jd_is_bot_handle(const char *h) {
    size_t n, i;
    if (!h) return 0;
    n = strlen(h);
    if (n < 5 || n > 32) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)h[i];
        if (!(isalnum(c) || c == '_')) return 0;
    }
    return tolower((unsigned char)h[n - 3]) == 'b' && tolower((unsigned char)h[n - 2]) == 'o' &&
           tolower((unsigned char)h[n - 1]) == 't';
}

static int jd_is_word_char(unsigned char c) {
    return isalnum(c) || c == '_';
}

/* One (text, entities) source's mention/bot_command contribution. */
static void jd_collect_source_handles(const json_t *src_text, const json_t *entities,
                                      const char *own, char ***handles, size_t *count) {
    size_t i, n = json_len(entities);
    const char *text = jd_text(src_text);
    if (!text) text = "";
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(entities, i);
        char *etype = NULL, *span = NULL;
        const json_t *et = jd_get(e, "type");
        if (!jd_is_none(et)) {
            char *full = jd_str_of(et);
            char *dot = strrchr(full, '.');
            char *cut = jd_dup(dot ? dot + 1 : full);
            etype = jd_strip_lower(cut);
            free(full); free(cut);
        }
        if (!etype) continue;
        if (strcmp(etype, "mention") != 0 && strcmp(etype, "bot_command") != 0) { free(etype); continue; }
        {
            long offset = jd_int_or_zero(jd_get(e, "offset"));
            long length = jd_int_or_zero(jd_get(e, "length"));
            if (offset >= 0 && length > 0) span = jd_utf16_slice(text, offset, length);
            else span = jd_dup("");
        }
        if (!strcmp(etype, "mention")) {
            char *st = jd_strip(span);
            const char *p = st;
            char *handle, *low;
            while (*p == '@') p++;
            handle = jd_dup(p);
            low = jd_strip_lower(handle);
            if ((own && own[0] && !strcmp(low, own)) || jd_is_bot_handle(low)) {
                (*handles) = (char **)realloc(*handles, (*count + 1) * sizeof(char *));
                (*handles)[(*count)++] = jd_dup(low);
            }
            free(st); free(handle); free(low);
        } else {
            char *st = jd_strip(span);
            char *at = strchr(st, '@');
            if (at) {
                char *target = jd_strip(at + 1);
                char *low = jd_strip_lower(target);
                if ((own && own[0] && !strcmp(low, own)) || jd_is_bot_handle(low)) {
                    (*handles) = (char **)realloc(*handles, (*count + 1) * sizeof(char *));
                    (*handles)[(*count)++] = jd_dup(low);
                }
                free(target); free(low);
            }
            free(st);
        }
        free(etype); free(span);
    }
}

/* Raw-text fallback: (?i)(?<![A-Za-z0-9_`/])@([A-Za-z0-9_]{2,31})\b */
static void jd_collect_raw_handles(const char *text, const char *own, char ***handles, size_t *count) {
    size_t i = 0;
    if (!text) return;
    for (i = 0; text[i]; i++) {
        size_t s, e;
        if (text[i] != '@') continue;
        if (i > 0) {
            unsigned char prev = (unsigned char)text[i - 1];
            if (jd_is_word_char(prev) || prev == '`' || prev == '/') continue;
        }
        s = i + 1;
        e = s;
        while (text[e] && jd_is_word_char((unsigned char)text[e])) e++;
        if (e - s < 2 || e - s > 31) continue;
        {
            char *h = (char *)malloc(e - s + 1);
            char *low;
            memcpy(h, text + s, e - s);
            h[e - s] = '\0';
            low = jd_strip_lower(h);
            if ((own && own[0] && !strcmp(low, own)) || jd_is_bot_handle(low)) {
                (*handles) = (char **)realloc(*handles, (*count + 1) * sizeof(char *));
                (*handles)[(*count)++] = jd_dup(low);
            }
            free(h); free(low);
        }
    }
}

/* PoP: w27_adapter_a96ad6_p11_extract_bot_mention_usernames @ adapter.py:TelegramAdapter._extract_bot_mention_usernames */
json_t *w27_adapter_a96ad6_p11_extract_bot_mention_usernames(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const char *self_username = jd_get_text(req, "self_username");
    const char *text = jd_get_text(msg, "text");
    const char *caption = jd_get_text(msg, "caption");
    const json_t *entities = jd_get(msg, "entities");
    const json_t *caption_entities = jd_get(msg, "caption_entities");
    char *own = NULL;
    char **handles = NULL;
    size_t count = 0;
    json_t *out;
    if (self_username) {
        const char *p = self_username;
        while (*p == '@') p++;
        own = jd_strip_lower(p);
    }
    if (!text) text = "";
    if (!caption) caption = "";
    if (text[0]) {
        json_t *src = json_string(text);
        jd_collect_source_handles(src, entities, own, &handles, &count);
        json_free(src);
    }
    if (caption[0]) {
        json_t *src = json_string(caption);
        jd_collect_source_handles(src, caption_entities, own, &handles, &count);
        json_free(src);
    }
    if (text[0] && !json_len(entities)) jd_collect_raw_handles(text, own, &handles, &count);
    if (caption[0] && !json_len(caption_entities)) jd_collect_raw_handles(caption, own, &handles, &count);
    {
        size_t i, w = 0;
        for (i = 0; i < count; i++) {
            size_t j;
            int dup = 0;
            for (j = 0; j < w; j++)
                if (!strcmp(handles[j], handles[i])) { dup = 1; break; }
            if (dup) free(handles[i]);
            else handles[w++] = handles[i];
        }
        count = w;
    }
    if (count > 1) qsort(handles, count, sizeof(char *), jd_cmp_str);
    out = json_array();
    {
        size_t i;
        for (i = 0; i < count; i++) {
            json_append(out, json_string(handles[i]));
            free(handles[i]);
        }
    }
    free(handles);
    free(own);
    return out;
}

/* ------------------------------------------------------------------ */
/* Gate p12                                                           */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p12_message_mentions_bot @ adapter.py:TelegramAdapter._message_mentions_bot */
json_t *w27_adapter_a96ad6_p12_message_mentions_bot(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *bot = jd_get(req, "bot");
    const char *username_raw = jd_get_text(req, "current_username");
    char *username = NULL;
    const char *text = jd_get_text(msg, "text");
    const char *caption = jd_get_text(msg, "caption");
    const json_t *entities = jd_get(msg, "entities");
    const json_t *caption_entities = jd_get(msg, "caption_entities");
    const json_t *bot_id = jd_get(bot, "id");
    char *expected = NULL;
    int hit = 0, counted = 0;

    if (username_raw && username_raw[0]) {
        const char *p = username_raw;
        while (*p == '@') p++;
        username = jd_lower_dup(p);   /* _current_bot_username() lowercases */
    }
    if (jd_is_none(bot)) { free(username); return jd_bool(0); }
    if (username && username[0]) expected = jd_fmt("@%s", username);
    if (!text) text = "";
    if (!caption) caption = "";

    {
        int pass;
        for (pass = 0; pass < 2 && !hit; pass++) {
            const json_t *srcv = pass == 0 ? jd_get(msg, "text") : jd_get(msg, "caption");
            const json_t *ents = pass == 0 ? entities : caption_entities;
            const char *src = pass == 0 ? text : caption;
            size_t i, n = json_len(ents);
            (void)srcv;
            for (i = 0; i < n && !hit; i++) {
                const json_t *e = json_get(ents, i);
                const json_t *et = jd_get(e, "type");
                char *etype = NULL;
                if (jd_is_none(et)) continue;
                {
                    char *full = jd_str_of(et);
                    char *dot = strrchr(full, '.');
                    char *cut = jd_dup(dot ? dot + 1 : full);
                    etype = jd_strip_lower(cut);
                    free(full); free(cut);
                }
                if (!etype) continue;
                if (!strcmp(etype, "mention") && expected) {
                    long offset = jd_int_or_zero(jd_get(e, "offset"));
                    long length = jd_int_or_zero(jd_get(e, "length"));
                    if (offset >= 0 && length > 0) {
                        char *span = jd_utf16_slice(src, offset, length);
                        char *st = jd_strip(span);
                        char *low = jd_strip_lower(st);
                        if (low && !strcmp(low, expected)) hit = 1;
                        free(span); free(st); free(low);
                    }
                } else if (!strcmp(etype, "text_mention")) {
                    const json_t *user = jd_get(e, "user");
                    const json_t *uid = jd_get(user, "id");
                    if (!jd_is_none(user) && jd_truthy(user) && !jd_is_none(uid) && !jd_is_none(bot_id)) {
                        if (uid->type == JSON_NUMBER && bot_id->type == JSON_NUMBER && uid->num_val == bot_id->num_val)
                            hit = 1;
                        else if (uid->type == JSON_STRING && bot_id->type == JSON_STRING &&
                                 !strcmp(uid->str_val, bot_id->str_val))
                            hit = 1;
                    }
                } else if (!strcmp(etype, "bot_command") && expected) {
                    long offset = jd_int_or_zero(jd_get(e, "offset"));
                    long length = jd_int_or_zero(jd_get(e, "length"));
                    if (offset >= 0 && length > 0) {
                        char *cmd = jd_utf16_slice(src, offset, length);
                        char *at = strchr(cmd, '@');
                        if (at) {
                            char *st = jd_strip(at);
                            char *low = jd_strip_lower(st);
                            if (low && !strcmp(low, expected)) hit = 1;
                            free(st); free(low);
                        }
                        free(cmd);
                    }
                }
                free(etype);
            }
        }
    }
    if (!hit && username && username[0]) {
        /* bot_username in _extract_bot_mention_usernames(message, bot_username) */
        json_t *sub = json_object();
        size_t i;
        char *own = jd_strip_lower(username);
        json_t *h;
        json_set(sub, "message", json_copy(msg ? msg : json_null()));
        json_set(sub, "self_username", json_string(username));
        h = w27_adapter_a96ad6_p11_extract_bot_mention_usernames(sub);
        json_free(sub);
        counted = (int)json_len(h);
        for (i = 0; i < json_len(h); i++) {
            if (jd_text(json_get(h, i)) && !strcmp(jd_text(json_get(h, i)), own)) hit = 1;
        }
        json_free(h);
        free(own);
    }
    free(expected);
    free(username);
    (void)counted;
    return jd_bool(hit);
}

/* PoP: w27_adapter_a96ad6_p12_sender_is_other_bot @ adapter.py:TelegramAdapter._sender_is_other_bot */
json_t *w27_adapter_a96ad6_p12_sender_is_other_bot(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *bot = jd_get(req, "bot");
    const json_t *sender = jd_get(msg, "from_user");
    const json_t *bot_id = jd_get(bot, "id");
    const json_t *sid;
    if (jd_is_none(sender) || !jd_truthy(jd_get(sender, "is_bot"))) return jd_bool(0);
    sid = jd_get(sender, "id");
    if (jd_is_none(bot_id) || jd_is_none(sid)) return jd_bool(1);
    if (bot_id->type == JSON_NUMBER && sid->type == JSON_NUMBER) return jd_bool(bot_id->num_val != sid->num_val);
    if (bot_id->type == JSON_STRING && sid->type == JSON_STRING) return jd_bool(strcmp(bot_id->str_val, sid->str_val) != 0);
    return jd_bool(1);
}

/* PoP: w27_adapter_a96ad6_p12_bot_sender_suppressed @ adapter.py:TelegramAdapter._bot_sender_suppressed */
json_t *w27_adapter_a96ad6_p12_bot_sender_suppressed(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    int require = jd_truthy(jd_get(req, "bots_require_mention"));
    json_t *sub;
    int other, mentions;
    if (!require) return jd_bool(0);
    sub = json_object();
    json_set(sub, "message", json_copy(msg ? msg : json_null()));
    json_set(sub, "bot", json_copy(jd_get(req, "bot") ? jd_get(req, "bot") : json_null()));
    other = jd_truthy(w27_adapter_a96ad6_p12_sender_is_other_bot(sub));
    if (!other) { json_free(sub); return jd_bool(0); }
    json_set(sub, "current_username", json_copy(jd_get(req, "current_username") ?
                                                        jd_get(req, "current_username") : json_null()));
    mentions = jd_truthy(w27_adapter_a96ad6_p12_message_mentions_bot(sub));
    json_free(sub);
    return jd_bool(!mentions);
}

/* PoP: w27_adapter_a96ad6_p12_media_message_type @ adapter.py:TelegramAdapter._media_message_type */
json_t *w27_adapter_a96ad6_p12_media_message_type(const json_t *req) {
    static const char *const ATTRS[6] = {"sticker", "photo", "video", "video_note", "audio", "voice"};
    static const char *const TYPES[6] = {"sticker", "photo", "video", "video", "audio", "voice"};
    const json_t *msg = jd_get(req, "msg");
    size_t i;
    for (i = 0; i < 6; i++)
        if (jd_truthy(jd_get(msg, ATTRS[i]))) return json_string(TYPES[i]);
    return json_string("document");
}

/* PoP: w27_adapter_a96ad6_p12_append_observed_note @ adapter.py:TelegramAdapter._append_observed_note */
json_t *w27_adapter_a96ad6_p12_append_observed_note(const json_t *req) {
    const json_t *existing = jd_get(req, "existing");
    const json_t *note = jd_get(req, "note");
    char *out;
    json_t *r;
    if (!jd_truthy(note)) {
        char *s = jd_str_of(existing);
        r = json_string(jd_truthy(existing) ? s : "");
        free(s);
        return r;
    }
    if (jd_truthy(existing)) {
        char *es = jd_str_of(existing);
        char *ns = jd_str_of(note);
        out = jd_fmt("%s\n\n%s", es, ns);
        free(es); free(ns);
    } else {
        out = jd_str_of(note);
    }
    r = json_string(out);
    free(out);
    return r;
}

/* PoP: w27_adapter_a96ad6_p12_is_own_message @ adapter.py:TelegramAdapter._is_own_message */
json_t *w27_adapter_a96ad6_p12_is_own_message(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *bot = jd_get(req, "bot");
    const json_t *from_user = jd_get(msg, "from_user");
    const json_t *bot_id = jd_get(bot, "id");
    const json_t *uid;
    if (jd_is_none(bot)) return jd_bool(0);
    if (jd_is_none(from_user) || !jd_truthy(from_user)) return jd_bool(0);
    uid = jd_get(from_user, "id");
    if (jd_is_none(bot_id) || jd_is_none(uid)) return jd_bool(0);
    if (bot_id->type == JSON_NUMBER && uid->type == JSON_NUMBER) return jd_bool(bot_id->num_val == uid->num_val);
    if (bot_id->type == JSON_STRING && uid->type == JSON_STRING) return jd_bool(!strcmp(bot_id->str_val, uid->str_val));
    return jd_bool(0);
}

/* ------------------------------------------------------------------ */
/* Gate p13                                                           */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_a96ad6_p13_text_batch_delay_for @ adapter.py:TelegramAdapter._text_batch_delay_for */
json_t *w27_adapter_a96ad6_p13_text_batch_delay_for(const json_t *req) {
    const json_t *pending = jd_get(req, "pending");
    const json_t *last_len_v = jd_get(pending, "_last_chunk_len");
    const json_t *text_v = jd_get(pending, "text");
    long last_len = (last_len_v && last_len_v->type == JSON_NUMBER) ? (long)last_len_v->num_val : 0;
    size_t total_len = jd_cp_len(jd_truthy(text_v) ? jd_text(text_v) : "");
    double split_threshold = json_get_num(req, "split_threshold", 4000.0);
    double fast_len = json_get_num(req, "fast_len", 320.0);
    double fast_delay = json_get_num(req, "fast_delay", 0.18);
    double short_len = json_get_num(req, "short_len", 1024.0);
    double short_delay = json_get_num(req, "short_delay", 0.24);
    double delay = json_get_num(req, "delay", 1.5);
    double split_delay = json_get_num(req, "split_delay", 2.0);
    if ((double)last_len >= split_threshold) return json_number(split_delay);
    if ((double)total_len <= fast_len) return json_number(delay < fast_delay ? delay : fast_delay);
    if ((double)total_len <= short_len) return json_number(delay < short_delay ? delay : short_delay);
    return json_number(delay);
}

/* PoP: w27_adapter_a96ad6_p13_ext_from_path @ adapter.py:TelegramAdapter._ext_from_path */
json_t *w27_adapter_a96ad6_p13_ext_from_path(const json_t *req) {
    const char *file_path = jd_get_text(req, "file_path");
    const json_t *candidates = jd_get(req, "candidates");
    const char *dflt = jd_get_text(req, "default");
    size_t i, n = json_len(candidates);
    if (jd_truthy(jd_get(req, "file_path")) && file_path) {
        char *low = jd_strip_lower(file_path);
        for (i = 0; i < n; i++) {
            const char *c = jd_text(json_get(candidates, i));
            size_t cl, ll;
            if (!c) continue;
            cl = strlen(c);
            ll = strlen(low);
            if (cl && ll >= cl && !strcmp(low + ll - cl, c)) {
                json_t *r = json_string(c);
                free(low);
                return r;
            }
        }
        free(low);
    }
    return json_string(dflt ? dflt : "");
}

/* PoP: w27_adapter_a96ad6_p13_get_dm_topic_info @ adapter.py:TelegramAdapter._get_dm_topic_info */
json_t *w27_adapter_a96ad6_p13_get_dm_topic_info(const json_t *req) {
    const char *chat_id = jd_get_text(req, "chat_id");
    const json_t *thread_v = jd_get(req, "thread_id");
    const json_t *topics = jd_get(req, "dm_topics");
    const json_t *config = jd_get(req, "dm_topics_config");
    long thread_int;
    size_t i, n;

    if (!jd_truthy(thread_v)) return json_null();
    {
        char *s = jd_str_of(thread_v);
        char *t = jd_strip(s);
        thread_int = t ? strtol(t, NULL, 10) : 0;
        free(s); free(t);
    }
    if (!chat_id) chat_id = "";
    n = json_object_size(topics);
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(topics, i);
        const json_t *cached = json_object_get_at(topics, i);
        char *prefix;
        if (!key || !cached || cached->type != JSON_NUMBER) continue;
        if ((long)cached->num_val != thread_int) continue;
        prefix = jd_fmt("%s:", chat_id);
        if (strncmp(key, prefix, strlen(prefix)) != 0) { free(prefix); continue; }
        free(prefix);
        {
            const char *colon = strchr(key, ':');
            const char *topic_name = colon ? colon + 1 : "";
            size_t ci, cn = json_len(config);
            for (ci = 0; ci < cn; ci++) {
                const json_t *entry = json_get(config, ci);
                const json_t *eid = jd_get(entry, "chat_id");
                char *eid_s = jd_str_of(eid);
                int match = eid_s && !strcmp(eid_s, chat_id);
                free(eid_s);
                if (!match) continue;
                {
                    const json_t *tl = jd_get(entry, "topics");
                    size_t ti, tn = json_len(tl);
                    for (ti = 0; ti < tn; ti++) {
                        const json_t *t = json_get(tl, ti);
                        const char *tname = jd_get_text(t, "name");
                        if (tname && !strcmp(tname, topic_name)) return json_copy(t);
                    }
                }
            }
            {
                json_t *fallback = json_object();
                json_set(fallback, "name", json_string(topic_name));
                return fallback;
            }
        }
    }
    /* cache miss: _reload_dm_topics_from_config() is a seam (the config is
     * supplied by the caller already), so the second lookup is identical. */
    return json_null();
}

/* PoP: w27_adapter_a96ad6_p13_resolve_topic_binding @ adapter.py:TelegramAdapter._resolve_topic_binding */
json_t *w27_adapter_a96ad6_p13_resolve_topic_binding(const json_t *req) {
    const json_t *msg = jd_get(req, "message");
    const json_t *chat = jd_get(msg, "chat");
    const char *chat_type = jd_get_text(req, "chat_type");
    const json_t *thread_v = jd_get(req, "thread_id");
    const char *thread_id = jd_text(thread_v);
    const json_t *gt = jd_get(req, "group_topics");
    json_t *chat_topic = NULL, *topic_skill = NULL;
    json_t *out;
    char *chat_id_s = jd_str_of(jd_get(chat, "id"));

    if (!chat_type) chat_type = "";
    if (!strcmp(chat_type, "dm") && thread_id && thread_id[0]) {
        json_t *sub = json_object();
        json_t *info;
        json_set(sub, "chat_id", json_string(chat_id_s));
        json_set(sub, "thread_id", json_copy(thread_v));
        json_set(sub, "dm_topics", json_copy(jd_get(req, "dm_topics") ? jd_get(req, "dm_topics") : json_null()));
        json_set(sub, "dm_topics_config",
                 json_copy(jd_get(req, "dm_topics_config") ? jd_get(req, "dm_topics_config") : json_null()));
        info = w27_adapter_a96ad6_p13_get_dm_topic_info(sub);
        json_free(sub);
        if (!jd_is_none(info) && jd_truthy(info)) {
            chat_topic = json_copy(jd_get(info, "name"));
            topic_skill = json_copy(jd_get(info, "skill"));
        }
        json_free(info);
        {
            const json_t *created = jd_get(msg, "forum_topic_created");
            const char *created_name = jd_get_text(created, "name");
            if (!jd_is_none(created) && created_name && created_name[0] && jd_is_none(chat_topic))
                chat_topic = json_string(created_name);
        }
    } else if (!strcmp(chat_type, "group") && thread_id && thread_id[0]) {
        size_t i = 0, n;
        /* dict -> [{chat_id, topics}]; list -> the dict entries */
        if (gt && gt->type == JSON_OBJECT) {
            n = json_object_size(gt);
            for (i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(gt, i);
                const json_t *topics = json_object_get_at(gt, i);
                char *key_s = jd_dup(k);
                size_t ti, tn;
                if (strcmp(key_s, chat_id_s) != 0) { free(key_s); continue; }
                tn = json_len(topics);
                for (ti = 0; ti < tn; ti++) {
                    const json_t *topic = json_get(topics, ti);
                    const json_t *tid = jd_get(topic, "thread_id");
                    if (jd_is_none(tid)) continue;
                    {
                        char *ts = jd_str_of(tid);
                        int match = !strcmp(ts, thread_id);
                        free(ts);
                        if (match) {
                            chat_topic = json_copy(jd_get(topic, "name"));
                            topic_skill = json_copy(jd_get(topic, "skill"));
                            free(key_s);
                            goto done;
                        }
                    }
                }
                free(key_s);
                break;
            }
        } else if (gt && gt->type == JSON_ARRAY) {
            n = json_len(gt);
            for (i = 0; i < n; i++) {
                const json_t *entry = json_get(gt, i);
                const json_t *eid;
                char *eid_s;
                size_t ti, tn;
                const json_t *topics;
                if (!entry || entry->type != JSON_OBJECT) continue;
                eid = jd_get(entry, "chat_id");
                eid_s = jd_str_of(eid);
                if (!eid_s || strcmp(eid_s, chat_id_s) != 0) { free(eid_s); continue; }
                free(eid_s);
                topics = jd_get(entry, "topics");
                tn = json_len(topics);
                for (ti = 0; ti < tn; ti++) {
                    const json_t *topic = json_get(topics, ti);
                    const json_t *tid;
                    if (!topic || topic->type != JSON_OBJECT) continue;
                    tid = jd_get(topic, "thread_id");
                    if (jd_is_none(tid)) continue;
                    {
                        char *ts = jd_str_of(tid);
                        int match = !strcmp(ts, thread_id);
                        free(ts);
                        if (match) {
                            chat_topic = json_copy(jd_get(topic, "name"));
                            topic_skill = json_copy(jd_get(topic, "skill"));
                            goto done;
                        }
                    }
                }
                break;
            }
        }
    }
done:
    out = json_array();
    json_append(out, chat_topic ? chat_topic : json_null());
    json_append(out, topic_skill ? topic_skill : json_null());
    free(chat_id_s);
    return out;
}

/* PoP: w27_adapter_a96ad6_p13_group_observe_channel_prompt @ adapter.py:TelegramAdapter._telegram_group_observe_channel_prompt */
json_t *w27_adapter_a96ad6_p13_group_observe_channel_prompt(const json_t *req) {
    const char *username = jd_get_text(req, "username");
    const json_t *bot_id = jd_get(req, "bot_id");
    char *id_s;
    char *out;
    json_t *r;
    if (!username || !username[0]) username = "unknown";
    if (!jd_truthy(bot_id)) id_s = jd_dup("unknown");
    else id_s = jd_str_of(bot_id);
    out = jd_fmt("You are handling a Telegram group chat message.\n"
                 "- Your identity: user_id=%s, @-mention name in this group=@%s\n"
                 "- observed Telegram group context may be provided in a separate context-only block "
                 "before the current message; it is not necessarily addressed to you.\n"
                 "- Treat only the current new message as a request explicitly directed at you, "
                 "and use observed context only when the current message asks for it.",
                 id_s, username);
    free(id_s);
    r = json_string(out);
    free(out);
    return r;
}

/* ------------------------------------------------------------------ */
/* Dispatchers                                                        */
/* ------------------------------------------------------------------ */

typedef json_t *(*w27_fn)(const json_t *);

typedef struct {
    const char *name;
    w27_fn fn;
} w27_entry;

#define W27_ENTRY(gate, fn) { #fn, w27_adapter_a96ad6_##gate##_##fn }

static const w27_entry W27_P1_FUNCS[] = {
    W27_ENTRY(p1, bold_label_html),
    W27_ENTRY(p1, flood_cap_result),
    W27_ENTRY(p1, env_float_clamped),
    W27_ENTRY(p1, notification_kwargs),
    W27_ENTRY(p1, normalize_chat_type),
    W27_ENTRY(p1, gateway_chat_type),
};

static const w27_entry W27_P2_FUNCS[] = {
    W27_ENTRY(p2, metadata_thread_id),
    W27_ENTRY(p2, metadata_direct_messages_topic_id),
    W27_ENTRY(p2, metadata_reply_to_message_id),
    W27_ENTRY(p2, dm_topic_fallback),
    W27_ENTRY(p2, dm_topic_missing_anchor_error),
    W27_ENTRY(p2, is_private_dm_topic_send),
};

static const w27_entry W27_P3_FUNCS[] = {
    W27_ENTRY(p3, reply_to_message_id_for_send),
    W27_ENTRY(p3, thread_kwargs_for_send),
    W27_ENTRY(p3, direct_topic_kwargs),
    W27_ENTRY(p3, thread_kwargs_for_draft),
    W27_ENTRY(p3, message_thread_id_for_send),
    W27_ENTRY(p3, message_thread_id_for_typing),
};

static const w27_entry W27_P4_FUNCS[] = {
    W27_ENTRY(p4, is_thread_not_found_error),
    W27_ENTRY(p4, is_bad_request_error),
    W27_ENTRY(p4, is_reply_anchor_missing_error),
    W27_ENTRY(p4, should_retry_without_dm_topic_reply_anchor),
    W27_ENTRY(p4, looks_like_polling_conflict),
    W27_ENTRY(p4, looks_like_auth_error),
};

static const w27_entry W27_P5_FUNCS[] = {
    W27_ENTRY(p5, looks_like_network_error),
    W27_ENTRY(p5, exception_graph_matches),
    W27_ENTRY(p5, looks_like_connect_timeout),
    W27_ENTRY(p5, looks_like_pool_timeout),
    W27_ENTRY(p5, coerce_bool_extra),
};

static const w27_entry W27_P6_FUNCS[] = {
    W27_ENTRY(p6, details_math_crash_shape),
    W27_ENTRY(p6, cjk_rich_garble_shape),
    W27_ENTRY(p6, needs_rich_rendering),
    W27_ENTRY(p6, is_rich_capability_error),
    W27_ENTRY(p6, is_rich_fallback_error),
};

static const w27_entry W27_P7_FUNCS[] = {
    W27_ENTRY(p7, is_timed_out),
    W27_ENTRY(p7, should_thread_reply),
    W27_ENTRY(p7, chunk_reply_routing),
    W27_ENTRY(p7, compute_single_send_routing),
    W27_ENTRY(p7, send_retry_is_final),
};

static const w27_entry W27_P8_FUNCS[] = {
    W27_ENTRY(p8, bot_commands),
    W27_ENTRY(p8, is_id_like),
    W27_ENTRY(p8, normalize_reaction_event),
    W27_ENTRY(p8, normalize_message_edited_event),
    W27_ENTRY(p8, normalize_platform_event),
};

static const w27_entry W27_P9_FUNCS[] = {
    W27_ENTRY(p9, rows_of_two),
    W27_ENTRY(p9, ea_escape),
    W27_ENTRY(p9, missing_media_path_error),
    W27_ENTRY(p9, media_too_large_note),
    W27_ENTRY(p9, int_or_zero),
    W27_ENTRY(p9, media_size_allowed),
    W27_ENTRY(p9, caption_1024),
};

static const w27_entry W27_P10_FUNCS[] = {
    W27_ENTRY(p10, is_transient_typing_error),
    W27_ENTRY(p10, extra_bool),
    W27_ENTRY(p10, extra_str_set),
    W27_ENTRY(p10, chat_type_str),
    W27_ENTRY(p10, chat_id_str),
    W27_ENTRY(p10, topic_id_or_general),
};

static const w27_entry W27_P11_FUNCS[] = {
    W27_ENTRY(p11, is_group_chat),
    W27_ENTRY(p11, effective_message_thread_id),
    W27_ENTRY(p11, is_reply_to_bot),
    W27_ENTRY(p11, entity_type),
    W27_ENTRY(p11, entity_span),
    W27_ENTRY(p11, telegram_entity_text),
    W27_ENTRY(p11, extract_bot_mention_usernames),
};

static const w27_entry W27_P12_FUNCS[] = {
    W27_ENTRY(p12, message_mentions_bot),
    W27_ENTRY(p12, sender_is_other_bot),
    W27_ENTRY(p12, bot_sender_suppressed),
    W27_ENTRY(p12, media_message_type),
    W27_ENTRY(p12, append_observed_note),
    W27_ENTRY(p12, is_own_message),
};

static const w27_entry W27_P13_FUNCS[] = {
    W27_ENTRY(p13, text_batch_delay_for),
    W27_ENTRY(p13, ext_from_path),
    W27_ENTRY(p13, get_dm_topic_info),
    W27_ENTRY(p13, resolve_topic_binding),
    W27_ENTRY(p13, group_observe_channel_prompt),
};

static json_t *w27_run(const w27_entry *table, size_t n, const char *func, const json_t *req) {
    size_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(table[i].name, func)) return table[i].fn(req);
    return json_string("UNKNOWN_FUNC");
}

json_t *w27_adapter_a96ad6_p1_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P1_FUNCS, sizeof(W27_P1_FUNCS) / sizeof(W27_P1_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p2_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P2_FUNCS, sizeof(W27_P2_FUNCS) / sizeof(W27_P2_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p3_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P3_FUNCS, sizeof(W27_P3_FUNCS) / sizeof(W27_P3_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p4_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P4_FUNCS, sizeof(W27_P4_FUNCS) / sizeof(W27_P4_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p5_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P5_FUNCS, sizeof(W27_P5_FUNCS) / sizeof(W27_P5_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p6_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P6_FUNCS, sizeof(W27_P6_FUNCS) / sizeof(W27_P6_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p7_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P7_FUNCS, sizeof(W27_P7_FUNCS) / sizeof(W27_P7_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p8_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P8_FUNCS, sizeof(W27_P8_FUNCS) / sizeof(W27_P8_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p9_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P9_FUNCS, sizeof(W27_P9_FUNCS) / sizeof(W27_P9_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p10_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P10_FUNCS, sizeof(W27_P10_FUNCS) / sizeof(W27_P10_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p11_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P11_FUNCS, sizeof(W27_P11_FUNCS) / sizeof(W27_P11_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p12_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P12_FUNCS, sizeof(W27_P12_FUNCS) / sizeof(W27_P12_FUNCS[0]), func, req);
}
json_t *w27_adapter_a96ad6_p13_dispatch(const char *func, const json_t *req) {
    return w27_run(W27_P13_FUNCS, sizeof(W27_P13_FUNCS) / sizeof(W27_P13_FUNCS[0]), func, req);
}
