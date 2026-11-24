/*
 * shared_session_attach.c -- C11 port of hermes_cli/shared_session_attach.py.
 *
 * PoP: w20_sharedsess_a7d92d_local_origin @ hermes_cli/shared_session_attach.py:_local_origin
 * PoP: w20_sharedsess_a7d92d_request @ hermes_cli/shared_session_attach.py:discover_attach_url
 * PoP: w20_sharedsess_a7d92d_verify @ hermes_cli/shared_session_attach.py:discover_attach_url
 * PoP: w20_sharedsess_a7d92d_discover @ hermes_cli/shared_session_attach.py:discover_attach_url
 * PoP: w20_sharedsess_a7d92d_configure @ hermes_cli/shared_session_attach.py:configure_tui_attachment
 *
 * The socket, the registry file and the HTTP client stay upstream; the fence,
 * the HMAC proof pair and the reply gate are pure and ported here.
 */
#include "hermes_json.h"
#include "noc_w20_sharedsess_a7d92d.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SS_ERR_ENDPOINT =
    "Shared runtime endpoint must be an explicit loopback address and port.";
static const char *const SS_ERR_IDENTITY =
    "Shared runtime handshake identity does not match the requested owner.";
static const char *const SS_ERR_ORIGIN =
    "Shared runtime endpoint must be an origin without a path or query.";
static const char *const SS_ERR_AMBIGUOUS =
    "Session owner identity is ambiguous; no attachment was attempted.";
static const char *const SS_ERR_DIFFERENT =
    "Shared runtime handshake returned a different endpoint.";

static char *ss_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int ss_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* str(value) for the scalar wire shapes a lease id can carry. */
static char *ss_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return ss_dup("None");
    switch (v->type) {
    case JSON_STRING: return ss_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return ss_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return ss_dup(buf);
    default: return ss_dup("");
    }
}

/* --- SHA-256 / HMAC-SHA-256 ------------------------------------------------ */

typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t n;
} ss_sha256;

static const uint32_t SS_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t ss_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void ss_sha_block(ss_sha256 *c, const unsigned char *p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ss_rotr(w[i - 15], 7) ^ ss_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ss_rotr(w[i - 2], 17) ^ ss_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
    e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ss_rotr(e, 6) ^ ss_rotr(e, 11) ^ ss_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + SS_K[i] + w[i];
        uint32_t S0 = ss_rotr(a, 2) ^ ss_rotr(a, 13) ^ ss_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void ss_sha_init(ss_sha256 *c) {
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u;
    c->h[3] = 0xa54ff53au; c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->len = 0;
    c->n = 0;
}

static void ss_sha_update(ss_sha256 *c, const unsigned char *data, size_t len) {
    size_t i = 0;
    c->len += (uint64_t)len * 8;
    while (i < len) {
        size_t take = 64 - c->n;
        if (take > len - i) take = len - i;
        memcpy(c->buf + c->n, data + i, take);
        c->n += take;
        i += take;
        if (c->n == 64) {
            ss_sha_block(c, c->buf);
            c->n = 0;
        }
    }
}

static void ss_sha_final(ss_sha256 *c, unsigned char out[32]) {
    unsigned char pad[72];
    size_t padlen;
    uint64_t bits = c->len;
    int i;
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    padlen = (c->n < 56) ? (56 - c->n) : (120 - c->n);
    ss_sha_update(c, pad, padlen);
    for (i = 0; i < 8; i++) pad[i] = (unsigned char)(bits >> (56 - 8 * i));
    ss_sha_update(c, pad, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)c->h[i];
    }
}

static void ss_hmac_hex(const char *key, const char *msg, char out[65]) {
    unsigned char k[64], ipad[64], opad[64], inner[32], outer[32];
    size_t klen = key ? strlen(key) : 0;
    size_t mlen = msg ? strlen(msg) : 0;
    ss_sha256 c;
    int i;
    static const char HEX[] = "0123456789abcdef";
    memset(k, 0, sizeof(k));
    if (klen > 64) {
        ss_sha_init(&c);
        ss_sha_update(&c, (const unsigned char *)key, klen);
        ss_sha_final(&c, k);
    } else if (klen) {
        memcpy(k, key, klen);
    }
    for (i = 0; i < 64; i++) {
        ipad[i] = (unsigned char)(k[i] ^ 0x36);
        opad[i] = (unsigned char)(k[i] ^ 0x5c);
    }
    ss_sha_init(&c);
    ss_sha_update(&c, ipad, 64);
    ss_sha_update(&c, (const unsigned char *)(msg ? msg : ""), mlen);
    ss_sha_final(&c, inner);
    ss_sha_init(&c);
    ss_sha_update(&c, opad, 64);
    ss_sha_update(&c, inner, 32);
    ss_sha_final(&c, outer);
    for (i = 0; i < 32; i++) {
        out[i * 2] = HEX[outer[i] >> 4];
        out[i * 2 + 1] = HEX[outer[i] & 0xf];
    }
    out[64] = '\0';
}

