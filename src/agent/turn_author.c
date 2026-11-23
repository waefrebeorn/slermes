/*
 * turn_author.c — C home for upstream Hermes agent/turn_author.py.
 *
 * PoP: w6_turnauthor_9c0bab_env_key @ agent/turn_author.py:TURN_AUTHOR_ENV
 * PoP: w6_turnauthor_9c0bab_clean_text @ agent/turn_author.py:_clean_text
 * PoP: w6_turnauthor_9c0bab_bot_flag @ agent/turn_author.py:_bot_flag
 * PoP: w6_turnauthor_9c0bab_bot_author_id @ agent/turn_author.py:bot_author_id
 * PoP: w6_turnauthor_9c0bab_parse_turn_author @ agent/turn_author.py:parse_turn_author
 * PoP: w6_turnauthor_9c0bab_turn_author_env @ agent/turn_author.py:turn_author_env
 * PoP: w6_turnauthor_9c0bab_a2a_key @ agent/turn_author.py:a2a_key
 *
 * Caveat: Python's unicodedata category "Cn" (unassigned) varies by Unicode
 * version and is not tracked; Cc/Cs/Co are filtered exactly.  JSON numbers are
 * doubles here, so an integral JSON_NUMBER is treated as a Python int (a float
 * such as 1.0 passed to _bot_flag would differ; fixtures use ints).
 */
#include "noc_w6_turnauthor_9c0bab.h"
#include "hermes_json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELD_LEN 200
#define MAX_CHAIN_HOPS 1000

const char *w6_turnauthor_9c0bab_env_key(void) { return "HERMES_TURN_AUTHOR"; }

/* ---- code point helpers ------------------------------------------------ */

static uint32_t u8dec(const unsigned char *s, size_t len, size_t *i)
{
    unsigned char c = s[*i];
    uint32_t cp;
    int need;
    if (c < 0x80) { (*i)++; return c; }
    if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1Fu; }
    else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0Fu; }
    else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07u; }
    else { (*i)++; return c; }
    if (*i + (size_t)need >= len) { (*i)++; return c; }
    for (int k = 1; k <= need; k++) {
        unsigned char cc = s[*i + (size_t)k];
        if ((cc & 0xC0) != 0x80) { (*i)++; return c; }
        cp = (cp << 6) | (uint32_t)(cc & 0x3F);
    }
    *i += (size_t)need + 1;
    return cp;
}

static void u8enc(uint32_t cp, char *out, size_t *j)
{
    if (cp < 0x80) out[(*j)++] = (char)cp;
    else if (cp < 0x800) {
        out[(*j)++] = (char)(0xC0 | (cp >> 6));
        out[(*j)++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out[(*j)++] = (char)(0xE0 | (cp >> 12));
        out[(*j)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*j)++] = (char)(0x80 | (cp & 0x3F));
    } else {
        out[(*j)++] = (char)(0xF0 | (cp >> 18));
        out[(*j)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[(*j)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*j)++] = (char)(0x80 | (cp & 0x3F));
    }
}

/* unicodedata.category(ch) in {"Cc", "Cs", "Co"}; Cn is not tracked. */
static int dropped_cp(uint32_t cp)
{
    if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return 1;       /* Cc */
    if (cp >= 0xD800 && cp <= 0xDFFF) return 1;                   /* Cs */
    if (cp >= 0xE000 && cp <= 0xF8FF) return 1;                   /* Co (BMP) */
    if (cp >= 0xF0000 && cp <= 0xFFFFD) return 1;                 /* Co (SMP) */
    if (cp >= 0x100000 && cp <= 0x10FFFD) return 1;               /* Co (SIP) */
    return 0;
}

