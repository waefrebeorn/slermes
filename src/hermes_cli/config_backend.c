/*
 * config_backend.c — C home for upstream Hermes hermes_cli/config_backend.py.
 *
 * PoP: w12_configback_32e61c_version_token @ hermes_cli/config_backend.py:version_token
 * PoP: w12_configback_32e61c_select       @ hermes_cli/config_backend.py:get_config_backend
 * PoP: w12_configback_32e61c_locked_error @ hermes_cli/config_backend.py:ConfigLockedError.__init__
 *
 * Pure helpers from the config-backend seam. No filesystem / env access: the
 * backend-kind environment value is injected as an explicit argument.
 */
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_configback_32e61c.h"

/* ---------- Python-repr of a tuple of simple JSON scalars (for crc32) ------- */

static void repr_str(const char *s, char *out, size_t cap, size_t *n) {
    char q = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
    if (*n < cap) out[(*n)++] = q;
    for (; *s && *n + 1 < cap; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == (unsigned char)q) {
            if (*n + 2 < cap) { out[(*n)++] = '\\'; out[(*n)++] = (char)c; }
        } else if (c == '\n') { if (*n + 2 < cap) { out[(*n)++] = '\\'; out[(*n)++] = 'n'; } }
        else if (c == '\t') { if (*n + 2 < cap) { out[(*n)++] = '\\'; out[(*n)++] = 't'; } }
        else if (c == '\r') { if (*n + 2 < cap) { out[(*n)++] = '\\'; out[(*n)++] = 'r'; } }
        else out[(*n)++] = (char)c;
    }
    if (*n < cap) out[(*n)++] = q;
}

static void repr_val(const json_t *v, char *out, size_t cap, size_t *n) {
    if (!v || v->type == JSON_NULL) {
        const char *s = "None"; while (*s && *n + 1 < cap) out[(*n)++] = *s++;
    } else if (v->type == JSON_BOOL) {
        const char *s = v->bool_val ? "True" : "False";
        while (*s && *n + 1 < cap) out[(*n)++] = *s++;
    } else if (v->type == JSON_STRING) {
        repr_str(v->str_val ? v->str_val : "", out, cap, n);
    } else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        char tmp[64];
        if (d == (double)(long long)d && fabs(d) < 9.0e18) snprintf(tmp, sizeof tmp, "%.0f", d);
        else snprintf(tmp, sizeof tmp, "%.17g", d);
        for (const char *p = tmp; *p && *n + 1 < cap; ++p) out[(*n)++] = *p;
    }
}

/* repr(tuple(version)) into out. */
static void tuple_repr(const json_t *version, char *out, size_t cap) {
    size_t n = 0;
    if (n < cap) out[n++] = '(';
    size_t count = (version && version->type == JSON_ARRAY) ? json_len(version) : 0;
    for (size_t i = 0; i < count; ++i) {
        if (i) { if (n + 2 < cap) { out[n++] = ','; out[n++] = ' '; } }
        repr_val(json_get(version, i), out, cap, &n);
    }
    if (count == 1) { if (n + 1 < cap) out[n++] = ','; }
    if (n + 1 < cap) out[n++] = ')';
    out[n] = 0;
}

/* zlib.crc32 (IEEE, reflected) — matches Python zlib.crc32 exactly. */
static uint32_t crc32_bytes(const unsigned char *data, size_t len) {
    static uint32_t table[256];
    static int ready = 0;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = 1;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

json_t *w12_configback_32e61c_version_token(const json_t *version) {
    if (!version || version->type != JSON_ARRAY || json_len(version) == 0) return json_number(0);
    const json_t *first = json_get(version, 0);
    /* isinstance(version[0], int): JSON integral number, or bool (True/False). */
    if (first && ((first->type == JSON_NUMBER &&
                   first->num_val == (double)(long long)first->num_val) ||
                  first->type == JSON_BOOL)) {
        double lead = (first->type == JSON_BOOL) ? (first->bool_val ? 1.0 : 0.0) : first->num_val;
        return json_number(lead / 1000000000.0);
    }
    char repr[4096];
    tuple_repr(version, repr, sizeof repr);
    uint32_t crc = crc32_bytes((const unsigned char *)repr, strlen(repr));
    if (crc == 0) crc = 1;  /* `zlib.crc32(...) or 1` */
    return json_number((double)crc);
}

static int kind_ok(const char *k) {
    size_t n = strlen(k);
    if (n < 1 || n > 64) return 0;
    if (!((k[0] >= 'a' && k[0] <= 'z'))) return 0;
    for (size_t i = 1; i < n; ++i) {
        char c = k[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

json_t *w12_configback_32e61c_select(const char *env_value) {
    const char *s = env_value ? env_value : "";
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char kind[256];
    size_t k = 0;
    for (size_t i = 0; i < n && k + 1 < sizeof kind; ++i) kind[k++] = (char)tolower((unsigned char)s[i]);
    kind[k] = 0;
    if (k == 0) { kind[0] = 'f'; kind[1] = 'i'; kind[2] = 'l'; kind[3] = 'e'; kind[4] = 0; }
    int is_file = (strcmp(kind, "file") == 0);
    int valid = is_file || kind_ok(kind);
    json_t *out = json_object();
    json_set(out, "kind", json_string(kind));
    json_set(out, "file", json_bool(is_file));
    json_set(out, "valid", json_bool(valid));
    return out;
}

json_t *w12_configback_32e61c_locked_error(const char *path, const char *locked_by) {
    char msg[2048];
    snprintf(msg, sizeof msg, "%s is locked by the %s level",
             path ? path : "", locked_by ? locked_by : "");
    return json_string(msg);
}