/* --- urlsplit / _local_origin ---------------------------------------------- */

typedef struct {
    char scheme[16];
    char host[192];
    char path[512];
    char query[512];
    char port_err[96];
    int user_present;
    int pass_present;
    int has_port;
    int port;
    int has_fragment;
} ss_url;

static int ss_is_unreserved(unsigned char c) {
    return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

static void ss_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* urlencode/quote_plus(value, safe="") */
static char *ss_quote_plus(const char *s) {
    size_t i, o = 0;
    char *out;
    static const char HEX[] = "0123456789ABCDEF";
    if (!s) s = "";
    out = (char *)malloc(strlen(s) * 3 + 1);
    if (!out) return NULL;
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (ss_is_unreserved(c)) {
            out[o++] = (char)c;
        } else if (c == ' ') {
            out[o++] = '+';
        } else {
            out[o++] = '%';
            out[o++] = HEX[c >> 4];
            out[o++] = HEX[c & 0xf];
        }
    }
    out[o] = '\0';
    return out;
}

static int ss_all_digits(const char *s, size_t n) {
    size_t i;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* A faithful-enough urlsplit: scheme, netloc (userinfo/host/port), path, query,
 * fragment; `.port` raises the same ValueErrors Python does. */
static void ss_urlsplit(const char *url, ss_url *u) {
    const char *p = url ? url : "";
    const char *rest, *auth_end, *auth;
    size_t i;
    memset(u, 0, sizeof(*u));
    /* scheme */
    for (i = 0; p[i]; i++) {
        char c = p[i];
        if (c == ':') break;
        if (c == '/' || c == '?' || c == '#') break;
        if (i == 0) {
            if (!isalpha((unsigned char)c)) break;
        } else if (!(isalnum((unsigned char)c) || c == '+' || c == '-' || c == '.')) {
            break;
        }
    }
    if (p[i] == ':' && i > 0) {
        size_t n = i < sizeof(u->scheme) - 1 ? i : sizeof(u->scheme) - 1;
        memcpy(u->scheme, p, n);
        u->scheme[n] = '\0';
        ss_lower(u->scheme);
        rest = p + i + 1;
    } else {
        rest = p;
    }
    if (rest[0] == '/' && rest[1] == '/') {
        auth = rest + 2;
        auth_end = auth;
        while (*auth_end && *auth_end != '/' && *auth_end != '?' && *auth_end != '#') auth_end++;
        {
            const char *at = NULL, *q;
            for (q = auth; q < auth_end; q++) {
                if (*q == '@') at = q;
            }
            if (at) {
                const char *colon = NULL;
                u->user_present = 1;
                for (q = auth; q < at; q++) {
                    if (*q == ':') { colon = q; break; }
                }
                if (colon) u->pass_present = 1;
                auth = at + 1;
            }
            {
                size_t hlen;
                const char *colon = NULL;
                if (*auth == '[') {
                    const char *close = memchr(auth, ']', (size_t)(auth_end - auth));
                    if (close) {
                        hlen = (size_t)(close - auth - 1);
                        if (hlen >= sizeof(u->host)) hlen = sizeof(u->host) - 1;
                        memcpy(u->host, auth + 1, hlen);
                        u->host[hlen] = '\0';
                        if (close + 1 < auth_end && close[1] == ':') {
                            colon = close + 1;
                        }
                    } else {
                        hlen = (size_t)(auth_end - auth);
                        if (hlen >= sizeof(u->host)) hlen = sizeof(u->host) - 1;
                        memcpy(u->host, auth, hlen);
                        u->host[hlen] = '\0';
                    }
                } else {
                    for (q = auth; q < auth_end; q++) {
                        if (*q == ':') colon = q;
                    }
                    hlen = (size_t)((colon ? colon : auth_end) - auth);
                    if (hlen >= sizeof(u->host)) hlen = sizeof(u->host) - 1;
                    memcpy(u->host, auth, hlen);
                    u->host[hlen] = '\0';
                }
                ss_lower(u->host);
                if (colon) {
                    size_t plen = (size_t)(auth_end - colon - 1);
                    if (plen == 0) {
                        u->has_port = 0;
                    } else if (!ss_all_digits(colon + 1, plen)) {
                        snprintf(u->port_err, sizeof(u->port_err),
                                 "Port could not be cast to integer value as '%.*s'",
                                 (int)(plen > 60 ? 60 : plen), colon + 1);
                    } else {
                        long v = strtol(colon + 1, NULL, 10);
                        if (v < 0 || v > 65535) {
                            snprintf(u->port_err, sizeof(u->port_err),
                                     "Port out of range 0-65535");
                        } else {
                            u->has_port = 1;
                            u->port = (int)v;
                        }
                    }
                }
            }
        }
        rest = auth_end;
    }
    /* path / query / fragment */
    {
        const char *frag = NULL, *qmark = NULL, *q;
        for (q = rest; *q; q++) {
            if (*q == '#' && !frag) { frag = q; break; }
        }
        for (q = rest; *q && (!frag || q < frag); q++) {
            if (*q == '?') { qmark = q; break; }
        }
        {
            const char *pend = qmark ? qmark : (frag ? frag : rest + strlen(rest));
            size_t n = (size_t)(pend - rest);
            if (n >= sizeof(u->path)) n = sizeof(u->path) - 1;
            memcpy(u->path, rest, n);
            u->path[n] = '\0';
        }
        if (qmark) {
            const char *qend = frag ? frag : qmark + strlen(qmark);
            size_t n = (size_t)(qend - qmark - 1);
            if (n >= sizeof(u->query)) n = sizeof(u->query) - 1;
            memcpy(u->query, qmark + 1, n);
            u->query[n] = '\0';
        }
        if (frag && frag[1]) u->has_fragment = 1;
    }
}

static int ss_is_loopback(const char *host) {
    if (!host || !host[0]) return 0;
    if (!strcmp(host, "::1")) return 1;
    if (!strcmp(host, "0:0:0:0:0:0:0:1")) return 1;
    {
        unsigned a = 0, b = 0, c = 0, d = 0;
        char tail;
        if (sscanf(host, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) == 4) {
            return a == 127 && b < 256 && c < 256 && d < 256;
        }
    }
    return 0;
}

static json_t *ss_fail(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(type));
    json_set(out, "message", json_string(message));
    return out;
}

