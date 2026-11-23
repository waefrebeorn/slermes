/*
 * auth_error_copy.c — faithful C11 port of the pure predicates/copy builders
 * in hermes_cli/auth_error_copy.py (gate w13_autherror_74a9bf).
 * PoP: w13_autherror_74a9bf_device_flow_error @ hermes_cli/auth_error_copy.py:device_flow_error
 * PoP: w13_autherror_74a9bf_is_network_error @ hermes_cli/auth_error_copy.py:is_network_error
 * PoP: w13_autherror_74a9bf_is_device_code_expired @ hermes_cli/auth_error_copy.py:is_device_code_expired
 * PoP: w13_autherror_74a9bf_is_cancelled @ hermes_cli/auth_error_copy.py:is_cancelled
 */
#include "noc_w13_autherror_74a9bf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } sbuf;
static void sb_init(sbuf *b) {
    b->cap = 256; b->len = 0; b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void sb_grow(sbuf *b, size_t need) {
    if (b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void sb_addn(sbuf *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    sb_grow(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void sb_add(sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static char *sb_take(sbuf *b) { char *p = b->p; b->p = NULL; b->len = b->cap = 0; return p; }

/* str.format(retry=<retry_command>): replace every "{retry}". */
static void sb_add_retry(sbuf *b, const char *tpl, const char *retry) {
    const char *p = tpl;
    while (*p) {
        const char *q = strstr(p, "{retry}");
        if (!q) { sb_add(b, p); break; }
        sb_addn(b, p, (size_t)(q - p));
        sb_add(b, retry ? retry : "");
        p = q + 7;
    }
}

json_t *w13_autherror_74a9bf_device_flow_error(const char *code,
                                               const char *description,
                                               const char *retry_command) {
    const char *c = code ? code : "";
    const char *d = description ? description : "";
    const char *retry = retry_command ? retry_command : "hermes portal";
    sbuf lead; sb_init(&lead);

    if (!strcmp(c, "expired_token")) {
        sb_add(&lead, "The sign-in code expired before it was approved in the browser. Run `");
        sb_add_retry(&lead, "{retry}", retry);
        sb_add(&lead, "` to get a new code.");
    } else if (!strcmp(c, "access_denied")) {
        sb_add(&lead, "Sign-in was declined in the browser. Run `");
        sb_add_retry(&lead, "{retry}", retry);
        sb_add(&lead, "` to try again, or `hermes model` to pick a different provider.");
    } else if (!strcmp(c, "invalid_grant")) {
        sb_add(&lead, "The sign-in code was not accepted by the server. Run `");
        sb_add_retry(&lead, "{retry}", retry);
        sb_add(&lead, "` to get a new code.");
    } else if (!strcmp(c, "invalid_client")) {
        sb_add(&lead, "The server did not recognize this copy of Hermes. Run `hermes update`, then `");
        sb_add_retry(&lead, "{retry}", retry);
        sb_add(&lead, "` again.");
    } else {
        sb_add(&lead, "Sign-in did not complete: ");
        sb_add(&lead, d[0] ? d : "the server rejected the request");
        sb_add(&lead, ". Run `");
        sb_add_retry(&lead, "{retry}", retry);
        sb_add(&lead, "` to try again.");
    }

    sbuf details; sb_init(&details);
    if (c[0]) {
        sb_add(&details, c); sb_add(&details, ": "); sb_add(&details, d);
    } else {
        sb_add(&details, d);
    }

    sbuf full; sb_init(&full);
    sb_add(&full, lead.p ? lead.p : "");
    if (details.len) {
        sb_add(&full, "\n  Details: ");
        sb_add(&full, details.p ? details.p : "");
    }
    free(lead.p); free(details.p);

    json_t *out = json_object();
    json_set(out, "str", json_string(full.p ? full.p : ""));
    json_set(out, "oauth_error_code", json_string(c));
    free(full.p);
    return out;
}

static int in_set(const char *s, const char *const *set, size_t n) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

static const char *const NETWORK_TYPES[] = {
    "ConnectError", "ConnectTimeout", "ReadTimeout", "PoolTimeout", "WriteTimeout",
    "TimeoutException", "RemoteProtocolError", "ReadError", "ProxyError",
    "UnsupportedProtocol", "NetworkError",
};
static const char *const NETWORK_BASES[] = {
    "ConnectionError", "TimeoutError", "ConnectionResetError", "ConnectError", "ReadTimeout",
};

int w13_autherror_74a9bf_is_network_error(const char *type_name,
                                          const char *oauth_error_code,
                                          const char *code) {
    (void)oauth_error_code; (void)code;
    if (!type_name) return 0;
    /* SignInCopyError / DeviceCodeExpired short-circuit to False. */
    if (!strcmp(type_name, "SignInCopyError") || !strcmp(type_name, "DeviceCodeExpired")) return 0;
    if (in_set(type_name, NETWORK_TYPES,
               sizeof(NETWORK_TYPES) / sizeof(NETWORK_TYPES[0]))) return 1;
    if (in_set(type_name, NETWORK_BASES,
               sizeof(NETWORK_BASES) / sizeof(NETWORK_BASES[0]))) return 1;
    return 0;
}

int w13_autherror_74a9bf_is_device_code_expired(const char *oauth_error_code,
                                                const char *code) {
    const char *const expires[] = {"device_code_timeout", "timeout"};
    if (in_set(code, expires, 2)) return 1;
    if (oauth_error_code && !strcmp(oauth_error_code, "expired_token")) return 1;
    return 0;
}

int w13_autherror_74a9bf_is_cancelled(const char *type_name, long long exit_code) {
    if (!type_name) return 0;
    if (!strcmp(type_name, "KeyboardInterrupt") || !strcmp(type_name, "EOFError")) return 1;
    if (!strcmp(type_name, "SystemExit") &&
        (exit_code == 130 || exit_code == -1 || exit_code == 0)) return 1;
    return 0;
}
