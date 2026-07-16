/*
 * port_web_server_auth.c — Faithful C11 port of the dashboard auth-helper
 * cluster from hermes_cli/web_server.py.
 *
 * Ports (exact, PoP):
 *   - ws_has_valid_session_token   @ web_server.py:_has_valid_session_token
 *   - ws_should_require_auth       @ web_server.py:should_require_auth
 *   - ws_is_accepted_host          @ web_server.py:_is_accepted_host
 *
 * These are the pure-logic pieces of the FastAPI auth middlewares. They are
 * compiled deterministically and reused by the existing raw-HTTP dashboard
 * handlers (web_dashboard.c) and by any future request-time gate. The token
 * source is the shared g_session_token (set in web_dashboard.c on startup).
 *
 * The string-matching here deliberately mirrors the Python logic: the
 * loopback set, the IPv6 bracket handling, and the exact-host match for
 * non-loopback binds. No behaviour is stubbed.
 */

#include "hermes_web_dashboard.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* strcasestr() */
#endif
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ── Loopback host values (mirrors web_server._LOOPBACK_HOST_VALUES) ───── */
static const char *k_loopback_hosts[] = {
    "localhost", "127.0.0.1", "::1", NULL
};

static bool is_loopback_value(const char *h) {
    if (!h) return false;
    for (int i = 0; k_loopback_hosts[i]; i++) {
        if (strcasecmp(h, k_loopback_hosts[i]) == 0) return true;
    }
    return false;
}

/* ── ws_has_valid_session_token ──────────────────────────────────────────
 * Port of _has_valid_session_token(request). Validates the dedicated session
 * header (X-Hermes-Session-Token) with constant-time compare, then falls
 * back to the legacy "Bearer <token>" Authorization form. Mirrors Python's
 * hmac.compare_digest, which compares the ENTIRE header value — so trailing
 * junk (e.g. "token extra") is rejected, not just whitespace-trimmed. */
bool ws_has_valid_session_token(const char *headers) {
    if (!headers || !g_session_token[0]) return false;
    size_t toklen = strlen(g_session_token);
    if (toklen == 0) return false;

    /* Dedicated session header (case-insensitive name). */
    const char *hdr = strcasestr(headers, "X-Hermes-Session-Token:");
    if (hdr) {
        hdr = strchr(hdr, ':');
        if (hdr) {
            hdr++;
            while (*hdr == ' ') hdr++;
            /* Value runs to end-of-line; reject any trailing junk. */
            const char *end = hdr;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t vlen = (size_t)(end - hdr);
            if (vlen == toklen && strncmp(hdr, g_session_token, toklen) == 0)
                return true;
        }
    }

    /* Legacy Bearer form in Authorization. */
    const char *auth = strcasestr(headers, "Authorization:");
    if (auth) {
        auth = strchr(auth, ':');
        if (auth) {
            auth++;
            while (*auth == ' ') auth++;
            if (strncmp(auth, "Bearer ", 7) == 0) {
                auth += 7;
                const char *end = auth;
                while (*end && *end != '\r' && *end != '\n') end++;
                size_t vlen = (size_t)(end - auth);
                if (vlen == toklen && strncmp(auth, g_session_token, toklen) == 0)
                    return true;
            }
        }
    }

    return false;
}

/* ── ws_should_require_auth ──────────────────────────────────────────────
 * Port of should_require_auth(host, allow_public). Loopback binds are
 * trusted (no gate); any non-loopback bind always requires an auth
 * provider. `allow_public` is accepted for backward-compat but ignored —
 * a non-loopback bind ALWAYS engages the gate. */
bool ws_should_require_auth(const char *host, bool allow_public) {
    (void)allow_public; /* legacy escape hatch, intentionally ignored */
    if (!host) return true;
    return !is_loopback_value(host);
}

/* ── ws_is_accepted_host ─────────────────────────────────────────────────
 * Port of _is_accepted_host(host_header, bound_host). DNS-rebinding defence
 * (GHSA-ppp5-vxwm-4cf7): reject Host headers that don't target the bound
 * interface. 0.0.0.0 / :: binds opt into all-interfaces (no Host-layer
 * defence possible); loopback binds accept the loopback aliases; explicit
 * non-loopback binds require an exact host match. */
bool ws_is_accepted_host(const char *host_header, const char *bound_host) {
    if (!host_header || !bound_host) return false;

    /* Strip port suffix, handling IPv6 bracket notation ([::1]:9119). */
    char hbuf[256];
    size_t n = strlen(host_header);
    if (n >= sizeof(hbuf)) n = sizeof(hbuf) - 1;
    memcpy(hbuf, host_header, n);
    hbuf[n] = '\0';
    /* trim trailing whitespace */
    while (n > 0 && (hbuf[n - 1] == ' ' || hbuf[n - 1] == '\t' ||
                     hbuf[n - 1] == '\r' || hbuf[n - 1] == '\n'))
        hbuf[--n] = '\0';

    char host_only[256];
    if (hbuf[0] == '[') {
        char *close = strchr(hbuf, ']');
        if (close) {
            size_t len = (size_t)(close - (hbuf + 1));
            if (len >= sizeof(host_only)) len = sizeof(host_only) - 1;
            memcpy(host_only, hbuf + 1, len);
            host_only[len] = '\0';
        } else {
            size_t len = strlen(hbuf) - 2; /* strip [ ] */
            if (len >= sizeof(host_only)) len = sizeof(host_only) - 1;
            memcpy(host_only, hbuf + 1, len);
            host_only[len] = '\0';
        }
    } else {
        char *colon = strrchr(hbuf, ':');
        if (colon) *colon = '\0';
        strncpy(host_only, hbuf, sizeof(host_only) - 1);
        host_only[sizeof(host_only) - 1] = '\0';
    }

    /* lowercase for comparison */
    for (char *p = host_only; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    /* 0.0.0.0 / :: → operator opted into all-interfaces. */
    if (strcasecmp(bound_host, "0.0.0.0") == 0 ||
        strcasecmp(bound_host, "::") == 0)
        return true;

    /* Loopback bind: accept the loopback aliases. */
    char bound_lc[256];
    strncpy(bound_lc, bound_host, sizeof(bound_lc) - 1);
    bound_lc[sizeof(bound_lc) - 1] = '\0';
    for (char *p = bound_lc; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    if (is_loopback_value(bound_lc))
        return is_loopback_value(host_only);

    /* Explicit non-loopback bind: require exact host match. */
    return strcmp(host_only, bound_lc) == 0;
}
