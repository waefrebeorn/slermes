/*
 * port_web_server_ws_auth.c — WebSocket upgrade gate family.
 * Faithful port of _ws_client_reason, _ws_client_is_allowed,
 * _ws_host_origin_reason, _ws_host_origin_is_allowed, _ws_request_reason,
 * _ws_request_is_allowed, _ws_auth_mode, _ws_auth_reason, _ws_auth_ok, and
 * _has_valid_query_token from hermes_cli/web_server.py.
 *
 * Reuses: ws_is_accepted_host (port_web_server_auth.c) for the
 * DNS-rebinding Host check, and the ws_tickets store
 * (hermes_cli_ws_tickets.c) for gated-mode credentials.
 */

#include "web_server_ws_auth.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ws_tickets.h"

/* From port_web_server_auth.c */
extern bool ws_is_accepted_host(const char *host_header, const char *bound_host);

/* PoP: is_loopback_host @ gateway.platforms.webhook.py:_is_loopback_host */
/* _LOOPBACK_HOSTS = {"127.0.0.1", "::1", "localhost", "testclient"} */
static bool is_loopback_host(const char *h) {
    if (!h) return false;
    return strcmp(h, "127.0.0.1") == 0 || strcmp(h, "::1") == 0 ||
           strcmp(h, "localhost") == 0 || strcmp(h, "testclient") == 0;
}

/* (bound_host or "").strip().lower() */
static char *norm_bound_host(const char *bound) {
    if (!bound) bound = "";
    while (*bound == ' ' || *bound == '\t') bound++;
    size_t len = strlen(bound);
    while (len > 0 && (bound[len-1] == ' ' || bound[len-1] == '\t')) len--;
    char *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++) out[i] = (char)tolower((unsigned char)bound[i]);
    out[len] = '\0';
    return out;
}

static char *fmt(const char *f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return strdup(buf);
}

/* ── _ws_client_reason ──────────────────────────────────────────────────── */
/* PoP: ws_auth_client_reason @ hermes_cli/web_server.py:_ws_client_reason */
char *ws_auth_client_reason(const ws_auth_state_t *st,
                            const ws_upgrade_req_t *req) {
    if (st->auth_required) return NULL;
    char *bound = norm_bound_host(st->bound_host);
    if (*bound && !is_loopback_host(bound)) { free(bound); return NULL; }
    const char *client = req->client_host ? req->client_host : "";
    if (!*client) {
        char *r = fmt("missing_or_empty_peer bound=%s", *bound ? bound : "?");
        free(bound);
        return r;
    }
    if (is_loopback_host(client)) { free(bound); return NULL; }
    char *r = fmt("peer_not_loopback peer=%s bound=%s", client,
                  *bound ? bound : "?");
    free(bound);
    return r;
}

/* ── _ws_client_is_allowed ──────────────────────────────────────────────── */
/* PoP: ws_auth_client_is_allowed @ hermes_cli/web_server.py:_ws_client_is_allowed */
bool ws_auth_client_is_allowed(const ws_auth_state_t *st,
                               const ws_upgrade_req_t *req) {
    if (st->auth_required) return true;
    char *bound = norm_bound_host(st->bound_host);
    if (*bound && !is_loopback_host(bound)) { free(bound); return true; }
    free(bound);
    const char *client = req->client_host ? req->client_host : "";
    if (!*client) return false; /* fail-closed */
    return is_loopback_host(client);
}

/* ── _ws_host_origin_reason ─────────────────────────────────────────────── */
/* Minimal urlparse for the Origin header: scheme://netloc[/...]. Mirrors
 * urllib.parse.urlparse: scheme must be [a-z][a-z0-9+.-]* before "://";
 * netloc is what follows "//" up to '/', '?' or '#'. */
static bool origin_parse(const char *origin, char *scheme, size_t scheme_sz,
                         char *netloc, size_t netloc_sz) {
    scheme[0] = '\0';
    netloc[0] = '\0';
    const char *colon = strchr(origin, ':');
    if (colon && colon != origin) {
        bool ok = true;
        for (const char *p = origin; p < colon; p++) {
            char c = (char)tolower((unsigned char)*p);
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '+' || c == '-' || c == '.'))
                { ok = false; break; }
        }
        if (ok && !isdigit((unsigned char)origin[0])) {
            size_t sl = (size_t)(colon - origin);
            if (sl >= scheme_sz) sl = scheme_sz - 1;
            for (size_t i = 0; i < sl; i++)
                scheme[i] = (char)tolower((unsigned char)origin[i]);
            scheme[sl] = '\0';
            origin = colon + 1;
        }
    }
    if (strncmp(origin, "//", 2) == 0) {
        origin += 2;
        size_t i = 0;
        while (origin[i] && origin[i] != '/' && origin[i] != '?' &&
               origin[i] != '#' && i < netloc_sz - 1) {
            netloc[i] = origin[i];
            i++;
        }
        netloc[i] = '\0';
    }
    return true;
}