/* Python str.strip() whitespace set. */
static int space_cp(uint32_t cp)
{
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

static int ascii_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int strncasecmp_local(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (ascii_lower((unsigned char)a[i]) != ascii_lower((unsigned char)b[i])) return 1;
    return 0;
}

char *w6_turnauthor_9c0bab_clean_text(const char *value)
{
    if (!value) return NULL;
    size_t len = strlen(value);
    uint32_t *cps = (uint32_t *)malloc((len + 1) * sizeof(uint32_t));
    if (!cps) return NULL;
    size_t total = 0;
    for (size_t i = 0; i < len;) cps[total++] = u8dec((const unsigned char *)value, len, &i);

    size_t m = 0;
    for (size_t k = 0; k < total; k++) if (!dropped_cp(cps[k])) cps[m++] = cps[k];

    size_t a = 0, b = m;
    while (a < b && space_cp(cps[a])) a++;
    while (b > a && space_cp(cps[b - 1])) b--;
    if (b <= a) { free(cps); return NULL; }
    size_t cnt = b - a;
    if (cnt > MAX_FIELD_LEN) cnt = MAX_FIELD_LEN;   /* text[:200] */

    char *out = (char *)malloc(4 * cnt + 1);
    if (!out) { free(cps); return NULL; }
    size_t j = 0;
    for (size_t k = 0; k < cnt; k++) u8enc(cps[a + k], out, &j);
    out[j] = '\0';
    free(cps);
    return out;
}

/* ---- bot flag ---------------------------------------------------------- */

static int truthy_string(const char *s)
{
    if (!s) return 0;
    while (*s && space_cp((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && space_cp((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    return (n == 4 && strncasecmp_local(s, "true", 4) == 0) ||
           (n == 1 && s[0] == '1') ||
           (n == 3 && strncasecmp_local(s, "yes", 3) == 0);
}

int w6_turnauthor_9c0bab_bot_flag(const struct json_t *value)
{
    if (!value) return 0;
    if (value->type == JSON_STRING) return truthy_string(value->str_val);
    if (value->type == JSON_BOOL) return value->bool_val ? 1 : 0;
    if (value->type == JSON_NUMBER) return value->num_val != 0.0 ? 1 : 0;
    return 0;
}

char *w6_turnauthor_9c0bab_bot_author_id(const char *profile, const char *origin)
{
    const char *o = origin ? origin : "";
    while (*o && space_cp((unsigned char)*o)) o++;
    size_t n = strlen(o);
    while (n > 0 && space_cp((unsigned char)o[n - 1])) n--;
    int has_origin = n > 0;
    const char *p = profile ? profile : "";
    size_t pl = strlen(p);
    char *out = (char *)malloc(5 + (has_origin ? n + 1 : 0) + pl + 1);
    if (!out) return NULL;
    size_t j = 0;
    memcpy(out + j, "bot:", 4); j += 4;
    if (has_origin) { memcpy(out + j, o, n); j += n; out[j++] = '/'; }
    memcpy(out + j, p, pl); j += pl;
    out[j] = '\0';
    return out;
}

/* ---- Python-compatible compact JSON dump (ensure_ascii=True) ---------- */

typedef struct { char *p; size_t len, cap; } buf_t;

static void bneed(buf_t *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap * 2 : 128;
    while (nc < b->len + extra + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}

static void bputn(buf_t *b, const char *s, size_t n)
{
    bneed(b, n);
    if (b->p) { memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0'; }
}

static void bputs(buf_t *b, const char *s) { bputn(b, s, strlen(s)); }

static void bputjstr(buf_t *b, const char *s)
{
    bputs(b, "\"");
    if (!s) { bputs(b, "\""); return; }
    const unsigned char *p = (const unsigned char *)s;
    size_t len = strlen(s);
    for (size_t i = 0; i < len;) {
        unsigned char c = p[i];
        if (c == '"') { bputs(b, "\\\""); i++; continue; }
        if (c == '\\') { bputs(b, "\\\\"); i++; continue; }
        if (c == '\n') { bputs(b, "\\n"); i++; continue; }
        if (c == '\r') { bputs(b, "\\r"); i++; continue; }
        if (c == '\t') { bputs(b, "\\t"); i++; continue; }
        if (c == '\b') { bputs(b, "\\b"); i++; continue; }
        if (c == '\f') { bputs(b, "\\f"); i++; continue; }
        if (c < 0x20) {
            char tmp[8]; snprintf(tmp, sizeof tmp, "\\u%04x", c); bputs(b, tmp); i++; continue;
        }
        if (c < 0x80) { bputn(b, (const char *)&c, 1); i++; continue; }
        uint32_t cp = u8dec(p, len, &i);
        char tmp[16];
        if (cp <= 0xFFFF) {
            snprintf(tmp, sizeof tmp, "\\u%04x", (unsigned)cp);
            bputs(b, tmp);
        } else {
            cp -= 0x10000;
            snprintf(tmp, sizeof tmp, "\\u%04x\\u%04x",
                     (unsigned)(0xD800 + (cp >> 10)), (unsigned)(0xDC00 + (cp & 0x3FF)));
            bputs(b, tmp);
        }
    }
    bputs(b, "\"");
}

static void bdump(buf_t *b, const struct json_t *v)
{
    if (!v || v->type == JSON_NULL) { bputs(b, "null"); return; }
    switch (v->type) {
    case JSON_BOOL: bputs(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: {
        char tmp[64];
        snprintf(tmp, sizeof tmp, "%.17g", v->num_val);
        bputs(b, tmp);
        break;
    }
    case JSON_STRING: bputjstr(b, v->str_val); break;
    case JSON_ARRAY: {
        bputs(b, "[");
        size_t n = json_len(v);
        for (size_t i = 0; i < n; i++) { if (i) bputs(b, ","); bdump(b, json_get(v, i)); }
        bputs(b, "]");
        break;
    }
    case JSON_OBJECT: {
        bputs(b, "{");
        size_t n = json_object_size(v);
        for (size_t i = 0; i < n; i++) {
            if (i) bputs(b, ",");
            bputjstr(b, json_object_get_key_at(v, i));
            bputs(b, ":");
            bdump(b, json_object_get_at(v, i));
        }
        bputs(b, "}");
        break;
    }
    default: bputs(b, "null"); break;
    }
}

static char *compact_dump(const struct json_t *v)
{
    buf_t b = {0};
    bdump(&b, v);
    return b.p ? b.p : (char *)calloc(1, 1);
}

/* ---- parse_turn_author -------------------------------------------------- */

struct json_t *w6_turnauthor_9c0bab_parse_turn_author(const struct json_t *raw)
{
    const struct json_t *src = raw;
    struct json_t *parsed = NULL;
    if (raw && raw->type == JSON_STRING) {
        parsed = json_parse(raw->str_val, NULL);
        src = parsed;
    }
    if (!src || src->type != JSON_OBJECT) { json_free(parsed); return json_null(); }

    const struct json_t *jv;
    char *id = NULL, *name = NULL, *origin = NULL;
    jv = json_obj_get(src, "id");
    if (jv && jv->type == JSON_STRING) id = w6_turnauthor_9c0bab_clean_text(jv->str_val);
    jv = json_obj_get(src, "name");
    if (jv && jv->type == JSON_STRING) name = w6_turnauthor_9c0bab_clean_text(jv->str_val);

    int isbot = w6_turnauthor_9c0bab_bot_flag(json_obj_get(src, "is_bot"));

    if (!id && !name) {
        free(id); free(name); json_free(parsed);
        return json_null();
    }

    jv = json_obj_get(src, "hops");
    int has_hops = 0;
    double hops = 0;
    if (jv && jv->type == JSON_NUMBER && jv->num_val > 0 && jv->num_val <= MAX_CHAIN_HOPS &&
        jv->num_val == (double)(long long)jv->num_val) {
        has_hops = 1; hops = jv->num_val;
    }

    jv = json_obj_get(src, "origin");
    if (jv && jv->type == JSON_STRING) origin = w6_turnauthor_9c0bab_clean_text(jv->str_val);

    if (origin && id && strncmp(id, "bot:", 4) == 0 && !strchr(id + 4, '/')) {
        char *qual = w6_turnauthor_9c0bab_bot_author_id(id + 4, origin);
        char *clean = w6_turnauthor_9c0bab_clean_text(qual);
        free(qual);
        if (clean) { free(id); id = clean; }
    }

    struct json_t *out = json_object();
    json_set(out, "id", id ? json_string(id) : json_null());
    json_set(out, "name", name ? json_string(name) : json_null());
    json_set(out, "is_bot", json_bool(isbot != 0));
    if (has_hops) json_set(out, "hops", json_number(hops));

    free(id); free(name); free(origin); json_free(parsed);
    return out;
}

struct json_t *w6_turnauthor_9c0bab_turn_author_env(const struct json_t *author)
{
    char *s = compact_dump(author);
    struct json_t *out = json_object();
    json_set(out, w6_turnauthor_9c0bab_env_key(), json_string(s));
    free(s);
    return out;
}

char *w6_turnauthor_9c0bab_a2a_key(const struct json_t *author)
{
    if (!author || author->type != JSON_OBJECT) return NULL;
    const struct json_t *b = json_obj_get(author, "is_bot");
    const struct json_t *i = json_obj_get(author, "id");
    int bot = 0, hasid = 0;
    if (b) {
        if (b->type == JSON_BOOL) bot = b->bool_val ? 1 : 0;
        else if (b->type == JSON_NUMBER) bot = b->num_val != 0.0;
        else if (b->type == JSON_STRING) bot = b->str_val && b->str_val[0] != '\0';
    }
    if (i && i->type == JSON_STRING && i->str_val && i->str_val[0]) hasid = 1;
    if (!bot || !hasid) return NULL;
    size_t n = strlen(i->str_val);
    char *out = (char *)malloc(4 + n + 1);
    if (!out) return NULL;
    memcpy(out, "a2a:", 4);
    memcpy(out + 4, i->str_val, n);
    out[4 + n] = '\0';
    return out;
}