static json_t *ss_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

/* _local_origin: the (host, port) fence, or NULL with *err set. */
static int ss_local_origin(const char *url, const char *scheme, char *host_out, size_t host_cap,
                          int *port_out, const char **err, char *errbuf, size_t errcap) {
    ss_url u;
    ss_urlsplit(url, &u);
    if (u.port_err[0]) {
        snprintf(errbuf, errcap, "%s", u.port_err);
        *err = errbuf;
        return 0;
    }
    if (strcmp(u.scheme, scheme ? scheme : "") != 0 || !ss_is_loopback(u.host) ||
        u.user_present || u.pass_present || u.has_fragment || !u.has_port || u.port == 0) {
        *err = SS_ERR_ENDPOINT;
        return 0;
    }
    snprintf(host_out, host_cap, "%s", u.host);
    *port_out = u.port;
    return 1;
}

json_t *w20_sharedsess_a7d92d_local_origin(const char *url, const char *scheme) {
    char host[192];
    char errbuf[128];
    int port = 0;
    const char *err = NULL;
    if (!ss_local_origin(url, scheme, host, sizeof(host), &port, &err, errbuf, sizeof(errbuf))) {
        return ss_fail("ValueError", err);
    }
    {
        json_t *value = json_array();
        json_append(value, json_string(host));
        json_append(value, json_number((double)port));
        return ss_ok(value);
    }
}

json_t *w20_sharedsess_a7d92d_request(const char *session_id, const char *nonce,
                                     const char *profile_home, const char *lease_id) {
    char msg[2048];
    char proof[65];
    char *q_sid, *q_nonce, *q_home, *q_proof;
    char *query;
    size_t need;
    json_t *value;
    snprintf(msg, sizeof(msg), "hermes-session-attach-request:%s:%s",
             session_id ? session_id : "", nonce ? nonce : "");
    ss_hmac_hex(lease_id ? lease_id : "", msg, proof);
    q_sid = ss_quote_plus(session_id ? session_id : "");
    q_nonce = ss_quote_plus(nonce ? nonce : "");
    q_home = ss_quote_plus(profile_home ? profile_home : "");
    q_proof = ss_quote_plus(proof);
    if (!q_sid || !q_nonce || !q_home || !q_proof) {
        free(q_sid); free(q_nonce); free(q_home); free(q_proof);
        return ss_fail("MemoryError", "");
    }
    need = strlen(q_sid) + strlen(q_nonce) + strlen(q_home) + strlen(q_proof) + 64;
    query = (char *)malloc(need);
    if (!query) {
        free(q_sid); free(q_nonce); free(q_home); free(q_proof);
        return ss_fail("MemoryError", "");
    }
    snprintf(query, need, "session_id=%s&nonce=%s&profile_home=%s&client_proof=%s",
             q_sid, q_nonce, q_home, q_proof);
    value = json_object();
    json_set(value, "client_proof", json_string(proof));
    json_set(value, "query", json_string(query));
    json_set(value, "path", json_string("/api/session-attach"));
    free(q_sid); free(q_nonce); free(q_home); free(q_proof);
    free(query);
    return ss_ok(value);
}