/* PoP: ws_auth_host_origin_reason @ hermes_cli/web_server.py:_ws_host_origin_reason */
char *ws_auth_host_origin_reason(const ws_auth_state_t *st,
                                 const ws_upgrade_req_t *req) {
    const char *bound_host = st->bound_host;
    if (!bound_host || !*bound_host) return NULL;

    const char *host_header = req->host_header ? req->host_header : "";
    if (!ws_is_accepted_host(host_header, bound_host))
        return fmt("host_mismatch host=%s bound=%s",
                   *host_header ? host_header : "?", bound_host);

    const char *origin = req->origin_header ? req->origin_header : "";
    if (!*origin) return NULL;

    char scheme[64], netloc[512];
    origin_parse(origin, scheme, sizeof(scheme), netloc, sizeof(netloc));
    if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0)
        return NULL; /* non-web origin (file://, null, app://) — trust upstream */
    if (!*netloc)
        return fmt("origin_mismatch origin=%s bound=%s", origin, bound_host);
    if (!ws_is_accepted_host(netloc, bound_host))
        return fmt("origin_mismatch origin=%s bound=%s", origin, bound_host);
    return NULL;
}

/* ── _ws_host_origin_is_allowed ─────────────────────────────────────────── */
/* PoP: ws_auth_host_origin_is_allowed @ hermes_cli/web_server.py:_ws_host_origin_is_allowed */
bool ws_auth_host_origin_is_allowed(const ws_auth_state_t *st,
                                    const ws_upgrade_req_t *req) {
    char *r = ws_auth_host_origin_reason(st, req);
    if (r) { free(r); return false; }
    return true;
}

/* ── _ws_request_reason ─────────────────────────────────────────────────── */
/* PoP: ws_auth_request_reason @ hermes_cli/web_server.py:_ws_request_reason */
char *ws_auth_request_reason(const ws_auth_state_t *st,
                             const ws_upgrade_req_t *req) {
    char *r = ws_auth_host_origin_reason(st, req);
    if (r) return r;
    return ws_auth_client_reason(st, req);
}

/* ── _ws_request_is_allowed ─────────────────────────────────────────────── */
/* PoP: ws_auth_request_is_allowed @ hermes_cli/web_server.py:_ws_request_is_allowed */
bool ws_auth_request_is_allowed(const ws_auth_state_t *st,
                                const ws_upgrade_req_t *req) {
    return ws_auth_host_origin_is_allowed(st, req) &&
           ws_auth_client_is_allowed(st, req);
}

/* ── _ws_auth_mode ──────────────────────────────────────────────────────── */
/* PoP: ws_auth_mode @ hermes_cli/web_server.py:_ws_auth_mode */
const char *ws_auth_mode(const ws_auth_state_t *st) {
    if (st->auth_required) return "gated";
    char *bound = norm_bound_host(st->bound_host);
    bool insecure = *bound && !is_loopback_host(bound);
    free(bound);
    return insecure ? "insecure" : "loopback";
}

/* constant-time compare (hmac.compare_digest semantics) */
static bool const_time_eq(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    unsigned char diff = (unsigned char)(la != lb);
    size_t n = la < lb ? la : lb;
    for (size_t i = 0; i < n; i++)
        diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

/* ── _ws_auth_reason ────────────────────────────────────────────────────── */
/* PoP: ws_auth_reason @ hermes_cli/web_server.py:_ws_auth_reason */
const char *ws_auth_reason(const ws_auth_state_t *st,
                           const ws_upgrade_req_t *req,
                           const char **credential) {
    const char *cred = "none";
    const char *reason = NULL;
    if (st->auth_required) {
        const char *internal = req->q_internal ? req->q_internal : "";
        if (*internal) {
            char *who = ws_tickets_consume_internal_credential(internal);
            if (who) { free(who); cred = "internal"; reason = NULL; goto out; }
            cred = "internal";
            reason = "internal_invalid";
            goto out;
        }
        const char *ticket = req->q_ticket ? req->q_ticket : "";
        if (!*ticket) { cred = "none"; reason = "no_credential"; goto out; }
        char *info = ws_tickets_consume_ticket(ticket);
        if (info) { free(info); cred = "ticket"; reason = NULL; goto out; }
        cred = "ticket";
        reason = "ticket_invalid";
        goto out;
    }
    {
        const char *token = req->q_token ? req->q_token : "";
        if (!*token) { cred = "none"; reason = "no_credential"; goto out; }
        if (const_time_eq(token, st->session_token ? st->session_token : "")) {
            cred = "token";
            reason = NULL;
            goto out;
        }
        cred = "token";
        reason = "token_mismatch";
    }
out:
    if (credential) *credential = cred;
    return reason;
}

/* ── _ws_auth_ok ────────────────────────────────────────────────────────── */
/* PoP: ws_auth_ok @ hermes_cli/web_server.py:_ws_auth_ok */
bool ws_auth_ok(const ws_auth_state_t *st, const ws_upgrade_req_t *req) {
    return ws_auth_reason(st, req, NULL) == NULL;
}

/* ── _has_valid_query_token ─────────────────────────────────────────────── */
/* _QUERY_TOKEN_API_PATHS = frozenset({"/api/files/download"}) */
/* PoP: ws_auth_has_valid_query_token @ hermes_cli/web_server.py:_has_valid_query_token */
bool ws_auth_has_valid_query_token(const ws_auth_state_t *st,
                                   const char *path, const char *q_token) {
    if (!path || strcmp(path, "/api/files/download") != 0) return false;
    if (!q_token || !*q_token) return false;
    return const_time_eq(q_token, st->session_token ? st->session_token : "");
}
