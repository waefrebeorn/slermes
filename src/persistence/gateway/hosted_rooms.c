/*
 * hosted_rooms.c -- C11 port of the pure half of upstream
 *   persistence/gateway/hosted_rooms.py.
 *
 * PoP: w23_hostedroom_95f0aa_grant_digest @ persistence/gateway/hosted_rooms.py:grant_digest
 * PoP: w23_hostedroom_95f0aa_hard_expiry @ persistence/gateway/hosted_rooms.py:room_grant_hard_expiry
 * PoP: w23_hostedroom_95f0aa_task_identifier @ persistence/gateway/hosted_rooms.py:_task_identifier
 * PoP: w23_hostedroom_95f0aa_task_identity @ persistence/gateway/hosted_rooms.py:HostedTaskIdentity
 * PoP: w23_hostedroom_95f0aa_constants @ persistence/gateway/hosted_rooms.py:MAX_OWED_REVOKES
 *
 * The repository Protocols are sqlite and stay upstream. What is pure is
 * ported here: the grant digest an owed revoke is keyed under, the grant's
 * hard expiry read from its unsigned payload (the same lenient urlsafe
 * base64 and JSON, with every failure landing on the longest horizon), the
 * room-driver identifier rule and the stable task identity built from it.
 *
 * Pure C11, ASCII, no VLA, no strdup.
 */
#include "hermes_json.h"
#include "noc_w23_hostedroom_95f0aa.h"
#include "crypto.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HB_MAX_OWED_REVOKE_SECONDS     (30.0 * 24.0 * 60.0 * 60.0)
#define HB_MAX_OWED_REVOKES_PER_MEMBER 4
#define HB_MAX_OWED_REVOKES            512
#define HB_MAX_IDENTIFIER_CHARS        128

/* ---------- constants ---------- */

json_t *w23_hostedroom_95f0aa_constants(void) {
    static const char *const statuses[] = {"cancelled", "deferred", "failed", "indeterminate",
                                           "queued", "running", "settled", "stopping"};
    static const char *const terminal[] = {"cancelled", "failed", "settled"};
    static const char *const shown[] = {"queued", "running", "stopping", "indeterminate", "deferred"};
    json_t *out = json_object();
    json_t *arr;
    size_t i;
    if (!out) return json_null();
    json_set(out, "MAX_OWED_REVOKE_SECONDS", json_number(HB_MAX_OWED_REVOKE_SECONDS));
    json_set(out, "MAX_OWED_REVOKES_PER_MEMBER", json_number((double)HB_MAX_OWED_REVOKES_PER_MEMBER));
    json_set(out, "MAX_OWED_REVOKES", json_number((double)HB_MAX_OWED_REVOKES));
    arr = json_array();
    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) json_append(arr, json_string(statuses[i]));
    json_set(out, "TASK_STATUSES", arr);
    arr = json_array();
    for (i = 0; i < sizeof(terminal) / sizeof(terminal[0]); i++) json_append(arr, json_string(terminal[i]));
    json_set(out, "TERMINAL_STATUSES", arr);
    arr = json_array();
    for (i = 0; i < sizeof(shown) / sizeof(shown[0]); i++) json_append(arr, json_string(shown[i]));
    json_set(out, "SHOWN_TASK_STATUSES", arr);
    return out;
}

/* ---------- grant_digest ---------- */

char *w23_hostedroom_95f0aa_grant_digest(const char *grant) {
    unsigned char digest[CRYPTO_SHA256_LEN];
    char *hex;
    size_t i;
    static const char *hexdig = "0123456789abcdef";
    if (!grant) return NULL;
    crypto_sha256((const unsigned char *)grant, strlen(grant), digest);
    hex = (char *)malloc(CRYPTO_SHA256_LEN * 2 + 1);
    if (!hex) return NULL;
    for (i = 0; i < CRYPTO_SHA256_LEN; i++) {
        hex[i * 2] = hexdig[digest[i] >> 4];
        hex[i * 2 + 1] = hexdig[digest[i] & 0x0f];
    }
    hex[CRYPTO_SHA256_LEN * 2] = '\0';
    return hex;
}

