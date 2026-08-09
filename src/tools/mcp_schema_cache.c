/*
 * mcp_schema_cache.c — C11 port of tools/mcp_schema_cache.py.
 *
 * Persistent MCP tool-schema cache for lazy server startup.  All functions
 * are pure I/O + hashing + JSON: no async, no DB, no HTTP.
 *
 * Reuses: libcrypto (crypto_sha256 + crypto_hex_encode), libjson,
 *         slermes_home (slermes_home), port_utils_ports (util_atomic_json_write).
 */

#define _POSIX_C_SOURCE 200809L
#include "mcp_schema_cache.h"
#include <json.h>
#include <crypto.h>
#include <port_utils_ports.h>
#include <slermes_home.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#define CACHE_FILENAME "mcp_schema_cache.json"

/* _cache_lock — module-global mutex (mirrors Python threading.Lock). */
static pthread_mutex_t _cache_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── json helpers (libjson lacks deep-clone + sorted serialize) ─────────── */

static json_t *json_obj_clone_deep(const json_t *n)
{
    char *s = json_serialize(n);
    if (!s) return NULL;
    char *err = NULL;
    json_t *c = json_parse(s, &err);
    free(s);
    if (err) free(err);
    return c;
}

static void mcp_json_sort_strings(json_t *arr)
{
    if (!arr || arr->type != JSON_ARRAY || arr->c.count < 2) return;
    /* Python sorted() on strings — lexbyte order. */
    for (size_t i = 0; i < arr->c.count; i++)
        for (size_t j = i + 1; j < arr->c.count; j++) {
            const char *a = (arr->c.items[i] && arr->c.items[i]->type == JSON_STRING)
                                ? arr->c.items[i]->str_val : "";
            const char *b = (arr->c.items[j] && arr->c.items[j]->type == JSON_STRING)
                                ? arr->c.items[j]->str_val : "";
            if (strcmp(a, b) > 0) {
                json_t *t = arr->c.items[i];
                arr->c.items[i] = arr->c.items[j];
                arr->c.items[j] = t;
            }
        }
}

/* ── compact sorted-key serializer (json.dumps sort_keys=True, sep=(",",":")) */

typedef struct { char *buf; size_t len, cap; } buf_t;

static void buf_init(buf_t *b)
{
    b->cap = 64; b->len = 0;
    b->buf = malloc(b->cap);
    b->buf[0] = 0;
}

static void buf_reserve(buf_t *b, size_t add)
{
    while (b->len + add + 1 > b->cap) b->cap *= 2;
    b->buf = realloc(b->buf, b->cap);
}

static void buf_put(buf_t *b, const char *s, size_t n)
{
    buf_reserve(b, n);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = 0;
}

static void buf_putc(buf_t *b, char c) { buf_put(b, &c, 1); }

static void buf_puts(buf_t *b, const char *s) { buf_put(b, s, strlen(s)); }

/* Emit a JSON-escaped string (with surrounding quotes), matching Python's
 * json.dumps for str.  Reuse libjson's serialize on a string node. */
static void ser_str_escaped(buf_t *b, const char *s)
{
    json_t *t = json_string(s);
    char *esc = t ? json_serialize(t) : NULL;  /* "s" with escapes */
    if (esc) {
        /* esc = "...."; emit without surrounding quotes, then re-add. */
        size_t el = strlen(esc);
        if (el >= 2) {
            b->buf[b->len] = '"'; b->len++; b->buf[b->len] = 0;
            buf_put(b, esc + 1, el - 2);
            buf_putc(b, '"');
        }
        free(esc);
    } else {
        buf_putc(b, '"'); buf_puts(b, s); buf_putc(b, '"');
    }
    json_free(t);
}

static void ser_value(buf_t *b, const json_t *n);

static void ser_value(buf_t *b, const json_t *n)
{
    if (!n) { buf_puts(b, "null"); return; }
    switch (n->type) {
    case JSON_NULL:    buf_puts(b, "null"); break;
    case JSON_BOOL:    buf_puts(b, n->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: {
        double d = n->num_val;
        long long i = (long long)d;
        /* Python json: integer-valued floats serialize with a ".0" only if
         * the original was a float.  config_fingerprint payload only has
         * strings/arrays/None — no numbers in practice.  Emit integers
         * without fractional part when exact. */
        if ((double)i == d && d >= -9e15 && d <= 9e15) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%lld", i);
            buf_puts(b, tmp);
        } else {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%g", d);
            buf_puts(b, tmp);
        }
        break;
    }
    case JSON_STRING:  ser_str_escaped(b, n->str_val); break;
    case JSON_ARRAY: {
        buf_putc(b, '[');
        for (size_t i = 0; i < n->c.count; i++) {
            if (i) buf_putc(b, ',');
            ser_value(b, n->c.items[i]);
        }
        buf_putc(b, ']');
        break;
    }
    case JSON_OBJECT: {
        buf_putc(b, '{');
        /* sort keys via index array (selection sort — objects are small). */
        size_t nkeys = n->c.count;
        if (nkeys == 0) { buf_putc(b, '}'); break; }
        size_t *idx = malloc(sizeof(size_t) * nkeys);
        for (size_t i = 0; i < nkeys; i++) idx[i] = i;
        for (size_t i = 0; i < nkeys; i++)
            for (size_t j = i + 1; j < nkeys; j++)
                if (strcmp(n->c.keys[idx[i]], n->c.keys[idx[j]]) > 0) {
                    size_t t = idx[i]; idx[i] = idx[j]; idx[j] = t;
                }
        for (size_t i = 0; i < nkeys; i++) {
            size_t k = idx[i];
            if (i) buf_putc(b, ',');
            buf_putc(b, '"'); buf_puts(b, n->c.keys[k]); buf_putc(b, '"');
            buf_putc(b, ':');
            ser_value(b, n->c.items[k]);
        }
        free(idx);
        buf_putc(b, '}');
        break;
    }
    }
}

