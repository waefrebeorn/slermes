/*
 * gateway_connections.c — faithful C11 port of hermes_cli/gateway_connections.py
 * (gate w18_gatewaycon_03912e).
 *
 * PoP: w18_gatewaycon_03912e_revision @ hermes_cli/gateway_connections.py:_revision
 * PoP: w18_gatewaycon_03912e_plain @ hermes_cli/gateway_connections.py:_plain
 * PoP: w18_gatewaycon_03912e_envelope @ hermes_cli/gateway_connections.py:_envelope
 * PoP: w18_gatewaycon_03912e_token_preview @ hermes_cli/gateway_connections.py:_token_preview
 * PoP: w18_gatewaycon_03912e_kept_envelope @ hermes_cli/gateway_connections.py:_kept_envelope
 * PoP: w18_gatewaycon_03912e_public_connection @ hermes_cli/gateway_connections.py:_public_connection
 * PoP: w18_gatewaycon_03912e_public_registry @ hermes_cli/gateway_connections.py:public_registry
 * PoP: w18_gatewaycon_03912e_signed_in_ids_gone @ hermes_cli/gateway_connections.py:_signed_in_ids_gone
 * PoP: w18_gatewaycon_03912e_incoming_headers @ hermes_cli/gateway_connections.py:_incoming_headers
 */
#include "noc_w18_gatewaycon_03912e.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *sv(const json_t *v) { return is_str(v) ? v->str_val : NULL; }

/* str.strip() for the whitespace these fields actually encounter. */
static char *strip_ascii(const char *s) {
    size_t a = 0, b;
    char *out;
    if (!s) return NULL;
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* Recursive value equality matching Python == for JSON trees (dict order free). */
static int jeq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (i = 0; i < a->c.count; i++)
                if (!jeq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            n = json_object_size(a);
            for (i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!k || !jeq(json_obj_get(a, k), json_obj_get(b, k))) return 0;
            }
            return 1;
    }
    return 0;
}

/* Python truthiness for the scalar cases this module tests. */
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count != 0;
}

/* _plain(secret): plaintext of a stored envelope; safeStorage reads unset.
 * Returns a malloc'd copy so callers never retain an interior json pointer. */
static char *plain_str(const json_t *secret) {
    const json_t *v;
    const char *enc, *val;
    char *out;
    if (is_str(secret)) {
        const char *s = sv(secret);
        out = (char *)malloc(strlen(s) + 1);
        if (out) strcpy(out, s);
        return out;
    }
    if (!is_obj(secret)) return NULL;
    enc = sv(json_obj_get(secret, "encoding"));
    if (enc && !strcmp(enc, "safeStorage")) return NULL;
    v = json_obj_get(secret, "value");
    val = sv(v);
    if (!val) return NULL;
    out = (char *)malloc(strlen(val) + 1);
    if (out) strcpy(out, val);
    return out;
}

/* _token_preview(value): None for empty, "set" for <=8 chars, "...<last 6>".
 * Counts code points; the tail pointer always starts at a UTF-8 lead byte. */
static json_t *token_preview_str(const char *value) {
    size_t bytes, cps = 0, i, tail_off;
    const unsigned char *p, *start;
    char *out;
    if (!value || !*value) return json_null();
    bytes = strlen(value);
    for (i = 0; i < bytes; i++)
        if (((unsigned char)value[i] & 0xC0) != 0x80) cps++;
    if (cps <= 8) return json_string("set");
    start = (const unsigned char *)value;
    p = start + bytes;
    for (i = 0; i < 6; i++) {
        while (p > start) {
            --p;
            if (((*p) & 0xC0) != 0x80) break;
        }
    }
    tail_off = (size_t)(p - start);
    out = (char *)malloc(bytes - tail_off + 4);
    if (!out) return json_null();
    out[0] = out[1] = out[2] = '.';
    memcpy(out + 3, p, bytes - tail_off);
    out[3 + bytes - tail_off] = '\0';
    { json_t *r = json_string(out); free(out); return r; }
}

/* str(q.get("reason") or "unknown") for public_registry's quarantine map. */
static json_t *reason_or_unknown(const json_t *v) {
    const char *s;
    if (!truthy(v)) return json_string("unknown");
    s = sv(v);
    if (s) return json_string(s);
    if (v->type == JSON_BOOL) return json_string(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", v->num_val);
        return json_string(buf);
    }
    return json_string("unknown");
}

json_t *w18_gatewaycon_03912e_revision(const json_t *doc) {
    const json_t *v = is_obj(doc) ? json_obj_get(doc, "revision") : NULL;
    if (!v || v->type != JSON_NUMBER) return json_number(0.0);
    if (v->num_val != (double)(long long)v->num_val) return json_number(0.0);
    return json_number((double)(long long)v->num_val);
}