/* ---------- urlsafe base64, as base64.urlsafe_b64decode reads it ----------
 * The caller re-pads to a multiple of four first; characters outside the
 * alphabet are discarded (binascii's non-strict mode), '=' ends the data and
 * a leftover count of one more than a multiple of four is the padding error
 * that raises ValueError upstream. */
static int hb_b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

static unsigned char *hb_b64url_decode(const char *s, size_t *out_len) {
    size_t n, i, count = 0, olen = 0;
    unsigned int acc = 0, bits = 0;
    unsigned char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (unsigned char *)malloc(n / 4 * 3 + 4);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        int v;
        if (s[i] == '=') break;
        v = hb_b64val(s[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned int)v;
        bits += 6;
        count++;
        if (bits >= 8) {
            bits -= 8;
            out[olen++] = (unsigned char)((acc >> bits) & 0xff);
        }
    }
    if (count % 4 == 1) { free(out); return NULL; }
    *out_len = olen;
    return out;
}

static int hb_all_ascii(const unsigned char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (b[i] >= 0x80) return 0;
    return 1;
}

/* The fallback: an unreadable grant counts as living the longest horizon. */
static json_t *hb_longest_horizon(const json_t *req) {
    const json_t *n = req && req->type == JSON_OBJECT ? json_obj_get(req, "now") : NULL;
    double now = (n && n->type == JSON_NUMBER) ? n->num_val : (double)time(NULL);
    return json_number(now + HB_MAX_OWED_REVOKE_SECONDS);
}

static int hb_to_double(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        char *end = NULL;
        double d;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f') s++;
        if (!*s) return 0;
        d = strtod(s, &end);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\v' || *end == '\f') end++;
        if (*end) return 0;
        *out = d;
        return 1;
    }
    return 0;
}

json_t *w23_hostedroom_95f0aa_hard_expiry(const json_t *req) {
    const char *grant = req && req->type == JSON_OBJECT ? json_get_str(req, "grant", NULL) : NULL;
    const char *dot;
    char *enc;
    unsigned char *payload, *sig;
    size_t payload_len = 0, sig_len = 0, enc_len;
    json_t *claims, *found, *expires;
    double value;

    if (!grant) return hb_longest_horizon(req);
    dot = strchr(grant, '.');
    if (!dot) return hb_longest_horizon(req);

    enc_len = (size_t)(dot - grant);
    enc = (char *)malloc(enc_len + 5);
    if (!enc) return hb_longest_horizon(req);
    memcpy(enc, grant, enc_len);
    memset(enc + enc_len, '=', (4 - enc_len % 4) % 4);
    enc[enc_len + (4 - enc_len % 4) % 4] = '\0';
    payload = hb_b64url_decode(enc, &payload_len);
    free(enc);
    if (!payload) return hb_longest_horizon(req);

    /* the signature part is decoded too, so its padding errors refuse first */
    {
        const char *sp = dot + 1;
        size_t sl = strlen(sp), pad = (4 - sl % 4) % 4;
        char *sbuf = (char *)malloc(sl + pad + 1);
        if (!sbuf) { free(payload); return hb_longest_horizon(req); }
        memcpy(sbuf, sp, sl);
        memset(sbuf + sl, '=', pad);
        sbuf[sl + pad] = '\0';
        sig = hb_b64url_decode(sbuf, &sig_len);
        free(sbuf);
        if (!sig) { free(payload); return hb_longest_horizon(req); }
        free(sig);
    }

    if (!hb_all_ascii(payload, payload_len)) { free(payload); return hb_longest_horizon(req); }
    {
        char *text = (char *)malloc(payload_len + 1);
        if (!text) { free(payload); return hb_longest_horizon(req); }
        memcpy(text, payload, payload_len);
        text[payload_len] = '\0';
        free(payload);
        claims = json_parse(text, NULL);
        free(text);
    }
    if (!claims || claims->type != JSON_OBJECT) {
        json_free(claims);
        return hb_longest_horizon(req);
    }
    /* claims.get("status_expires_at", claims["expires_at"]): the default is
     * read eagerly, so a grant without expires_at refuses even when it has a
     * status horizon. */
    expires = json_obj_get(claims, "expires_at");
    if (!expires) { json_free(claims); return hb_longest_horizon(req); }
    found = json_obj_get(claims, "status_expires_at");
    if (!found) found = expires;
    if (!hb_to_double(found, &value)) { json_free(claims); return hb_longest_horizon(req); }
    json_free(claims);
    return json_number(value);
}