static char *mcp_json_serialize_sorted(const json_t *n)
{
    buf_t b;
    buf_init(&b);
    ser_value(&b, n);
    return b.buf;
}

/* ── cache_path ──────────────────────────────────────────────────────────── */
/* PoP: _cache_path @ tools/mcp_schema_cache.py:_cache_path */
char *mcp_cache_path(void)
{
    const char *home = slermes_home();
    if (!home) return NULL;
    size_t n = strlen(home) + 1 + strlen("cache") + 1 + strlen(CACHE_FILENAME) + 1;
    char *path = malloc(n);
    if (!path) return NULL;
    snprintf(path, n, "%s/cache/%s", home, CACHE_FILENAME);
    return path;
}

/* ── config_fingerprint ──────────────────────────────────────────────────── */
/* PoP: config_fingerprint @ tools/mcp_schema_cache.py:config_fingerprint */
char *mcp_config_fingerprint(const json_t *config)
{
    if (!config) return NULL;
    json_t *payload = json_object();
    json_t *t = json_obj_get(config, "command");
    if (t) json_set(payload, "command", json_obj_clone_deep(t));
    else   json_set(payload, "command", json_null());   /* Python: config.get("command") -> None */

    json_t *a = json_obj_get(config, "args");
    if (a && a->type == JSON_ARRAY)
        json_set(payload, "args", json_obj_clone_deep(a));
    else
        json_set(payload, "args", json_array());

    json_t *u = json_obj_get(config, "url");
    if (u) json_set(payload, "url", json_obj_clone_deep(u));
    else   json_set(payload, "url", json_null());

    json_t *tr = json_obj_get(config, "transport");
    if (tr) json_set(payload, "transport", json_obj_clone_deep(tr));
    else   json_set(payload, "transport", json_null());

    json_t *tf = json_obj_get(config, "tools");
    if (tf && tf->type == JSON_OBJECT) {
        json_t *ti = json_obj_get(tf, "include");
        json_t *te = json_obj_get(tf, "exclude");
        if (ti && ti->type == JSON_ARRAY) {
            json_t *sorted = json_array();
            for (size_t i = 0; i < ti->c.count; i++)
                json_append(sorted, json_obj_clone_deep(ti->c.items[i]));
            mcp_json_sort_strings(sorted);
            json_set(payload, "tools_include", sorted);
        } else {
            json_set(payload, "tools_include", json_array());
        }
        if (te && te->type == JSON_ARRAY) {
            json_t *sorted = json_array();
            for (size_t i = 0; i < te->c.count; i++)
                json_append(sorted, json_obj_clone_deep(te->c.items[i]));
            mcp_json_sort_strings(sorted);
            json_set(payload, "tools_exclude", sorted);
        } else {
            json_set(payload, "tools_exclude", json_array());
        }
    } else {
        json_set(payload, "tools_include", json_array());
        json_set(payload, "tools_exclude", json_array());
    }

    char *raw = mcp_json_serialize_sorted(payload);
    json_free(payload);
    if (!raw) return NULL;

    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)raw, strlen(raw), hash);
    free(raw);

    char *hex = crypto_hex_encode(hash, CRYPTO_SHA256_LEN);
    if (!hex) return NULL;
    char *fp = malloc(17);
    if (!fp) { free(hex); return NULL; }
    snprintf(fp, 17, "%.16s", hex);
    free(hex);
    return fp;
}

/* ── _load_all / _save_all ───────────────────────────────────────────────── */
/* PoP: _load_all @ tools/mcp_schema_cache.py:_load_all */
json_t *mcp_load_all(void)
{
    char *path = mcp_cache_path();
    if (!path) return json_object();
    struct stat st;
    if (stat(path, &st) != 0) { free(path); return json_object(); }
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return json_object();
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return json_object(); }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = 0;
    char *err = NULL;
    json_t *data = json_parse(buf, &err);
    free(buf);
    if (err) free(err);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return json_object();
    }
    return data;
}

/* PoP: _save_all @ tools/mcp_schema_cache.py:_save_all */
/* Python: atomic_json_write(path, data, mode=0o600).  C uses
 * util_atomic_json_write (indent=2) + chmod 0600. */