json_t *w18_gatewaycon_03912e_plain(const json_t *secret) {
    char *s = plain_str(secret);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

json_t *w18_gatewaycon_03912e_envelope(const char *value) {
    json_t *out = json_object();
    json_set(out, "encoding", json_string("plain"));
    json_set(out, "value", json_string(value ? value : ""));
    return out;
}

json_t *w18_gatewaycon_03912e_token_preview(const char *value) {
    return token_preview_str(value);
}

json_t *w18_gatewaycon_03912e_kept_envelope(const json_t *token, const json_t *existing) {
    const char *enc;
    const json_t *val, *prev;
    if (!is_obj(token)) return json_bool(0);
    enc = sv(json_obj_get(token, "encoding"));
    if (!enc || strcmp(enc, "safeStorage")) return json_bool(0);
    val = json_obj_get(token, "value");
    if (!truthy(val)) return json_bool(0);
    if (!is_obj(existing)) return json_bool(0);
    prev = json_obj_get(existing, "token");
    return json_bool(jeq(prev, token) ? 1 : 0);
}

json_t *w18_gatewaycon_03912e_public_connection(const json_t *entry) {
    json_t *out = json_object();
    char *token;
    size_t i, n;
    if (!is_obj(entry)) return out;
    for (i = 0, n = json_object_size(entry); i < n; i++) {
        const char *k = json_object_get_key_at(entry, i);
        if (!k || !strcmp(k, "token") || !strcmp(k, "headers")) continue;
        json_set(out, k, json_copy(json_object_get_at(entry, i)));
    }
    token = plain_str(json_obj_get(entry, "token"));
    json_set(out, "tokenSet", json_bool(token && token[0] ? 1 : 0));
    json_set(out, "tokenPreview", token_preview_str(token));
    {
        json_t *names = json_array();
        const json_t *h = json_obj_get(entry, "headers");
        if (is_obj(h)) {
            size_t j, m = json_object_size(h);
            for (j = 0; j < m; j++) {
                const char *hk = json_object_get_key_at(h, j);
                if (hk) json_append(names, json_string(hk));
            }
        }
        json_set(out, "headerNames", names);
    }
    free(token);
    return out;
}

json_t *w18_gatewaycon_03912e_public_registry(const json_t *reg) {
    json_t *out = json_object();
    const json_t *v;
    size_t i, n;
    static const char *direct[] = {"version", "primary", "launchMode", "lastUsed"};
    for (i = 0; i < 4; i++) {
        v = is_obj(reg) ? json_obj_get(reg, direct[i]) : NULL;
        json_set(out, direct[i], v ? json_copy(v) : json_null());
    }
    json_set(out, "secureTokenStorage", json_bool(1));
    {
        json_t *conns = json_array();
        const json_t *src = is_obj(reg) ? json_obj_get(reg, "connections") : NULL;
        if (is_arr(src)) {
            for (i = 0, n = json_len(src); i < n; i++)
                json_append(conns, w18_gatewaycon_03912e_public_connection(json_get(src, i)));
        }
        json_set(out, "connections", conns);
    }
    {
        json_t *qs = json_array();
        const json_t *src = is_obj(reg) ? json_obj_get(reg, "quarantined") : NULL;
        if (is_arr(src)) {
            for (i = 0, n = json_len(src); i < n; i++) {
                const json_t *q = json_get(src, i);
                const json_t *ent;
                const char *label = NULL;
                json_t *row;
                if (!is_obj(q)) continue;
                ent = json_obj_get(q, "entry");
                if (is_obj(ent)) label = sv(json_obj_get(ent, "label"));
                row = json_object();
                json_set(row, "reason", reason_or_unknown(json_obj_get(q, "reason")));
                json_set(row, "label", json_string(label ? label : ""));
                json_append(qs, row);
            }
        }
        json_set(out, "quarantined", qs);
    }
    return out;
}

json_t *w18_gatewaycon_03912e_signed_in_ids_gone(const json_t *before, const json_t *after) {
    json_t *out = json_array();
    const json_t *bc = is_obj(before) ? json_obj_get(before, "connections") : NULL;
    const json_t *ac = is_obj(after) ? json_obj_get(after, "connections") : NULL;
    size_t i, j, nb, na;
    static const char *keys[] = {"url", "authMode"};
    nb = is_arr(bc) ? json_len(bc) : 0;
    na = is_arr(ac) ? json_len(ac) : 0;
    for (i = 0; i < nb; i++) {
        const json_t *c = json_get(bc, i);
        const char *cid;
        const json_t *now = NULL;
        int changed = 0;
        if (!is_obj(c)) continue;
        cid = sv(json_obj_get(c, "id"));
        if (!cid) continue;
        for (j = 0; j < na; j++) {
            const json_t *d = json_get(ac, j);
            const char *did = is_obj(d) ? sv(json_obj_get(d, "id")) : NULL;
            if (did && !strcmp(did, cid)) { now = d; break; }
        }
        if (!now) { json_append(out, json_string(cid)); continue; }
        for (j = 0; j < 2; j++)
            if (!jeq(json_obj_get(c, keys[j]), json_obj_get(now, keys[j]))) { changed = 1; break; }
        if (changed) json_append(out, json_string(cid));
    }
    return out;
}

json_t *w18_gatewaycon_03912e_incoming_headers(const json_t *raw, const json_t *existing) {
    json_t *out = json_object();
    size_t i, n;
    const json_t *stored = is_obj(existing) ? existing : NULL;
    if (!is_obj(raw)) return out;
    for (i = 0, n = json_object_size(raw); i < n; i++) {
        const char *name = json_object_get_key_at(raw, i);
        const json_t *value = json_object_get_at(raw, i);
        char *key;
        if (!name) continue;
        key = strip_ascii(name);
        if (!key || !*key) { free(key); continue; }
        if (is_str(value)) {
            char *trimmed = strip_ascii(value->str_val);
            if (trimmed && *trimmed) {
                json_t *env = json_object();
                json_set(env, "encoding", json_string("plain"));
                json_set(env, "value", json_string(trimmed));
                json_set(out, key, env);
            }
            free(trimmed);
        } else if (!value || value->type == JSON_NULL) {
            const json_t *prev = stored ? json_obj_get(stored, key) : NULL;
            if (prev) json_set(out, key, json_copy(prev));
        } else if (value->type == JSON_OBJECT) {
            json_set(out, key, json_copy(value));
        }
        free(key);
    }
    return out;
}

