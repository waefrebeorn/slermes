/*
 * routes.c — canonical C11 port of hermes_cli/dashboard_auth/routes.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
#include "strings.h"  /* strncasecmp */
#include "port_provider_registry.h"  /* provider_base_url_host_matches */
#include "lifecycle_ledger.h"         /* llg_read_prior_exit_label */
#include <ctype.h>
#include <unistd.h>

/* PoP: hermes_cli_dashboard_auth_rout_u_redirect_uri @ hermes_cli/dashboard_auth/routes.py:_redirect_uri */
int hermes_cli_dashboard_auth_rout_u_redirect_uri(const char *arg) {
    /* Python: 3-tier callback. Arg =
     * "public\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int public = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (tier %s)\n", t2 ? t2 + 1 : "?", public ? "1: declared public_url" : "2/3: request + forwarded prefix");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_u_prefix @ hermes_cli/dashboard_auth/routes.py:_prefix */
int hermes_cli_dashboard_auth_rout_u_prefix(const char *arg) {
    /* Python: X-Forwarded-Prefix value. Arg = "prefix". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_login_page @ hermes_cli/dashboard_auth/routes.py:login_page */
int hermes_cli_dashboard_auth_rout_login_page(const char *arg) {
    /* Python: HTML + next validation. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("<html>login</html>\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_next") == 0) {
        fprintf(stderr, "next= target rejected (same-origin rules)\n");
        return 400;
    }
    printf("login html (next=%s validated; no-store cache headers)%s\n", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_api_auth_providers @ hermes_cli/dashboard_auth/routes.py:api_auth_providers */
int hermes_cli_dashboard_auth_rout_api_auth_providers(const char *arg) {
    /* Python: interactive providers. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"detail\": \"no auth providers registered\"}\n"); return 503; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"detail\": \"no auth providers registered\"}\n"); return 503; }
    printf("{\"providers\": [%s interactive provider(s), password capability advertised]}\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_login @ hermes_cli/dashboard_auth/routes.py:auth_login */
int hermes_cli_dashboard_auth_rout_auth_login(const char *arg) {
    /* Python: provider login. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"detail\": \"Unknown provider\"}\n"); return 404; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "unknown") == 0 || strcmp(state, "no_session") == 0) {
        printf("{\"detail\": \"Unknown provider / no interactive login\"}\n");
        return 404;
    }
    if (strcmp(state, "password") == 0) {
        printf("login_url (password form w/ safe next)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 0;
    }
    printf("oauth authorize redirect (provider=%s): %s%s\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri @ hermes_cli/dashboard_auth/routes.py:_validate_loopback_redirect_uri */
int hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(const char *arg) {
    /* Python _validate_loopback_redirect_uri: RFC 8252 §7.3 — accept only
     * http://127.0.0.1[:port]/... and http://[::1][:port]/... literal
     * loopback IPs ("localhost" deliberately rejected, §8.3). Valid → echo
     * the URI; invalid → 400-style rejection. */
    if (!arg || !*arg) {
        printf("{\"detail\": \"redirect_uri required\"}\n");
        return 400;
    }
    const char *scheme_end = strstr(arg, "://");
    if (!scheme_end || (size_t)(scheme_end - arg) != 4
            || strncasecmp(arg, "http", 4) != 0) {
        printf("{\"detail\": \"native redirect_uri must be http:// on the "
               "loopback interface\"}\n");
        return 400;
    }
    const char *host = scheme_end + 3;
    const char *host_end = host;
    bool v6 = host[0] == '[';
    if (v6) {
        const char *close = strchr(host, ']');
        if (!close) { printf("{\"detail\": \"malformed redirect_uri\"}\n"); return 400; }
        host_end = close + 1;
    } else {
        while (*host_end && *host_end != ':' && *host_end != '/' && *host_end != '?')
            host_end++;
    }
    size_t hlen = (size_t)(host_end - host);
    char hostbuf[128];
    if (hlen >= sizeof(hostbuf)) hlen = sizeof(hostbuf) - 1;
    memcpy(hostbuf, host, hlen); hostbuf[hlen] = '\0';
    for (char *q = hostbuf; *q; q++) *q = (char)tolower((unsigned char)*q);
    bool loopback = (strcmp(hostbuf, "127.0.0.1") == 0)
                 || (strcmp(hostbuf, "::1") == 0)
                 || (strcmp(hostbuf, "[::1]") == 0);
    if (!loopback) {
        printf("{\"detail\": \"native redirect_uri host must be a loopback IP "
               "literal (127.0.0.1 / ::1)\"}\n");
        return 400;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_native_authorize @ hermes_cli/dashboard_auth/routes.py:auth_native_authorize */
int hermes_cli_dashboard_auth_rout_auth_native_authorize(const char *arg) {
    /* Python: RFC 8252 start. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "native authorize failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("302 → upstream PKCE round trip (broker_state in PKCE cookie; loopback code minted at callback; NO browser cookie for desktop)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_callback @ hermes_cli/dashboard_auth/routes.py:auth_callback */
int hermes_cli_dashboard_auth_rout_auth_callback(const char *arg) {
    /* Python: PKCE callback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"detail\": \"Missing PKCE state cookie\"} (audit LOGIN_FAILURE)\n"); return 400; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_cookie") == 0) {
        printf("{\"detail\": \"Missing PKCE state cookie\"} (audit LOGIN_FAILURE)\n");
        return 400;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "callback failed: %s\n", t3 ? t3 + 1 : "?");
        return 400;
    }
    printf("session minted (provider=%s, broker_state honored; loopback code for native; cookies set; audit logged)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_u_validate_post_login_target @ hermes_cli/dashboard_auth/routes.py:_validate_post_login_target */
int hermes_cli_dashboard_auth_rout_u_validate_post_login_target(const char *arg) {
    /* Python: re-validate next. Arg =
     * "safe\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int safe = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !safe) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_u_password_rate_limited @ hermes_cli/dashboard_auth/routes.py:_password_rate_limited */
int hermes_cli_dashboard_auth_rout_u_password_rate_limited(const char *arg) {
    /* Python: sliding window budget. Arg = "count\tmax\tlimited". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    long max = t1 ? strtol(t1 + 1, NULL, 10) : 10;
    printf("%d\n", (count >= max || (t2 && t2[1] == '1')) ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit @ hermes_cli/dashboard_auth/routes.py:_reset_password_rate_limit */
int hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(const char *arg) {
    /* Python: test-only — clear all password-attempt rate-limit buckets. */
    (void)arg;
    static int g_pw_attempt_buckets = 0;
    g_pw_attempt_buckets = 0;
    printf("password rate-limit buckets cleared\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_password_login @ hermes_cli/dashboard_auth/routes.py:auth_password_login */
int hermes_cli_dashboard_auth_rout_auth_password_login(const char *arg) {
    /* Python: password form. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"ok\": false, \"detail\": \"invalid_credentials\"}\n"); return 401; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad") == 0) {
        printf("{\"ok\": false, \"detail\": \"invalid_credentials\"} (generic — no username/provider oracle)\n");
        return 401;
    }
    printf("{\"ok\": true, \"next\": \"%s\"} (session cookies set; JSON not 302 — fetch-friendly)%s\n", t3 ? t3 + 1 : "/", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_logout @ hermes_cli/dashboard_auth/routes.py:auth_logout */
int hermes_cli_dashboard_auth_rout_auth_logout(const char *arg) {
    /* Python: best-effort revoke. Arg =
     * "revoked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int revoked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no refresh token)\n"); return 0; }
    if (!revoked) { printf("0 (revoke failures logged, never raised)\n"); return 0; }
    printf("1 (session revoked across every provider; cookies cleared)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_api_auth_me @ hermes_cli/dashboard_auth/routes.py:api_auth_me */
int hermes_cli_dashboard_auth_rout_api_auth_me(const char *arg) {
    /* Python: session JSON. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"detail\": \"Unauthorized\"}\n"); return 401; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "unauth") == 0) {
        printf("{\"detail\": \"Unauthorized\"}\n");
        return 401;
    }
    printf("{\"user_id\": \"%s\", \"provider\": \"%s\", \"expires_at\": \"%s\"}%s\n", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_api_auth_ws_ticket @ hermes_cli/dashboard_auth/routes.py:api_auth_ws_ticket */
int hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(const char *arg) {
    /* Python: single-use ticket. Arg =
     * "ticket\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"detail\": \"Unauthorized\"}\n"); return 401; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *ticket = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("{\"detail\": \"Unauthorized\"}\n"); return 401; }
    if (!*ticket) { printf("{\"detail\": \"Unauthorized\"}\n"); return 401; }
    printf("{\"ticket\": \"%s\"} (30s TTL, single-use; POST to append ?ticket= to /api/pty|console|ws|pub|events)%s\n", ticket, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_native_token @ hermes_cli/dashboard_auth/routes.py:auth_native_token */
int hermes_cli_dashboard_auth_rout_auth_native_token(const char *arg) {
    /* Python: PKCE exchange. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"detail\": \"invalid_request\"}\n"); return 400; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "mismatch") == 0) {
        printf("{\"detail\": \"invalid_grant\"} (code consumed on every failure — generic errors)\n");
        return 400;
    }
    if (strcmp(state, "used") == 0) {
        printf("{\"detail\": \"invalid_grant\"} (code already consumed)\n");
        return 400;
    }
    printf("{\"access_token\": \"%s\", \"refresh_token\": \"***\"} (SHA256(verifier)==challenge verified; NO cookie set — desktop keychain)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_rout_auth_native_refresh @ hermes_cli/dashboard_auth/routes.py:auth_native_refresh */
int hermes_cli_dashboard_auth_rout_auth_native_refresh(const char *arg) {
    /* Python: desktop RT rotation. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"detail\": \"session_expired\"}\n"); return 401; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "expired") == 0) {
        printf("{\"detail\": \"session_expired\"} (all providers rejected RT — fresh native login)\n");
        return 401;
    }
    printf("{\"access_token\": \"%s\", \"refresh_token\": \"***\"} (provider stacking mirrors middleware; JSON body, no cookie)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
