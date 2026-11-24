/*
 * ws_tickets.c - C11 port of upstream hermes_cli/dashboard_auth/ws_tickets.py
 * (gate w24_wstickets_fa7658).
 *
 * PoP: w24_wstickets_fa7658_mint_info @ hermes_cli/dashboard_auth/ws_tickets.py:mint_ticket
 * PoP: w24_wstickets_fa7658_consume @ hermes_cli/dashboard_auth/ws_tickets.py:consume_ticket
 * PoP: w24_wstickets_fa7658_gc_keep @ hermes_cli/dashboard_auth/ws_tickets.py:_gc_expired_locked
 * PoP: w24_wstickets_fa7658_consume_internal @ hermes_cli/dashboard_auth/ws_tickets.py:consume_internal_credential
 * PoP: w24_wstickets_fa7658_constants @ hermes_cli/dashboard_auth/ws_tickets.py:<module>
 *
 * Seams: ``secrets.token_urlsafe`` (the secret itself), ``time.time`` (passed as
 * ``now``), ``_lock`` and the ``_tickets`` / ``_internal_credential`` globals
 * (passed as the stored entry / the expected value).  Ported here is what the
 * store's values decide: the info object a mint writes, expired-vs-unknown-vs-ok
 * on consume (including the truncated ticket in the failure text), the sweep
 * predicate, and the internal-credential gate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_wstickets_fa7658.h"

#define TTL_SECONDS 30
#define INTERNAL_ID "server-internal"

/* Python truthiness of a JSON value. */
static int ws_truthy(const json_t *v) {
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

/* int(value): 1 on success.  Only the shapes a clock hands over (number /
 * bool) are accepted; anything else is the ValueError seam. */
static int ws_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    return 0;
}

/* Byte length of the first ``n`` characters of a UTF-8 string (Python slices
 * codepoints, so a multi-byte ticket must not be cut mid-sequence). */
static size_t ws_prefix_bytes(const char *s, size_t n) {
    size_t i = 0, count = 0;
    if (!s) return 0;
    while (s[i] && count < n) {
        i++;
        while (s[i] && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
        count++;
    }
    return i;
}

static json_t *ws_ok(json_t *info) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "info", info ? info : json_null());
    return o;
}

static json_t *ws_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg));
    return o;
}

json_t *w24_wstickets_fa7658_mint_info(const json_t *req) {
    const json_t *extra, *issuer;
    long long minted;
    json_t *info;

    if (!req || req->type != JSON_OBJECT) return json_null();
    if (!ws_int(json_obj_get(req, "now"), &minted)) return json_null();  /* ValueError seam */
    extra = json_obj_get(req, "extra");
    if (ws_truthy(extra) && extra->type != JSON_OBJECT)
        return json_null();                       /* **non-mapping -> TypeError seam */

    info = json_object();
    {
        const json_t *uid = json_obj_get(req, "user_id");
        const json_t *prov = json_obj_get(req, "provider");
        json_set(info, "user_id", uid ? json_copy(uid) : json_null());
        json_set(info, "provider", prov ? json_copy(prov) : json_null());
    }
    json_set(info, "minted_at", json_number((double)minted));
    if (extra && extra->type == JSON_OBJECT) {
        size_t i;
        for (i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (!k) continue;
            json_set(info, k, json_copy(json_object_get_at(extra, i)));
        }
    }
    issuer = json_obj_get(req, "issuer");
    if (ws_truthy(issuer))
        json_set(info, "issuer", json_copy(issuer));
    return info;
}

json_t *w24_wstickets_fa7658_consume(const json_t *req) {
    const json_t *ticket, *entry, *expires, *info;
    long long now = 0, exp = 0;

    if (!req || req->type != JSON_OBJECT) return json_null();
    ticket = json_obj_get(req, "ticket");
    entry = json_obj_get(req, "entry");
    if (!ws_int(json_obj_get(req, "now"), &now)) return json_null();

    if (!entry || entry->type == JSON_NULL) {
        const char *t;
        size_t cut, need;
        char *msg;
        json_t *o;

        if (!ws_truthy(ticket)) return ws_err("unknown ticket: <empty>");
        if (ticket->type != JSON_STRING) return json_null();   /* TypeError seam */
        t = ticket->str_val ? ticket->str_val : "";
        cut = ws_prefix_bytes(t, 8);
        need = strlen("unknown ticket: ") + cut + strlen("\xE2\x80\xA6") + 1;
        msg = (char *)malloc(need);
        if (!msg) return json_null();
        snprintf(msg, need, "unknown ticket: %.*s\xE2\x80\xA6", (int)cut, t);
        o = ws_err(msg);
        free(msg);
        return o;
    }

    if (entry->type != JSON_ARRAY || json_len(entry) != 2)
        return json_null();                        /* unpack ValueError seam */
    expires = json_get(entry, 0);
    info = json_get(entry, 1);
    if (!expires || expires->type != JSON_NUMBER) return json_null();
    exp = (long long)expires->num_val;
    if (exp < now) return ws_err("expired");
    return ws_ok(json_copy(info));
}

json_t *w24_wstickets_fa7658_gc_keep(const json_t *req) {
    const json_t *rows;
    long long now = 0;
    json_t *out;
    size_t i, n;

    if (!req || req->type != JSON_OBJECT) return json_array();
    if (!ws_int(json_obj_get(req, "now"), &now)) return json_null();
    out = json_array();
    rows = json_obj_get(req, "tickets");
    if (!rows || rows->type != JSON_ARRAY) return out;
    n = json_len(rows);
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *exp, *t;
        if (!row || row->type != JSON_OBJECT) { json_free(out); return json_null(); }
        exp = json_obj_get(row, "expires_at");
        if (!exp || exp->type != JSON_NUMBER) { json_free(out); return json_null(); }
        if ((long long)exp->num_val < now) continue;     /* swept */
        t = json_obj_get(row, "ticket");
        json_append(out, t ? json_copy(t) : json_null());
    }
    return out;
}

json_t *w24_wstickets_fa7658_consume_internal(const json_t *req) {
    const json_t *value, *expected;

    if (!req || req->type != JSON_OBJECT) return json_null();
    value = json_obj_get(req, "value");
    expected = json_obj_get(req, "expected");

    if (!ws_truthy(value) || !expected || expected->type == JSON_NULL)
        return ws_err("no internal credential");
    if (value->type != JSON_STRING || expected->type != JSON_STRING)
        return json_null();                        /* .encode() AttributeError seam */
    /* Constant-time compare: any difference is one rejection. */
    {
        const char *a = value->str_val ? value->str_val : "";
        const char *b = expected->str_val ? expected->str_val : "";
        size_t la = strlen(a), lb = strlen(b), i;
        unsigned diff = (unsigned)(la ^ lb);
        for (i = 0; i < la && i < lb; i++) diff |= (unsigned)(a[i] ^ b[i]);
        if (diff) return ws_err("internal credential mismatch");
    }
    {
        json_t *info = json_object();
        json_set(info, "user_id", json_string(INTERNAL_ID));
        json_set(info, "provider", json_string(INTERNAL_ID));
        return ws_ok(info);
    }
}

json_t *w24_wstickets_fa7658_constants(void) {
    json_t *o = json_object();
    json_set(o, "ttl_seconds", json_number((double)TTL_SECONDS));
    json_set(o, "internal_user_id", json_string(INTERNAL_ID));
    json_set(o, "internal_provider", json_string(INTERNAL_ID));
    return o;
}
