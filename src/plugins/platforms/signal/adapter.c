/*
 * adapter.c — C home for upstream Hermes plugins/platforms/signal/adapter.py.
 *
 * One port file shared by the three differential gates
 *   w29_adapter_d36d4e_p1 / _p2 / _p3.
 *
 * PORTED (pure; one PoP line per symbol below):
 *   module helpers: comma-list parsing, magic-byte extension guess (incl. the
 *   tools/audio_container sniff), image/audio extension predicates, ext→mime,
 *   mention rendering, service-id / E.164 predicates, config validation;
 *   inbound: sync-message unwrap, group mention policy, group allowlist,
 *   recipient identifier cache, quote author, own-quote detection, sent
 *   timestamp caches, contact uuid extraction, envelope decision chain;
 *   outbound: target routing keys, send-result validation, UTF-16 offsets,
 *   per-chunk style translation, formatted split, sent-timestamp LRU,
 *   typing backoff, reaction target/params, image error text, chat info.
 *
 * SEAMED (fixed fixtures stand in; no socket/file/process work here):
 *   HTTP client + SSE body (connect/_sse_listener/_handle_sse_line/_rpc),
 *   ffmpeg AAC→M4A remux, media cache writes + attachment fetch, gateway
 *   authorization (_is_sender_authorized), asyncio timers, i18n strings,
 *   markdown→Signal conversion (plugins/platforms/signal/signal_format.c).
 *
 * PoP: plugins_platforms_signal_adapter_p1 @ plugins/platforms/signal/adapter.py:_parse_comma_list,_guess_extension,_is_image_ext,_is_audio_ext,_ext_to_mime,_render_mentions,_is_signal_service_id,_looks_like_e164_number,validate_signal_config,check_signal_requirements
 * PoP: plugins_platforms_signal_adapter_p2 @ plugins/platforms/signal/adapter.py:_unwrap_sync_message,_apply_group_mention_rules,_group_allowed,_remember_recipient_identifiers,_extract_quote_author,_quote_references_own_message,_remember_sent_message_timestamp,_extract_contact_uuid,_consume_sent_timestamp,_handle_envelope
 * PoP: plugins_platforms_signal_adapter_p3 @ plugins/platforms/signal/adapter.py:_with_target,_validate_send_result,_utf16_offsets,_styles_for_chunk,_split_signal_formatted_message,_track_sent_timestamp,send_typing,_extract_reaction_target,send_reaction,remove_reaction,send_image,get_chat_info
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "hermes_json.h"
#include "noc_w29_adapter_d36d4e_p1.h"
#include "noc_w29_adapter_d36d4e_p2.h"
#include "noc_w29_adapter_d36d4e_p3.h"

/* ─────────────────────────── small utilities ─────────────────────────── */

static char *dup_str(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static json_t *err_obj(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

/* Python truthiness of a JSON value ("absent" and JSON null are both None). */
static bool jtruthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
    case JSON_NULL:   return false;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool num_integral(double v, long long *out) {
    long long ll;
    if (v != v) return false;               /* NaN */
    if (v > 9.2e18 || v < -9.2e18) return false;
    ll = (long long)v;
    if ((double)ll != v) return false;
    if (out) *out = ll;
    return true;
}

/* Python str() for the scalar shapes the adapter stringifies. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dup_str("None");
    switch (v->type) {
    case JSON_NULL: return dup_str("None");
    case JSON_BOOL: return dup_str(v->bool_val ? "True" : "False");
    case JSON_STRING: return dup_str(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        long long ll;
        if (num_integral(v->num_val, &ll)) {
            snprintf(buf, sizeof(buf), "%lld", ll);
            return dup_str(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return dup_str(buf);
    }
    case JSON_ARRAY: return dup_str("[...]");
    case JSON_OBJECT: return dup_str("{...}");
    }
    return dup_str("None");
}

/* Canonical map key for timestamp-keyed caches (fixture seam: keys arrive as
 * JSON object keys, i.e. strings, while Python built them from ints). */
static char *canon_key(const json_t *v) {
    char buf[64];
    long long ll;
    if (!v) return dup_str("None");
    if (v->type == JSON_STRING) return dup_str(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER && num_integral(v->num_val, &ll)) {
        snprintf(buf, sizeof(buf), "%lld", ll);
        return dup_str(buf);
    }
    return py_str(v);
}

/* Python == between two JSON scalars. */
static bool jequal(const json_t *a, const json_t *b) {
    bool an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (b->num_val != 0.0) == a->bool_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return (a->num_val != 0.0) == b->bool_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}

static bool starts_with(const char *s, const char *pre) {
    return s && strncmp(s, pre, strlen(pre)) == 0;
}

static bool jequal_str(const json_t *v, const char *s) {
    if (!v || v->type != JSON_STRING) return false;
    return strcmp(v->str_val ? v->str_val : "", s ? s : "") == 0;
}

static unsigned u8_decode(const char *s, long *adv) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned cp;
    *adv = 1;
    if (!p[0]) return 0;
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xE0) == 0xC0 && p[1]) { cp = ((unsigned)(p[0] & 0x1F) << 6) | (p[1] & 0x3F); *adv = 2; return cp; }
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        cp = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *adv = 3; return cp;
    }
    if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
        cp = ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12) |
             ((unsigned)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        *adv = 4; return cp;
    }
    return p[0];
}

static long u8_count(const char *s) {
    long n = 0;
    if (!s) return 0;
    while (*s) { long adv; u8_decode(s, &adv); s += adv; n++; }
    return n;
}

/* Byte offset of codepoint index cp, clamped to the string end. */
static long u8_byte_off(const char *s, long cp) {
    const char *p = s;
    long i = 0;
    if (!s) return 0;
    if (cp < 0) cp = 0;
    while (*p && i < cp) { long adv; u8_decode(p, &adv); p += adv; i++; }
    return (long)(p - s);
}

/* Python s[a:b] over codepoints (a, b clamped; negative handled as 0). */
static char *u8_slice(const char *s, long a, long b) {
    long n, ba, bb;
    char *out;
    if (!s) return dup_str("");
    n = u8_count(s);
    if (a < 0) a = 0;
    if (b > n) b = n;
    if (a > n) a = n;
    if (b < a) b = a;
    ba = u8_byte_off(s, a);
    bb = u8_byte_off(s, b);
    out = (char *)malloc((size_t)(bb - ba) + 1);
    if (!out) return NULL;
    memcpy(out, s + ba, (size_t)(bb - ba));
    out[bb - ba] = 0;
    return out;
}

static int u16_w(unsigned cp) { return cp > 0xFFFF ? 2 : 1; }

static long u16_len(const char *s) {
    long n = 0;
    if (!s) return 0;
    while (*s) { long adv; unsigned cp = u8_decode(s, &adv); s += adv; n += u16_w(cp); }
    return n;
}

/* ─────────────────────────── string building ─────────────────────────── */

typedef struct { char *p; size_t len, cap; } sb_t;

static void sb_init(sb_t *b) { b->cap = 64; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = 0; }
static void sb_need(sb_t *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t cap = b->cap * 2;
        char *np;
        while (cap < b->len + extra + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np; b->cap = cap;
    }
}
static void sb_addn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s) return;
    sb_need(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}