/* The reply gate, in the order discover_attach_url applies it. */
static int ss_verify_reply(const char *session_id, const char *nonce, const char *profile_home,
                          const char *lease_id, const char *endpoint, const json_t *reply,
                          char *url_out, size_t url_cap, const char **err) {
    char origin_host[192], ws_host[192];
    char origin_buf[128], ws_buf[128];
    int origin_port = 0, ws_port = 0;
    const char *origin_err = NULL, *ws_err = NULL;
    const char *ws_url;
    char msg[4096];
    char proof[65];
    ss_url ws_parts;

    if (!ss_local_origin(endpoint, "http", origin_host, sizeof(origin_host), &origin_port,
                         &origin_err, origin_buf, sizeof(origin_buf))) {
        *err = origin_err;
        return 0;
    }
    ws_url = (reply && reply->type == JSON_OBJECT) ? json_get_str(reply, "websocket_url", NULL)
                                                   : NULL;
    if (!ws_url) {
        *err = SS_ERR_IDENTITY;
        return 0;
    }
    snprintf(msg, sizeof(msg), "hermes-session-attach:%s:%s:%s:%s",
             session_id ? session_id : "", nonce ? nonce : "",
             profile_home ? profile_home : "", ws_url);
    ss_hmac_hex(lease_id ? lease_id : "", msg, proof);
    {
        const char *got_sid = json_get_str(reply, "session_id", "");
        const char *got_nonce = json_get_str(reply, "nonce", "");
        const char *got_home = json_get_str(reply, "profile_home", "");
        const char *got_proof = json_get_str(reply, "attach_proof", "");
        if (strcmp(got_sid, session_id ? session_id : "") ||
            strcmp(got_nonce, nonce ? nonce : "") ||
            strcmp(got_home, profile_home ? profile_home : "") ||
            strcmp(got_proof, proof)) {
            *err = SS_ERR_IDENTITY;
            return 0;
        }
    }
    if (!ss_local_origin(ws_url, "ws", ws_host, sizeof(ws_host), &ws_port, &ws_err,
                         ws_buf, sizeof(ws_buf))) {
        *err = ws_err;
        return 0;
    }
    ss_urlsplit(ws_url, &ws_parts);
    if (ws_port != origin_port || strcmp(ws_host, origin_host) ||
        strcmp(ws_parts.path, "/api/ws")) {
        *err = SS_ERR_DIFFERENT;
        return 0;
    }
    snprintf(url_out, url_cap, "%s", ws_url);
    return 1;
}

json_t *w20_sharedsess_a7d92d_verify(const char *session_id, const char *nonce,
                                    const char *profile_home, const char *lease_id,
                                    const char *endpoint, const json_t *reply) {
    char url[512];
    const char *err = NULL;
    if (!ss_verify_reply(session_id, nonce, profile_home, lease_id, endpoint, reply,
                         url, sizeof(url), &err)) {
        return ss_fail("ValueError", err);
    }
    return ss_ok(json_string(url));
}