void mcp_save_all(const json_t *data)
{
    char *path = mcp_cache_path();
    if (!path) return;
    char *dir = strdup(path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; mkdir(dir, 0700); }
    free(dir);
    char *ser = json_serialize_pretty(data, 2);
    if (!ser) { free(path); return; }
    util_atomic_json_write(path, ser);
    chmod(path, 0600);
    free(ser);
    free(path);
}

/* ── get_cached_entry / has_cached_entry ─────────────────────────────────── */
/* PoP: get_cached_entry @ tools/mcp_schema_cache.py:get_cached_entry */
json_t *mcp_get_cached_entry(const char *server_name, const char *fingerprint)
{
    pthread_mutex_lock(&_cache_lock);
    json_t *all = mcp_load_all();
    json_t *entry = json_obj_get(all, server_name);
    if (!entry || entry->type != JSON_OBJECT) {
        json_free(all);
        pthread_mutex_unlock(&_cache_lock);
        return NULL;
    }
    json_t *fp = json_obj_get(entry, "fingerprint");
    if (!fp || fp->type != JSON_STRING || strcmp(fp->str_val, fingerprint) != 0) {
        json_free(all);
        pthread_mutex_unlock(&_cache_lock);
        return NULL;
    }
    /* Return an owned copy — Python hands back the live dict (GC); in C we
     * serialize+reparse so the caller owns the result and we can free `all`. */
    char *ser = json_serialize(entry);
    json_free(all);
    pthread_mutex_unlock(&_cache_lock);
    char *err = NULL;
    json_t *copy = ser ? json_parse(ser, &err) : NULL;
    free(ser);
    if (err) free(err);
    return copy;
}

/* PoP: has_cached_entry @ tools/mcp_schema_cache.py:has_cached_entry */
bool mcp_has_cached_entry(const char *server_name, const char *fingerprint)
{
    json_t *e = mcp_get_cached_entry(server_name, fingerprint);
    if (e) { json_free(e); return true; }
    return false;
}

/* ── write_cache_entry / clear_cache_entry ───────────────────────────────── */
/* PoP: write_cache_entry @ tools/mcp_schema_cache.py:write_cache_entry */
void mcp_write_cache_entry(const char *server_name, const char *fingerprint,
                           const json_t *tools, const json_t *utility_tools)
{
    pthread_mutex_lock(&_cache_lock);
    json_t *data = mcp_load_all();
    json_t *entry = json_object();
    json_set(entry, "fingerprint", json_string(fingerprint));
    if (tools && tools->type == JSON_ARRAY)
        json_set(entry, "tools", json_obj_clone_deep(tools));
    else
        json_set(entry, "tools", json_array());
    if (utility_tools && utility_tools->type == JSON_ARRAY)
        json_set(entry, "utility_tools", json_obj_clone_deep(utility_tools));
    else
        json_set(entry, "utility_tools", json_array());

    /* Python: if data.get(server_name) == entry: return (byte-identical skip). */
    json_t *existing = json_obj_get(data, server_name);
    if (existing) {
        char *eser = json_serialize(existing);
        char *nser = json_serialize(entry);
        bool same = (eser && nser && strcmp(eser, nser) == 0);
        free(eser); free(nser);
        if (same) {
            json_free(entry);
            json_free(data);
            pthread_mutex_unlock(&_cache_lock);
            return;
        }
    }
    json_set(data, server_name, entry);
    mcp_save_all(data);
    json_free(data);
    pthread_mutex_unlock(&_cache_lock);
}

/* PoP: clear_cache_entry @ tools/mcp_schema_cache.py:clear_cache_entry */
void mcp_clear_cache_entry(const char *server_name)
{
    pthread_mutex_lock(&_cache_lock);
    json_t *data = mcp_load_all();
    if (json_obj_del(data, server_name))
        mcp_save_all(data);
    json_free(data);
    pthread_mutex_unlock(&_cache_lock);
}

/* ── tools_from_cache_entry / utility_tools_from_cache_entry ─────────────── */
/* PoP: tools_from_cache_entry @ tools/mcp_schema_cache.py:tools_from_cache_entry */
json_t *mcp_tools_from_cache_entry(const json_t *entry)
{
    if (!entry || entry->type != JSON_OBJECT) return json_array();
    json_t *tools = json_obj_get(entry, "tools");
    if (tools && tools->type == JSON_ARRAY)
        return json_obj_clone_deep(tools);
    return json_array();
}

/* PoP: utility_tools_from_cache_entry @ tools/mcp_schema_cache.py:utility_tools_from_cache_entry */
json_t *mcp_utility_tools_from_cache_entry(const json_t *entry)
{
    if (!entry || entry->type != JSON_OBJECT) return json_array();
    json_t *ut = json_obj_get(entry, "utility_tools");
    if (ut && ut->type == JSON_ARRAY)
        return json_obj_clone_deep(ut);
    return json_array();
}