static void sb_add(sb_t *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static void sb_addc(sb_t *b, char c) { sb_addn(b, &c, 1); }
static char *sb_take(sb_t *b) { char *p = b->p; b->p = NULL; b->len = b->cap = 0; return p; }

static bool is_py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip(const char *s) {
    size_t a = 0, b;
    char *out;
    if (!s) return dup_str("");
    b = strlen(s);
    while (a < b && is_py_space(s[a])) a++;
    while (b > a && is_py_space(s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = 0;
    return out;
}

/* str.replace(old, new) — non-overlapping, left to right. */
static char *str_replace(const char *s, const char *old, const char *with) {
    sb_t b;
    const char *p = s;
    size_t ol;
    if (!s || !old || !*old) return dup_str(s ? s : "");
    ol = strlen(old);
    sb_init(&b);
    while (*p) {
        if (!strncmp(p, old, ol)) { sb_add(&b, with); p += ol; }
        else { sb_addc(&b, *p); p++; }
    }
    return sb_take(&b);
}

static char *base64_decode(const char *in, size_t *out_len) {
    unsigned char *out;
    size_t n = 0, cap, i;
    unsigned acc = 0;
    int bits = 0;
    if (!in) { *out_len = 0; return NULL; }
    cap = strlen(in) * 3 / 4 + 4;
    out = (unsigned char *)malloc(cap);
    if (!out) { *out_len = 0; return NULL; }
    for (i = 0; in[i]; i++) {
        char c = in[i];
        int val;
        if (c == '=') break;
        if (c >= 'A' && c <= 'Z') val = c - 'A';
        else if (c >= 'a' && c <= 'z') val = c - 'a' + 26;
        else if (c >= '0' && c <= '9') val = c - '0' + 52;
        else if (c == '+') val = 62;
        else if (c == '/') val = 63;
        else continue;                       /* Python b64decode ignores non-alphabet */
        acc = (acc << 6) | (unsigned)val;
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            out[n++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    *out_len = n;
    return (char *)out;
}

/* ───────────────────── sniffing (tools/audio_container) ──────────────── */

/* PoP: sniff_container @ tools/audio_container.py:sniff_container */
static const char *sniff_container(const unsigned char *d, size_t n) {
    if (n >= 8 && !memcmp(d + 4, "ftyp", 4)) {
        if (n >= 12) {
            char b[5];
            int i;
            for (i = 0; i < 4; i++) b[i] = (char)tolower((unsigned char)d[8 + i]);
            b[4] = 0;
            if (!memcmp(b, "m4a ", 4) || !memcmp(b, "m4b ", 4)) return "m4a";
        }
        return "mp4";
    }
    if (starts_with((const char *)d, "OggS")) return "ogg";
    if (starts_with((const char *)d, "fLaC")) return "flac";
    if (starts_with((const char *)d, "ID3")) return "mp3";
    if (n >= 12 && !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WAVE", 4)) return "wav";
    if (n >= 2 && d[0] == 0xFF && (d[1] & 0xE0) == 0xE0)
        return ((d[1] & 0xF6) == 0xF0) ? "aac" : "mp3";
    if (!memcmp(d, "\x1a\x45\xdf\xa3", 4)) return "webm";
    return NULL;
}

/* ──────────────────────── gate p1: module helpers ─────────────────────── */

/* PoP: _parse_comma_list @ plugins/platforms/signal/adapter.py:_parse_comma_list */
json_t *w29_adapter_d36d4e_p1_parse_comma_list(const json_t *value) {
    json_t *out = json_array();
    char *copy, *tok, *save = NULL;
    if (!value || value->type != JSON_STRING) {
        json_free(out);
        return err_obj("AttributeError");
    }
    copy = dup_str(value->str_val ? value->str_val : "");
    tok = strtok_r(copy, ",", &save);
    while (tok) {
        char *t = py_strip(tok);
        if (t && *t) json_append(out, json_string(t));
        free(t);
        tok = strtok_r(NULL, ",", &save);
    }
    free(copy);
    return out;
}

static const char *const CONTAINER_EXT[][2] = {
    {"m4a", ".m4a"}, {"mp4", ".mp4"}, {"ogg", ".ogg"}, {"flac", ".flac"},
    {"wav", ".wav"}, {"mp3", ".mp3"}, {"aac", ".aac"}, {"webm", ".webm"},
};

/* PoP: _guess_extension @ plugins/platforms/signal/adapter.py:_guess_extension */
json_t *w29_adapter_d36d4e_p1_guess_extension(const json_t *data_b64) {
    char *raw;
    size_t n = 0;
    const unsigned char *d;
    const char *ext = NULL;
    json_t *out;
    size_t i;
    if (!data_b64 || data_b64->type != JSON_STRING) return err_obj("TypeError");
    raw = base64_decode(data_b64->str_val ? data_b64->str_val : "", &n);
    d = (const unsigned char *)raw;
    if (n >= 4 && !memcmp(d, "\x89PNG", 4)) ext = ".png";
    else if (n >= 2 && !memcmp(d, "\xff\xd8", 2)) ext = ".jpg";
    else if (n >= 4 && !memcmp(d, "GIF8", 4)) ext = ".gif";
    else if (n >= 4 && !memcmp(d, "%PDF", 4)) ext = ".pdf";
    else if (n >= 12 && !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WEBP", 4)) ext = ".webp";
    else {
        const char *c = sniff_container(d, n);
        if (c) {
            for (i = 0; i < sizeof(CONTAINER_EXT) / sizeof(CONTAINER_EXT[0]); i++)
                if (!strcmp(CONTAINER_EXT[i][0], c)) ext = CONTAINER_EXT[i][1];
        }
    }
    if (!ext) ext = (n >= 2 && d[0] == 'P' && d[1] == 'K') ? ".zip" : ".bin";
    out = json_string(ext);
    free(raw);
    return out;
}

/* PoP: _is_image_ext @ plugins/platforms/signal/adapter.py:_is_image_ext */
json_t *w29_adapter_d36d4e_p1_is_image_ext(const json_t *ext) {
    char *low;
    bool ok;
    if (!ext || ext->type != JSON_STRING) return err_obj("AttributeError");
    low = dup_str(ext->str_val ? ext->str_val : "");
    if (low) { char *p; for (p = low; *p; p++) *p = (char)tolower((unsigned char)*p); }
    ok = low && (!strcmp(low, ".jpg") || !strcmp(low, ".jpeg") || !strcmp(low, ".png") ||
                 !strcmp(low, ".gif") || !strcmp(low, ".webp"));
    free(low);
    return json_bool(ok);
}

/* PoP: _is_audio_ext @ plugins/platforms/signal/adapter.py:_is_audio_ext */
json_t *w29_adapter_d36d4e_p1_is_audio_ext(const json_t *ext) {
    char *low;
    bool ok;
    if (!ext || ext->type != JSON_STRING) return err_obj("AttributeError");
    low = dup_str(ext->str_val ? ext->str_val : "");
    if (low) { char *p; for (p = low; *p; p++) *p = (char)tolower((unsigned char)*p); }
    ok = low && (!strcmp(low, ".mp3") || !strcmp(low, ".wav") || !strcmp(low, ".ogg") ||
                 !strcmp(low, ".m4a") || !strcmp(low, ".aac"));
    free(low);
    return json_bool(ok);
}

/* PoP: _ext_to_mime @ plugins/platforms/signal/adapter.py:_ext_to_mime,
 *      mime_for_ext @ gateway/platforms/media_cache.py:mime_for_ext */
json_t *w29_adapter_d36d4e_p1_ext_to_mime(const json_t *ext) {
    static const char *const TBL[][2] = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
        {".gif", "image/gif"}, {".webp", "image/webp"}, {".ogg", "audio/ogg"},
        {".mp3", "audio/mpeg"}, {".wav", "audio/wav"}, {".m4a", "audio/mp4"},
        {".aac", "audio/aac"}, {".mp4", "video/mp4"}, {".pdf", "application/pdf"},
        {".zip", "application/zip"},
    };
    char *low, *strip;
    size_t i;
    const char *mime = "application/octet-stream";
    if (ext && ext->type != JSON_STRING) return err_obj("AttributeError");
    low = dup_str(ext ? (ext->str_val ? ext->str_val : "") : "");
    if (low) { char *p; for (p = low; *p; p++) *p = (char)tolower((unsigned char)*p); }
    strip = py_strip(low ? low : "");
    for (i = 0; i < sizeof(TBL) / sizeof(TBL[0]); i++) {
        if (!strcmp(strip, TBL[i][0])) { mime = TBL[i][1]; break; }
    }
    free(low);
    free(strip);
    return json_string(mime);
}

typedef struct { long start, len; char *ident; } mention_t;

/* PoP: _render_mentions @ plugins/platforms/signal/adapter.py:_render_mentions */
json_t *w29_adapter_d36d4e_p1_render_mentions(const json_t *text_v, const json_t *mentions) {
    const char *text;
    mention_t *ms;
    size_t n = 0, i;
    char *cur;
    if (text_v && text_v->type != JSON_STRING) return err_obj("AttributeError");
    text = text_v ? (text_v->str_val ? text_v->str_val : "") : "";
    if (!mentions || mentions->type != JSON_ARRAY || mentions->c.count == 0 ||
        !strstr(text, "\xEF\xBF\xBC"))
        return json_string(text);
    n = mentions->c.count;
    ms = (mention_t *)calloc(n, sizeof(*ms));
    if (!ms) return json_string(text);
    for (i = 0; i < n; i++) {
        const json_t *m = mentions->c.items[i];
        const json_t *num, *uuid, *idv, *st, *ln;
        if (!m || m->type != JSON_OBJECT) { free(ms); return err_obj("AttributeError"); }
        num = json_obj_get(m, "number");
        uuid = json_obj_get(m, "uuid");
        st = json_obj_get(m, "start");
        ln = json_obj_get(m, "length");
        ms[i].start = (st && st->type == JSON_NUMBER) ? (long)st->num_val : 0;
        ms[i].len = (ln && ln->type == JSON_NUMBER) ? (long)ln->num_val : 1;
        idv = jtruthy(num) ? num : (jtruthy(uuid) ? uuid : NULL);
        ms[i].ident = idv ? py_str(idv) : dup_str("user");
    }
    /* stable descending by start: insertion sort */
    for (i = 1; i < n; i++) {
        mention_t key = ms[i];
        size_t j = i;
        while (j > 0 && ms[j - 1].start < key.start) { ms[j] = ms[j - 1]; j--; }
        ms[j] = key;
    }
    cur = dup_str(text);
    for (i = 0; i < n; i++) {
        sb_t b;
        char *head, *tail, *next;
        sb_init(&b);
        head = u8_slice(cur, 0, ms[i].start);
        tail = u8_slice(cur, ms[i].start + ms[i].len, u8_count(cur));
        sb_add(&b, head);
        sb_addc(&b, '@');
        sb_add(&b, ms[i].ident ? ms[i].ident : "user");
        sb_add(&b, tail);
        next = sb_take(&b);
        free(head); free(tail); free(cur);
        cur = next;
    }
    for (i = 0; i < n; i++) free(ms[i].ident);
    free(ms);
    {
        json_t *out = json_string(cur ? cur : "");
        free(cur);
        return out;
    }
}

/* uuid.UUID(value) acceptance: 32 hex digits after stripping urn:/uuid:/{} and '-'. */
static bool looks_like_uuid(const char *v) {
    char buf[64];
    size_t n = 0, i;
    const char *p;
    if (!v) return false;
    for (p = v; *p; p++) {
        if (!strncmp(p, "urn:", 4)) { p += 3; continue; }
        if (!strncmp(p, "uuid:", 5)) { p += 4; continue; }
        if (*p == '{' || *p == '}' || *p == '-') continue;
        if (n + 1 >= sizeof(buf)) return false;
        buf[n++] = *p;
    }
    buf[n] = 0;
    if (n != 32) return false;
    for (i = 0; i < n; i++) if (!isxdigit((unsigned char)buf[i])) return false;
    return true;
}

/* PoP: _is_signal_service_id @ plugins/platforms/signal/adapter.py:_is_signal_service_id */
json_t *w29_adapter_d36d4e_p1_is_signal_service_id(const json_t *value) {
    const char *v;
    if (!jtruthy(value)) return json_bool(false);
    if (value->type != JSON_STRING) return err_obj("AttributeError");
    v = value->str_val ? value->str_val : "";
    if (starts_with(v, "PNI:") || starts_with(v, "u:")) return json_bool(true);
    return json_bool(looks_like_uuid(v));
}

/* PoP: _looks_like_e164_number @ plugins/platforms/signal/adapter.py:_looks_like_e164_number */
json_t *w29_adapter_d36d4e_p1_looks_like_e164_number(const json_t *value) {
    const char *v;
    size_t i, len;
    if (!jtruthy(value)) return json_bool(false);
    if (value->type != JSON_STRING) return err_obj("AttributeError");
    v = value->str_val ? value->str_val : "";
    if (v[0] != '+') return json_bool(false);
    len = strlen(v) - 1;
    for (i = 1; v[i]; i++)
        if (!isdigit((unsigned char)v[i])) return json_bool(false);
    return json_bool(len >= 7 && len <= 15);
}

/* PoP: validate_signal_config @ plugins/platforms/signal/adapter.py:validate_signal_config */
json_t *w29_adapter_d36d4e_p1_validate_signal_config(const json_t *extra, const json_t *env) {
    const json_t *h, *a;
    const char *hs, *as;
    char *h_strip, *a_strip;
    json_t *out;
    if (extra && extra->type != JSON_OBJECT && extra->type != JSON_NULL)
        return err_obj("AttributeError");
    h = jtruthy(json_obj_get(extra, "http_url")) ? json_obj_get(extra, "http_url")
                                                 : json_obj_get(env, "SIGNAL_HTTP_URL");
    a = jtruthy(json_obj_get(extra, "account")) ? json_obj_get(extra, "account")
                                                : json_obj_get(env, "SIGNAL_ACCOUNT");
    if ((h && h->type != JSON_STRING) || (a && a->type != JSON_STRING))
        return err_obj("AttributeError");
    hs = (h && h->type == JSON_STRING) ? h->str_val : "";
    as = (a && a->type == JSON_STRING) ? a->str_val : "";
    h_strip = py_strip(hs ? hs : "");
    a_strip = py_strip(as ? as : "");
    out = json_bool((h_strip && *h_strip) && (a_strip && *a_strip));
    free(h_strip);
    free(a_strip);
    return out;
}

/* PoP: check_signal_requirements @ plugins/platforms/signal/adapter.py:check_signal_requirements */
json_t *w29_adapter_d36d4e_p1_check_signal_requirements(void) {
    return json_bool(true);
}

/* ──────────────── gate p2: inbound envelope decisions ────────────────── */

static bool svc_ok(const json_t *v, bool *raised) {
    if (!jtruthy(v)) return false;
    if (v->type != JSON_STRING) { if (raised) *raised = true; return false; }
    return starts_with(v->str_val, "PNI:") || starts_with(v->str_val, "u:") ||
           looks_like_uuid(v->str_val);
}

static bool in_str_array(const json_t *arr, const char *s) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (i = 0; i < arr->c.count; i++)
        if (jequal_str(arr->c.items[i], s)) return true;
    return false;
}

static json_t *keys_array(const json_t *obj) {
    json_t *out = json_array();
    size_t i;
    if (!obj || obj->type != JSON_OBJECT) return out;
    for (i = 0; i < obj->c.count; i++) json_append(out, json_string(obj->c.keys[i]));
    return out;
}

/* PoP: _extract_quote_author @ plugins/platforms/signal/adapter.py:_extract_quote_author */
json_t *w29_adapter_d36d4e_p2_extract_quote_author(const json_t *quote) {
    static const char *const KEYS[] = {
        "author", "authorNumber", "authorUuid", "authorAci", "authorServiceId", "authorServiceIdString",
    };
    size_t i;
    if (!quote || quote->type != JSON_OBJECT) return json_null();
    for (i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        const json_t *v = json_obj_get(quote, KEYS[i]);
        if (jtruthy(v)) {
            char *s = py_str(v);
            json_t *out = json_string(s ? s : "");
            free(s);
            return out;
        }
    }
    return json_null();
}

/* PoP: _group_allowed @ plugins/platforms/signal/adapter.py:_group_allowed */
json_t *w29_adapter_d36d4e_p2_group_allowed(const json_t *group_id, const json_t *allow) {
    size_t i;
    bool star = false, found = false;
    if (!allow || allow->type != JSON_ARRAY || allow->c.count == 0) return json_bool(false);
    for (i = 0; i < allow->c.count; i++) {
        if (jequal_str(allow->c.items[i], "*")) star = true;
        if (group_id && allow->c.items[i] && jequal(allow->c.items[i], group_id)) found = true;
    }
    return json_bool(star || found);
}

/* PoP: _remember_recipient_identifiers @ plugins/platforms/signal/adapter.py:_remember_recipient_identifiers */
json_t *w29_adapter_d36d4e_p2_remember_recipient_identifiers(const json_t *number, const json_t *service_id,
                                                            const json_t *maps) {
    json_t *out = json_object();
    json_t *n2u, *u2n;
    bool raised = false;
    n2u = (maps && maps->type == JSON_OBJECT) ? json_copy(json_obj_get(maps, "number_to_uuid")) : json_object();
    u2n = (maps && maps->type == JSON_OBJECT) ? json_copy(json_obj_get(maps, "uuid_to_number")) : json_object();
    if (!n2u || n2u->type != JSON_OBJECT) { json_free(n2u); n2u = json_object(); }
    if (!u2n || u2n->type != JSON_OBJECT) { json_free(u2n); u2n = json_object(); }
    if (jtruthy(number) && jtruthy(service_id)) {
        if (svc_ok(service_id, &raised)) {
            char *nk = py_str(number), *sk = py_str(service_id);
            json_set(n2u, nk ? nk : "", json_copy(service_id));
            json_set(u2n, sk ? sk : "", json_copy(number));
            free(nk); free(sk);
        } else if (raised) {
            json_free(n2u); json_free(u2n);
            return err_obj("AttributeError");
        }
    }
    json_set(out, "number_to_uuid", n2u);
    json_set(out, "uuid_to_number", u2n);
    return out;
}

/* PoP: _quote_references_own_message @ plugins/platforms/signal/adapter.py:_quote_references_own_message */
json_t *w29_adapter_d36d4e_p2_quote_references_own_message(const json_t *reply_to_id,
                                                           const json_t *reply_to_author,
                                                           const json_t *sent_timestamps,
                                                           const json_t *account,
                                                           const json_t *uuid_by_number,
                                                           const json_t *number_by_uuid) {
    const char *acct = (account && account->type == JSON_STRING) ? account->str_val : "";
    char *rid;
    if (jtruthy(reply_to_id)) {
        rid = py_str(reply_to_id);
        if (in_str_array(sent_timestamps, rid)) { free(rid); return json_bool(true); }
        free(rid);
    }
    if (!jtruthy(reply_to_author)) return json_bool(false);
    {
        char *author_raw = py_str(reply_to_author);
        char *author = py_strip(author_raw ? author_raw : "");
        bool hit = false;
        free(author_raw);
        if (acct && *acct && author) {
            if (!strcmp(author, acct)) hit = true;
            else {
                const json_t *bot = json_obj_get(uuid_by_number, acct);
                const json_t *num;
                if (jtruthy(bot)) { char *bs = py_str(bot); if (bs && !strcmp(author, bs)) hit = true; free(bs); }
                num = json_obj_get(number_by_uuid, author);
                if (!hit && jequal_str(num, acct)) hit = true;
            }
        }
        free(author);
        return json_bool(hit);
    }
}

/* PoP: _remember_sent_message_timestamp @ plugins/platforms/signal/adapter.py:_remember_sent_message_timestamp */
json_t *w29_adapter_d36d4e_p2_remember_sent_message_timestamp(const json_t *cache, const json_t *timestamp,
                                                             const json_t *max_v) {
    json_t *out = json_array();
    long max = 500;
    char *key;
    size_t i;
    if (max_v && max_v->type == JSON_NUMBER) max = (long)max_v->num_val;
    if (cache && cache->type == JSON_ARRAY)
        for (i = 0; i < cache->c.count; i++) {
            char *k = canon_key(cache->c.items[i]);
            json_append(out, json_string(k ? k : ""));
            free(k);
        }
    if (!timestamp || timestamp->type == JSON_NULL) return out;
    key = py_str(timestamp);
    for (i = 0; i < out->c.count; i++) {
        if (jequal_str(out->c.items[i], key)) {
            size_t j;
            json_free(out->c.items[i]);
            for (j = i; j + 1 < out->c.count; j++) out->c.items[j] = out->c.items[j + 1];
            out->c.count--;
            break;
        }
    }
    json_append(out, json_string(key ? key : ""));
    free(key);
    while ((long)out->c.count > max && out->c.count > 0) {
        size_t j;
        json_free(out->c.items[0]);
        for (j = 0; j + 1 < out->c.count; j++) out->c.items[j] = out->c.items[j + 1];
        out->c.count--;
    }
    return out;
}

/* PoP: _extract_contact_uuid @ plugins/platforms/signal/adapter.py:_extract_contact_uuid */
json_t *w29_adapter_d36d4e_p2_extract_contact_uuid(const json_t *contact, const json_t *phone) {
    const json_t *sid, *profile;
    bool raised = false;
    if (!contact || contact->type != JSON_OBJECT) return json_null();
    sid = jtruthy(json_obj_get(contact, "uuid")) ? json_obj_get(contact, "uuid")
                                                 : json_obj_get(contact, "serviceId");
    profile = json_obj_get(contact, "profile");
    if (!jtruthy(sid) && profile && profile->type == JSON_OBJECT)
        sid = jtruthy(json_obj_get(profile, "serviceId")) ? json_obj_get(profile, "serviceId")
                                                          : json_obj_get(profile, "uuid");
    if (jtruthy(sid) && svc_ok(sid, &raised) &&
        (jequal(json_obj_get(contact, "number"), phone) || jequal(json_obj_get(contact, "recipient"), phone))) {
        return json_copy(sid);
    }
    return json_null();
}

/* PoP: _consume_sent_timestamp @ plugins/platforms/signal/adapter.py:_consume_sent_timestamp */
json_t *w29_adapter_d36d4e_p2_consume_sent_timestamp(const json_t *recent, const json_t *ts) {
    json_t *rem = (recent && recent->type == JSON_OBJECT) ? json_copy(recent) : json_object();
    json_t *out = json_object();
    bool echo = false;
    if (jtruthy(ts)) {
        char *key = canon_key(ts);
        const json_t *v = json_obj_get(rem, key);
        if (v) { echo = (v->type != JSON_NULL); json_obj_del(rem, key); }
        free(key);
    }
    json_set(out, "echo", json_bool(echo));
    json_set(out, "recent", keys_array(rem));
    json_free(rem);
    return out;
}

/* PoP: _unwrap_sync_message @ plugins/platforms/signal/adapter.py:_unwrap_sync_message */
json_t *w29_adapter_d36d4e_p2_unwrap_sync_message(const json_t *envelope_data, const json_t *account,
                                                  const json_t *recent_sent) {
    const json_t *sync_msg, *sent_msg, *dest, *group_info;
    json_t *out;
    char *acct;
    if (!envelope_data || envelope_data->type != JSON_OBJECT) return err_obj("AttributeError");
    sync_msg = json_obj_get(envelope_data, "syncMessage");
    sent_msg = (sync_msg && sync_msg->type == JSON_OBJECT) ? json_obj_get(sync_msg, "sentMessage") : NULL;
    out = json_object();
    if (!jtruthy(sent_msg) || sent_msg->type != JSON_OBJECT) {
        json_set(out, "envelope", json_null());
        return out;
    }
    dest = jtruthy(json_obj_get(sent_msg, "destinationNumber")) ? json_obj_get(sent_msg, "destinationNumber")
                                                                : json_obj_get(sent_msg, "destination");
    group_info = json_obj_get(sent_msg, "groupInfo");
    acct = (account && account->type == JSON_STRING) ? account->str_val : "";
    if (!jequal_str(dest, acct) && !jtruthy(json_obj_get(group_info, "groupId"))) {
        json_set(out, "envelope", json_null());
        return out;
    }
    {
        char *key = canon_key(json_obj_get(sent_msg, "timestamp"));
        const json_t *v = (recent_sent && recent_sent->type == JSON_OBJECT) ? json_obj_get(recent_sent, key) : NULL;
        bool echo = jtruthy(json_obj_get(sent_msg, "timestamp")) && v && v->type != JSON_NULL;
        free(key);
        if (echo) { json_set(out, "envelope", json_null()); return out; }
    }
    {
        json_t *merged = json_copy(envelope_data);
        json_set(merged, "dataMessage", json_copy(sent_msg));
        json_set(out, "envelope", merged);
    }
    return out;
}

/* PoP: _apply_group_mention_rules @ plugins/platforms/signal/adapter.py:_apply_group_mention_rules */
json_t *w29_adapter_d36d4e_p2_apply_group_mention_rules(const json_t *text_v, const json_t *data_message,
                                                        const json_t *account, const json_t *require_mention,
                                                        const json_t *uuid_by_number) {
    const char *acct = (account && account->type == JSON_STRING) ? account->str_val : "";
    const char *text = "";
    json_t *out = json_object();
    if (text_v && text_v->type != JSON_STRING && text_v->type != JSON_NULL) return err_obj("TypeError");
    if (text_v && text_v->type == JSON_STRING) text = text_v->str_val;
    if (jtruthy(require_mention)) {
        char *needle = NULL;
        bool in_text = false, in_meta = false;
        const json_t *mentions;
        size_t i;
        if (acct && *acct) {
            sb_t b; sb_init(&b); sb_addc(&b, '@'); sb_add(&b, acct); needle = sb_take(&b);
            in_text = needle && strstr(text, needle) != NULL;
            free(needle);
        }
        mentions = json_obj_get(data_message, "mentions");
        if (mentions && mentions->type == JSON_ARRAY)
            for (i = 0; i < mentions->c.count; i++) {
                const json_t *m = mentions->c.items[i];
                if (!m || m->type != JSON_OBJECT) continue;
                if (jequal_str(json_obj_get(m, "number"), acct) ||
                    jequal_str(json_obj_get(m, "uuid"), acct)) in_meta = true;
            }
        if (!in_text && !in_meta) {
            json_set(out, "mentioned", json_bool(false));
            json_set(out, "text", json_string(text));
            return out;
        }
    }
    {
        char *cur = dup_str(text);
        if (cur && *cur && acct && *acct) {
            sb_t b; char *needle, *next;
            sb_init(&b); sb_addc(&b, '@'); sb_add(&b, acct); needle = sb_take(&b);
            next = str_replace(cur, needle, "");
            free(needle); free(cur); cur = next;
            {
                const json_t *bot = json_obj_get(uuid_by_number, acct);
                if (jtruthy(bot)) {
                    char *bs = py_str(bot);
                    sb_init(&b); sb_addc(&b, '@'); sb_add(&b, bs ? bs : ""); needle = sb_take(&b);
                    next = str_replace(cur, needle, "");
                    free(needle); free(bs); free(cur); cur = next;
                }
            }
            next = str_replace(cur, "  ", " ");
            free(cur);
            cur = py_strip(next);
            free(next);
        }
        json_set(out, "mentioned", json_bool(true));
        json_set(out, "text", json_string(cur ? cur : ""));
        free(cur);
    }
    return out;
}

static const char *mime_msg_type(const json_t *media_types) {
    static const char *const PREFIX[][2] = {
        {"audio/", "voice"}, {"image/", "photo"}, {"video/", "video"},
    };
    size_t i, j;
    if (!media_types || media_types->type != JSON_ARRAY || media_types->c.count == 0) return "text";
    for (i = 0; i < 3; i++)
        for (j = 0; j < media_types->c.count; j++) {
            const json_t *m = media_types->c.items[j];
            if (m && m->type == JSON_STRING && starts_with(m->str_val, PREFIX[i][0])) return PREFIX[i][1];
        }
    return "document";
}

static bool text_is_blank(const char *s) {
    if (!s) return true;
    while (*s) { if (!is_py_space(*s)) return false; s++; }
    return true;
}

/* PoP: _handle_envelope @ plugins/platforms/signal/adapter.py:_handle_envelope
 * (decision chain only; handle_message/attachment fetch/authorization are seams) */
json_t *w29_adapter_d36d4e_p2_envelope_decision(const json_t *envelope, const json_t *account,
                                                const json_t *allow, const json_t *ignore_stories,
                                                const json_t *require_mention, const json_t *recent_sent,
                                                const json_t *sent_cache, const json_t *media_types,
                                                const json_t *ignore_attachments, const json_t *uuid_by_number) {
    const json_t *env_data, *sender = NULL, *data_message, *group_info, *group_id = NULL;
    const json_t *quote, *attachments, *text_v;
    json_t *out = json_object();
    bool note_to_self, is_group, media_present = false;
    const char *acct = (account && account->type == JSON_STRING) ? account->str_val : "";
    char *text;

#define DROP() do { json_free(out); out = json_object(); json_set(out, "dropped", json_bool(true)); return out; } while (0)

    if (!envelope || envelope->type != JSON_OBJECT) return err_obj("AttributeError");
    env_data = json_obj_get(envelope, "envelope");
    if (!env_data) env_data = envelope;
    if (env_data->type != JSON_OBJECT) return err_obj("TypeError");
    note_to_self = json_obj_get(env_data, "syncMessage") != NULL;
    if (note_to_self) {
        json_t *un = w29_adapter_d36d4e_p2_unwrap_sync_message(env_data, account, recent_sent);
        const json_t *merged = un && un->type == JSON_OBJECT ? json_obj_get(un, "envelope") : NULL;
        if (merged && merged->type == JSON_OBJECT) env_data = json_copy(merged);
        json_free(un);
        if (!merged || merged->type != JSON_OBJECT) DROP();
    }
    sender = json_obj_get(env_data, "sourceNumber");
    if (!jtruthy(sender)) sender = json_obj_get(env_data, "sourceUuid");
    if (!jtruthy(sender)) sender = json_obj_get(env_data, "source");
    if (!jtruthy(sender)) DROP();
    if (*acct && jequal_str(sender, acct) && !note_to_self) DROP();
    if (jtruthy(ignore_stories) && jtruthy(json_obj_get(env_data, "storyMessage"))) DROP();
    data_message = json_obj_get(env_data, "dataMessage");
    if (!jtruthy(data_message)) {
        const json_t *edit = json_obj_get(env_data, "editMessage");
        data_message = (edit && edit->type == JSON_OBJECT) ? json_obj_get(edit, "dataMessage") : NULL;
    }
    if (!jtruthy(data_message)) DROP();
    if (data_message->type != JSON_OBJECT) return err_obj("AttributeError");
    group_info = json_obj_get(data_message, "groupInfo");
    group_id = group_info ? json_obj_get(group_info, "groupId") : NULL;
    is_group = jtruthy(group_id);
    if (is_group) {
        json_t *allowed = w29_adapter_d36d4e_p2_group_allowed(group_id, allow);
        bool ok = jtruthy(allowed);
        json_free(allowed);
        if (!ok) DROP();
    }
    {
        char *chat_id;
        if (is_group) {
            sb_t b; char *gid = py_str(group_id);
            sb_init(&b); sb_add(&b, "group:"); sb_add(&b, gid ? gid : "");
            chat_id = sb_take(&b);
            free(gid);
        } else chat_id = py_str(sender);
        json_set(out, "chat_id", json_string(chat_id ? chat_id : ""));
        free(chat_id);
    }
    text_v = json_obj_get(data_message, "message");
    if (text_v && text_v->type != JSON_STRING && text_v->type != JSON_NULL) return err_obj("AttributeError");
    text = dup_str((text_v && text_v->type == JSON_STRING) ? text_v->str_val : "");
    {
        const json_t *mentions = json_obj_get(data_message, "mentions");
        if (text && *text && mentions && mentions->type == JSON_ARRAY && mentions->c.count > 0) {
            json_t *rendered = w29_adapter_d36d4e_p1_render_mentions(json_string(text), mentions);
            if (rendered && rendered->type == JSON_STRING) {
                free(text);
                text = dup_str(rendered->str_val);
            }
            json_free(rendered);
        }
    }
    if (is_group) {
        json_t *rules = w29_adapter_d36d4e_p2_apply_group_mention_rules(json_string(text), data_message,
                                                                        account, require_mention, uuid_by_number);
        const json_t *m = json_obj_get(rules, "mentioned");
        const json_t *t = json_obj_get(rules, "text");
        if (!jtruthy(m)) { json_free(rules); free(text); DROP(); }
        if (t && t->type == JSON_STRING) { free(text); text = dup_str(t->str_val); }
        json_free(rules);
    }
    quote = json_obj_get(data_message, "quote");
    if (!jtruthy(quote) || quote->type != JSON_OBJECT) quote = json_object();
    {
        const json_t *qid = json_obj_get(quote, "id");
        json_t *qauthor;
        json_t *author_j, *own;
        if (jtruthy(qid)) {
            char *s = py_str(qid);
            json_set(out, "reply_to_id", json_string(s ? s : ""));
            free(s);
        } else json_set(out, "reply_to_id", json_null());
        qauthor = w29_adapter_d36d4e_p2_extract_quote_author(quote);
        json_set(out, "reply_to_author", json_copy(qauthor));
        {
            const json_t *nm = json_obj_get(quote, "authorName");
            if (!jtruthy(nm)) nm = json_obj_get(quote, "authorProfileName");
            if (jtruthy(nm)) {
                char *s = py_str(nm);
                json_set(out, "reply_to_name", json_string(s ? s : ""));
                free(s);
            } else json_set(out, "reply_to_name", json_null());
        }
        author_j = json_copy(qauthor);
        own = w29_adapter_d36d4e_p2_quote_references_own_message(json_obj_get(out, "reply_to_id"),
                                                                 author_j, sent_cache, account,
                                                                 uuid_by_number, json_null());
        json_set(out, "own", json_copy(own));
        json_free(own);
        json_free(author_j);
        json_free(qauthor);
    }
    attachments = json_obj_get(data_message, "attachments");
    if (attachments && attachments->type == JSON_ARRAY && attachments->c.count > 0 &&
        !jtruthy(ignore_attachments) && media_types && media_types->type == JSON_ARRAY &&
        media_types->c.count > 0)
        media_present = true;
    if (text_is_blank(text) && !media_present) { free(text); DROP(); }
    json_set(out, "text", json_string(text ? text : ""));
    json_set(out, "msg_type", json_string(media_present ? mime_msg_type(media_types) : "text"));
    json_set(out, "dropped", json_bool(false));
    free(text);
    return out;
#undef DROP
}

/* ──────────────── gate p3: outbound message shaping ──────────────────── */

static bool py_int(const char *s, long *out) {
    char *end;
    long v;
    if (!s) return false;
    while (*s && is_py_space(*s)) s++;
    if (!*s) return false;
    v = strtol(s, &end, 10);
    if (end == s) return false;
    while (*end && is_py_space(*end)) end++;
    if (*end) return false;
    *out = v;
    return true;
}

static json_t *chunk_styles_json(const json_t *styles, long cs, long ce) {
    json_t *out = json_array();
    size_t i;
    if (!styles || styles->type != JSON_ARRAY) return out;
    for (i = 0; i < styles->c.count; i++) {
        const json_t *sv = styles->c.items[i];
        const char *s;
        char *copy, *c1, *c2;
        long a, l;
        if (!sv || sv->type != JSON_STRING) { json_free(out); return err_obj("AttributeError"); }
        s = sv->str_val ? sv->str_val : "";
        copy = dup_str(s);
        c1 = strchr(copy, ':');
        if (!c1) { free(copy); continue; }
        *c1 = 0;
        c2 = strchr(c1 + 1, ':');
        if (!c2) { free(copy); continue; }
        *c2 = 0;
        if (!py_int(copy, &a) || !py_int(c1 + 1, &l)) { free(copy); continue; }
        {
            long s0 = a, s1 = a + l;
            long o0 = s0 > cs ? s0 : cs;
            long o1 = s1 < ce ? s1 : ce;
            if (o0 < o1) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld:%ld:%s", o0 - cs, o1 - o0, c2 + 1);
                json_append(out, json_string(buf));
            }
        }
        free(copy);
    }
    return out;
}

/* PoP: _styles_for_chunk @ plugins/platforms/signal/adapter.py:_styles_for_chunk */
json_t *w29_adapter_d36d4e_p3_styles_for_chunk(const json_t *styles, const json_t *start_v, const json_t *end_v) {
    long a = (start_v && start_v->type == JSON_NUMBER) ? (long)start_v->num_val : 0;
    long b = (end_v && end_v->type == JSON_NUMBER) ? (long)end_v->num_val : 0;
    return chunk_styles_json(styles, a, b);
}

/* PoP: _utf16_offsets @ plugins/platforms/signal/adapter.py:_utf16_offsets */
json_t *w29_adapter_d36d4e_p3_utf16_offsets(const json_t *text_v) {
    json_t *out = json_array();
    const char *p;
    long acc = 0;
    if (!text_v || text_v->type != JSON_STRING) { json_free(out); return err_obj("TypeError"); }
    json_append(out, json_number(0));
    p = text_v->str_val;
    while (p && *p) {
        long adv;
        unsigned cp = u8_decode(p, &adv);
        p += adv;
        acc += u16_w(cp);
        json_append(out, json_number((double)acc));
    }
    return out;
}

/* PoP: _split_signal_formatted_message @ plugins/platforms/signal/adapter.py:_split_signal_formatted_message */
json_t *w29_adapter_d36d4e_p3_split_signal_message(const json_t *text_v, const json_t *styles,
                                                   const json_t *max_v) {
    const char *text;
    long total16, max, body, ncp, total, start, nr = 0, i;
    long *off;
    long *ra, *rb;
    json_t *out = json_array();
    if (!text_v || text_v->type != JSON_STRING) { json_free(out); return err_obj("TypeError"); }
    text = text_v->str_val ? text_v->str_val : "";
    max = (max_v && max_v->type == JSON_NUMBER) ? (long)max_v->num_val : 0;
    total16 = u16_len(text);
    if (total16 <= max) {
        json_t *pair = json_array();
        json_append(pair, json_string(text));
        json_append(pair, styles ? json_copy(styles) : json_array());
        json_append(out, pair);
        return out;
    }
    body = max - 10;
    if (body < 1) body = 1;
    ncp = u8_count(text);
    off = (long *)malloc(((size_t)ncp + 1) * sizeof(long));
    ra = (long *)malloc(((size_t)ncp + 1) * sizeof(long));
    rb = (long *)malloc(((size_t)ncp + 1) * sizeof(long));
    if (!off || !ra || !rb) { free(off); free(ra); free(rb); return out; }
    off[0] = 0;
    {
        const char *p = text;
        long idx = 0, acc = 0;
        while (p && *p) {
            long adv;
            unsigned cp = u8_decode(p, &adv);
            p += adv;
            acc += u16_w(cp);
            off[++idx] = acc;
        }
    }
    total = off[ncp];
    start = 0;
    while (off[start] < total) {
        long budget = off[start] + body, end;
        if (budget > total) budget = total;
        end = start + 1;
        while (end <= ncp && off[end] <= budget) end++;
        end = end - 1;
        if (end < start + 1) end = start + 1;
        ra[nr] = start;
        rb[nr] = end;
        nr++;
        start = end;
    }
    for (i = 0; i < nr; i++) {
        json_t *pair = json_array();
        char *chunk = u8_slice(text, ra[i], rb[i]);
        json_t *st = chunk_styles_json(styles, off[ra[i]], off[rb[i]]);
        if (nr > 1) {
            char suffix[48];
            sb_t b;
            snprintf(suffix, sizeof(suffix), " (%ld/%ld)", i + 1, nr);
            sb_init(&b);
            sb_add(&b, chunk ? chunk : "");
            sb_add(&b, suffix);
            free(chunk);
            chunk = sb_take(&b);
        }
        json_append(pair, json_string(chunk ? chunk : ""));
        json_append(pair, st);
        json_append(out, pair);
        free(chunk);
    }
    free(off);
    free(ra);
    free(rb);
    return out;
}

/* PoP: _with_target @ plugins/platforms/signal/adapter.py:_with_target (resolve=False path) */
json_t *w29_adapter_d36d4e_p3_with_target(const json_t *chat_id, const json_t *account) {
    json_t *out = json_object();
    const char *cid = (chat_id && chat_id->type == JSON_STRING) ? chat_id->str_val : NULL;
    if (!cid) { json_free(out); return err_obj("AttributeError"); }
    json_set(out, "account", account ? json_copy(account) : json_null());
    if (starts_with(cid, "group:")) {
        json_set(out, "groupId", json_string(cid + 6));
    } else {
        json_t *rec = json_array();
        json_append(rec, json_string(cid));
        json_set(out, "recipient", rec);
    }
    return out;
}

/* PoP: _validate_send_result @ plugins/platforms/signal/adapter.py:_validate_send_result */
json_t *w29_adapter_d36d4e_p3_validate_send_result(const json_t *result) {
    json_t *out = json_object();
    const json_t *results = (result && result->type == JSON_OBJECT) ? json_obj_get(result, "results") : NULL;
    bool success = true;
    const char *err = NULL;
    char *err_owned = NULL;
    size_t i;
    if (results && results->type == JSON_ARRAY)
        for (i = 0; i < results->c.count && success; i++) {
            const json_t *r = results->c.items[i];
            const json_t *rtype;
            if (!r || r->type != JSON_OBJECT) continue;
            rtype = json_obj_get(r, "type");
            if (jtruthy(rtype) && !jequal_str(rtype, "SUCCESS")) {
                success = false;
                err_owned = py_str(rtype);
                err = err_owned;
                break;
            }
            if (json_obj_get(r, "success") && !jtruthy(json_obj_get(r, "success"))) {
                const json_t *f = json_obj_get(r, "failure");
                success = false;
                err_owned = jtruthy(f) ? py_str(f) : dup_str("Recipient delivery failed");
                err = err_owned;
                break;
            }
        }
    json_set(out, "success", json_bool(success));
    json_set(out, "error", err ? json_string(err) : json_null());
    free(err_owned);
    return out;
}

/* PoP: _track_sent_timestamp @ plugins/platforms/signal/adapter.py:_track_sent_timestamp */
json_t *w29_adapter_d36d4e_p3_track_sent_timestamp(const json_t *result, const json_t *recent,
                                                   const json_t *sent_cache, const json_t *now_v,
                                                   const json_t *ttl_v, const json_t *max_recent_v,
                                                   const json_t *max_sent_v) {
    json_t *out = json_object();
    json_t *rec = (recent && recent->type == JSON_OBJECT) ? json_copy(recent) : json_object();
    json_t *sc = json_array();
    const json_t *ts = (result && result->type == JSON_OBJECT) ? json_obj_get(result, "timestamp") : NULL;
    double now = (now_v && now_v->type == JSON_NUMBER) ? now_v->num_val : 0.0;
    double ttl = (ttl_v && ttl_v->type == JSON_NUMBER) ? ttl_v->num_val : 300.0;
    long max_recent = (max_recent_v && max_recent_v->type == JSON_NUMBER) ? (long)max_recent_v->num_val : 512;
    size_t i;
    json_t *cache_keys = jtruthy(ts)
        ? w29_adapter_d36d4e_p2_remember_sent_message_timestamp(sent_cache, ts, max_sent_v)
        : (sent_cache && sent_cache->type == JSON_ARRAY ? json_copy(sent_cache) : json_array());
    for (i = 0; i < cache_keys->c.count; i++) json_append(sc, json_copy(cache_keys->c.items[i]));
    json_free(cache_keys);
    if (jtruthy(ts)) {
        char *key = canon_key(ts);
        json_obj_del(rec, key);
        json_set(rec, key, json_number(now));
        free(key);
        while (rec->c.count > 0) {
            const json_t *first = rec->c.items[0];
            if (!(first && first->type == JSON_NUMBER && first->num_val < now - ttl)) break;
            json_obj_del(rec, rec->c.keys[0]);
        }
        while ((long)rec->c.count > max_recent && rec->c.count > 0)
            json_obj_del(rec, rec->c.keys[0]);
    }
    {
        json_t *pairs = json_array();
        for (i = 0; i < rec->c.count; i++) {
            json_t *pair = json_array();
            json_append(pair, json_string(rec->c.keys[i]));
            json_append(pair, json_copy(rec->c.items[i]));
            json_append(pairs, pair);
        }
        json_set(out, "recent", pairs);
    }
    json_set(out, "sent_cache", sc);
    json_free(rec);
    return out;
}

/* PoP: send_typing backoff @ plugins/platforms/signal/adapter.py:send_typing
 * Input is the PRIOR consecutive-failure count; send_typing increments it
 * before applying  min(60, 16 * 2 ** (fails - 3)). */
json_t *w29_adapter_d36d4e_p3_typing_skip_seconds(const json_t *fails_v) {
    long fails;
    double v, p;
    if (!fails_v || fails_v->type != JSON_NUMBER) return err_obj("TypeError");
    fails = (long)fails_v->num_val + 1;
    if (fails < 3) return json_null();
    p = 16.0;
    {
        long k = fails - 3, i;
        for (i = 0; i < k; i++) p *= 2.0;
    }
    v = p < 60.0 ? p : 60.0;
    return json_number(v);
}

/* PoP: _extract_reaction_target @ plugins/platforms/signal/adapter.py:_extract_reaction_target */
json_t *w29_adapter_d36d4e_p3_extract_reaction_target(const json_t *raw) {
    const json_t *sender, *ts;
    if (!raw || raw->type != JSON_OBJECT) return json_null();
    sender = json_obj_get(raw, "sender");
    ts = json_obj_get(raw, "timestamp_ms");
    if (jtruthy(sender) && jtruthy(ts)) {
        json_t *out = json_array();
        json_append(out, json_copy(sender));
        json_append(out, json_copy(ts));
        return out;
    }
    return json_null();
}

/* PoP: send_reaction/remove_reaction @ plugins/platforms/signal/adapter.py:send_reaction,remove_reaction */
json_t *w29_adapter_d36d4e_p3_reaction_params(const json_t *account, const json_t *chat_id,
                                              const json_t *emoji, const json_t *author,
                                              const json_t *ts, const json_t *remove) {
    json_t *out = json_object();
    const char *cid = (chat_id && chat_id->type == JSON_STRING) ? chat_id->str_val : NULL;
    if (!cid) { json_free(out); return err_obj("AttributeError"); }
    json_set(out, "account", account ? json_copy(account) : json_null());
    json_set(out, "emoji", jtruthy(remove) ? json_string("") : (emoji ? json_copy(emoji) : json_string("")));
    json_set(out, "targetAuthor", author ? json_copy(author) : json_null());
    json_set(out, "targetTimestamp", ts ? json_copy(ts) : json_null());
    if (jtruthy(remove)) json_set(out, "remove", json_bool(true));
    if (starts_with(cid, "group:")) json_set(out, "groupId", json_string(cid + 6));
    else {
        json_t *rec = json_array();
        json_append(rec, json_string(cid));
        json_set(out, "recipient", rec);
    }
    return out;
}

/* PoP: send_image error map @ plugins/platforms/signal/adapter.py:send_image */
/* image error text needs the detail value for download/oversize */
json_t *w29_adapter_d36d4e_p3_image_error_text(const json_t *reason, const json_t *detail) {
    const char *r = (reason && reason->type == JSON_STRING) ? reason->str_val : NULL;
    /* `if reason:` guards the whole error map: an EMPTY reason skips it and
     * send_image goes on to _send_file with its own failure text (the KeyError arm
     * is only for a non-empty reason the map does not know). */
    if (!r) return json_null();
    if (!r[0]) return json_string("RPC send with attachment failed");
    if (!strcmp(r, "missing")) return json_string("Image file not found");
    if (!strcmp(r, "download")) {
        char *d = py_str(detail);
        json_t *out = json_string(d ? d : "");
        free(d);
        return out;
    }
    if (!strcmp(r, "oversize")) {
        char *d = py_str(detail);
        sb_t b;
        sb_init(&b);
        sb_add(&b, "Image too large (");
        sb_add(&b, d ? d : "");
        sb_add(&b, " bytes)");
        free(d);
        {
            char *txt = sb_take(&b);
            json_t *out = json_string(txt ? txt : "");
            free(txt);
            return out;
        }
    }
    return err_obj("KeyError");
}

/* PoP: get_chat_info @ plugins/platforms/signal/adapter.py:get_chat_info */
json_t *w29_adapter_d36d4e_p3_chat_info(const json_t *chat_id, const json_t *contact) {
    const char *cid = (chat_id && chat_id->type == JSON_STRING) ? chat_id->str_val : NULL;
    json_t *out = json_object();
    if (!cid) { json_free(out); return err_obj("AttributeError"); }
    if (starts_with(cid, "group:")) {
        json_set(out, "name", json_string(cid));
        json_set(out, "type", json_string("group"));
        json_set(out, "chat_id", json_string(cid));
        return out;
    }
    {
        const json_t *nm = NULL;
        char *name = NULL;
        if (contact && contact->type == JSON_OBJECT) {
            nm = json_obj_get(contact, "name");
            if (!jtruthy(nm)) nm = json_obj_get(contact, "profileName");
            if (jtruthy(nm)) name = py_str(nm);
        }
        json_set(out, "name", json_string(name ? name : cid));
        json_set(out, "type", json_string("dm"));
        json_set(out, "chat_id", json_string(cid));
        free(name);
    }
    return out;
}