/* discover_attach_url over a scripted registry + handshake. */
json_t *w20_sharedsess_a7d92d_discover(const char *session_id, const json_t *owners,
                                      const char *nonce, const char *profile_home,
                                      const char *owner_details,
                                      const json_t *handshake) {
    json_t *owner = NULL;
    json_t *endpoint, *handshakerr, *reply;
    const char *endpoint_text;
    char *lease;
    size_t matches = 0, i;
    char buf[8192];
    const char *err = NULL;

    if (owners && owners->type == JSON_ARRAY) {
        for (i = 0; i < owners->c.count; i++) {
            json_t *entry = owners->c.items[i];
            const char *sid = json_get_str(entry, "session_id", NULL);
            if (sid && !strcmp(sid, session_id ? session_id : "")) {
                matches++;
                if (!owner) owner = entry;
            }
        }
    }
    if (matches == 0) return ss_ok(json_null());
    if (matches > 1) return ss_fail("ValueError", SS_ERR_AMBIGUOUS);
    endpoint = json_obj_get(owner, "metadata");
    endpoint = endpoint ? json_obj_get(endpoint, "shared_runtime_url") : NULL;
    endpoint_text = (endpoint && endpoint->type == JSON_STRING) ? endpoint->str_val : NULL;
    if (!endpoint_text || !endpoint_text[0]) {
        snprintf(buf, sizeof(buf),
                 "This chat is open in another Hermes window/terminal, and attaching this "
                 "terminal to it is not available in this build. Close the chat there and run "
                 "hermes --resume %s here to take it over.\n%s",
                 session_id ? session_id : "", owner_details ? owner_details : "");
        return ss_fail("ValueError", buf);
    }
    {
        char host[192];
        char epbuf[128];
        int port = 0;
        ss_url u;
        if (!ss_local_origin(endpoint_text, "http", host, sizeof(host), &port, &err,
                             epbuf, sizeof(epbuf)))
            return ss_fail("ValueError", err);
        ss_urlsplit(endpoint_text, &u);
        if ((u.path[0] && strcmp(u.path, "/")) || u.query[0])
            return ss_fail("ValueError", SS_ERR_ORIGIN);
    }
    /* str(owner["lease_id"]) -- a missing key raises KeyError. */
    {
        json_t *lid = json_obj_get(owner, "lease_id");
        if (!lid) return ss_fail("KeyError", "'lease_id'");
        lease = ss_str(lid);
    }
    handshakerr = handshake ? json_obj_get(handshake, "error") : NULL;
    if ((handshakerr && ss_truthy(handshakerr)) ||
        (handshake && json_obj_get(handshake, "body_raw") != NULL)) {
        snprintf(buf, sizeof(buf),
                 "This chat is open in another Hermes window/terminal, and attaching this "
                 "terminal to it just failed. Use the chat where it is open, or close it there "
                 "and run hermes --resume %s here.\n%s",
                 session_id ? session_id : "", owner_details ? owner_details : "");
        free(lease);
        return ss_fail("ValueError", buf);
    }
    reply = handshake ? json_obj_get(handshake, "reply") : NULL;
    {
        char url[512];
        if (!ss_verify_reply(session_id, nonce, profile_home ? profile_home : "",
                             lease, endpoint_text, reply, url, sizeof(url), &err)) {
            free(lease);
            return ss_fail("ValueError", err);
        }
        free(lease);
        return ss_ok(json_string(url));
    }
}

json_t *w20_sharedsess_a7d92d_configure(const json_t *env, const char *session_id,
                                       const json_t *discovered_url) {
    json_t *out = json_object();
    const char *explicit_url;
    size_t i;
    for (i = 0; i < json_object_size(env); i++) {
        json_set(out, json_object_get_key_at(env, i),
                 json_copy(json_object_get_at(env, i)));
    }
    explicit_url = json_get_str(env, "HERMES_TUI_GATEWAY_URL", "");
    if (!session_id || !session_id[0]) return out;
    if (explicit_url && *explicit_url) {
        const char *scan = explicit_url;
        while (*scan && (unsigned char)*scan <= 0x20) scan++;
        if (*scan) return out;
    }
    if (discovered_url && discovered_url->type == JSON_STRING) {
        json_set(out, "HERMES_TUI_GATEWAY_URL", json_string(discovered_url->str_val));
    }
    return out;
}

json_t *w20_sharedsess_a7d92d_attach(const json_t *env, const char *session_id,
                                    const json_t *owners, const char *nonce,
                                    const char *profile_home, const char *owner_details,
                                    const json_t *handshake) {
    const char *explicit_url;
    json_t *found, *out;
    if (!session_id || !session_id[0]) return w20_sharedsess_a7d92d_configure(env, session_id, NULL);
    explicit_url = json_get_str(env, "HERMES_TUI_GATEWAY_URL", "");
    if (explicit_url && *explicit_url) {
        const char *scan = explicit_url;
        while (*scan && (unsigned char)*scan <= 0x20) scan++;
        if (*scan) return w20_sharedsess_a7d92d_configure(env, session_id, NULL);
    }
    found = w20_sharedsess_a7d92d_discover(session_id, owners, nonce, profile_home,
                                          owner_details, handshake);
    if (!strcmp(json_get_str(found, "status", ""), "error")) return found;
    out = w20_sharedsess_a7d92d_configure(env, session_id, json_obj_get(found, "value"));
    json_free(found);
    return out;
}