/* ---------- the identifier rule ---------- */

static int hb_is_ident_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static int hb_is_ident_char(char c) {
    return hb_is_ident_start(c) || c == '.' || c == '_' || c == ':' || c == '-';
}

/* A stripped string of at most 128 characters matching ^[A-Za-z0-9][A-Za-z0-9._:-]*$. */
static int hb_identifier_ok(const char *s, size_t n) {
    size_t i;
    if (n == 0 || n > HB_MAX_IDENTIFIER_CHARS) return 0;
    if (!hb_is_ident_start(s[0])) return 0;
    for (i = 1; i < n; i++)
        if (!hb_is_ident_char(s[i])) return 0;
    return 1;
}

static const char *hb_strip_left(const char *s, size_t *n) {
    while (*n > 0 && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f')) {
        s++;
        (*n)--;
    }
    return s;
}

static void hb_strip_right(const char *s, size_t *n) {
    while (*n > 0) {
        char c = s[*n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') (*n)--;
        else break;
    }
}

static void hb_set_error(json_t *out, const char *fmt, const char *label) {
    char msg[160];
    snprintf(msg, sizeof(msg), fmt, label ? label : "");
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(msg));
}

/* Returns the stripped identifier (malloc'd) or NULL; on failure `out` carries
 * the refusal message. */
static char *hb_identifier(const json_t *value, const char *label, json_t *out) {
    const char *s;
    size_t n;
    char *copy;
    if (!value || value->type != JSON_STRING || !value->str_val) {
        hb_set_error(out, "%s must be a string", label);
        return NULL;
    }
    s = value->str_val;
    n = strlen(s);
    s = hb_strip_left(s, &n);
    hb_strip_right(s, &n);
    if (!hb_identifier_ok(s, n)) {
        hb_set_error(out, "invalid %s", label);
        return NULL;
    }
    copy = (char *)malloc(n + 1);
    if (!copy) {
        hb_set_error(out, "invalid %s", label);
        return NULL;
    }
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

json_t *w23_hostedroom_95f0aa_task_identifier(const json_t *req) {
    const char *label = req && req->type == JSON_OBJECT ? json_get_str(req, "label", "value") : "value";
    json_t *out = json_object();
    char *value;
    if (!out) return json_null();
    value = hb_identifier(req && req->type == JSON_OBJECT ? json_obj_get(req, "value") : NULL,
                          label, out);
    if (!value) return out;
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", json_string(value));
    free(value);
    return out;
}

json_t *w23_hostedroom_95f0aa_task_identity(const json_t *req) {
    static const char *const fields[] = {"room_id", "task_id", "thread_id", "turn_id"};
    json_t *out = json_object();
    json_t *bag = json_object();
    size_t i;
    if (!out || !bag) {
        json_free(out);
        json_free(bag);
        return json_null();
    }
    for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        char *value = hb_identifier(req && req->type == JSON_OBJECT ? json_obj_get(req, fields[i]) : NULL,
                                    fields[i], out);
        if (!value) {
            json_free(bag);
            return out;
        }
        json_set(bag, fields[i], json_string(value));
        free(value);
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", bag);
    return out;
}
