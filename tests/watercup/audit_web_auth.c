/* tests/watercup/audit_web_auth.c — water-cup audit harness for the WEB
 * subsystem's auth gate.
 *
 * Every function body here is a FAITHFUL, character-for-character copy of the
 * control structure compiled into src/web_dashboard.c (the live dashboard
 * server). The only additions are the WCUP_* tracer calls. This is the exact
 * audit pattern the demo established (demo_c.c mirrors format_cost_label):
 * we pour the live control flow through the water-cup and compare the carved
 * shape against LIVE Python's twin.
 *
 * C twins (mirrored from src/web_dashboard.c):
 *   is_public_path()       -> LIVE python hermes_cli/dashboard_auth/public_paths.py:PUBLIC_API_PATHS
 *   is_loopback_client()   -> LIVE python hermes_cli/web_server.py:should_require_auth (loopback set)
 *   has_valid_token()      -> LIVE python hermes_cli/web_server.py:_has_valid_session_token
 *
 * Run: cc -I . audit_web_auth.c -o audit_web_auth && ./audit_web_auth <fn> <arg> ...
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

/* --- faithful copy of web_dashboard.c::is_public_path ----------------------
 * CURRENT COMPILED SET (6 entries):
 *   /api/health, /health, /api/status, /api/version, /api/token, /api/platforms
 * LIVE PYTHON SET (8 entries) in dashboard_auth/public_paths.py:
 *   /api/health, /api/status, /api/config/defaults, /api/config/schema,
 *   /api/model/info, /api/dashboard/themes, /api/dashboard/plugins, /api/cron/fire
 * The members are NOT the same set -> behavioral divergence (a FAP).
 */
static bool is_public_path(const char *path) {
    WCUP_BEGIN("is_public_path");
    WCUP_VALSTR(path ? path : "(null)");
    bool r = false;
    if (!path) { WCUP_BRANCH(0); }
    else {
        WCUP_BRANCH(1);
        if (strcmp(path, "/api/health") == 0) { WCUP_BRANCH(1); r = true; }
        else { WCUP_BRANCH(0);
        if (strcmp(path, "/health") == 0) { WCUP_BRANCH(1); r = true; }
        else { WCUP_BRANCH(0);
        if (strcmp(path, "/api/status") == 0) { WCUP_BRANCH(1); r = true; }
        else { WCUP_BRANCH(0);
        if (strcmp(path, "/api/version") == 0) { WCUP_BRANCH(1); r = true; }
        else { WCUP_BRANCH(0);
        if (strcmp(path, "/api/token") == 0) { WCUP_BRANCH(1); r = true; }
        else { WCUP_BRANCH(0);
        if (strcmp(path, "/api/platforms") == 0) { WCUP_BRANCH(1); r = true; }
        else { WCUP_BRANCH(0); } } } } } }
    }
    WCUP_VALSTR(r ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_public_path");
    return r;
}

/* --- faithful copy of web_dashboard.c::is_loopback_client ------------------
 * CURRENT COMPILED: IPv4 127.0.0.0/8 ONLY. No IPv6 ::1 handling.
 * LIVE PYTHON: host in {"localhost","127.0.0.1","::1"} -> loopback.
 */
static bool is_loopback_client(unsigned int client_ip) {
    WCUP_BEGIN("is_loopback_client");
    char ipb[32];
    snprintf(ipb, sizeof ipb, "%u", client_ip);
    WCUP_VALSTR(ipb);
    /* 127.0.0.0/8 */
    bool r = (client_ip & 0xFF000000u) == 0x7F000000u;
    WCUP_BRANCH(r);
    /* (no IPv6 path exists in C — this is the divergence surface) */
    WCUP_VALSTR(r ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_loopback_client");
    return r;
}

/* --- faithful copy of web_dashboard.c::has_valid_token ---------------------
 * CURRENT COMPILED: plain strncmp (NOT timing-safe).
 * LIVE PYTHON: hmac.compare_digest (constant-time).
 * We trace the same branch shape; the divergence is the comparison primitive.
 */
static bool has_valid_token(const char *headers, const char *token) {
    WCUP_BEGIN("has_valid_token");
    WCUP_VALSTR(headers ? headers : "(null)");
    WCUP_VALSTR(token ? token : "(null)");
    bool r = false;
    if (!headers || !token || !token[0]) { WCUP_BRANCH(0); }
    else {
        WCUP_BRANCH(1);
        const char *hdr = strstr(headers, "X-Hermes-Session-Token:");
        if (!hdr) hdr = strstr(headers, "x-hermes-session-token:");
        if (hdr) {
            WCUP_BRANCH(1);
            hdr = strchr(hdr, ':');
            if (hdr) {
                WCUP_BRANCH(1);
                hdr++;
                while (*hdr == ' ') hdr++;
                if (strncmp(hdr, token, strlen(token)) == 0) {
                    WCUP_BRANCH(1);
                    char after = hdr[strlen(token)];
                    if (after == '\0' || after == '\r' || after == '\n' || after == ' ') {
                        WCUP_BRANCH(1);
                        r = true;
                    } else { WCUP_BRANCH(0); }
                } else { WCUP_BRANCH(0); }
            } else { WCUP_BRANCH(0); }
        } else { WCUP_BRANCH(0); }

        if (!r) {
            WCUP_BRANCH(0);
            const char *auth = strstr(headers, "Authorization:");
            if (!auth) auth = strstr(headers, "authorization:");
            if (auth) {
                WCUP_BRANCH(1);
                auth = strchr(auth, ':');
                if (auth) {
                    WCUP_BRANCH(1);
                    auth++;
                    while (*auth == ' ') auth++;
                    if (strncmp(auth, "Bearer ", 7) == 0) {
                        WCUP_BRANCH(1);
                        auth += 7;
                        if (strncmp(auth, token, strlen(token)) == 0) {
                            WCUP_BRANCH(1);
                            char after = auth[strlen(token)];
                            if (after == '\0' || after == '\r' || after == '\n' || after == ' ') {
                                WCUP_BRANCH(1);
                                r = true;
                            } else { WCUP_BRANCH(0); }
                        } else { WCUP_BRANCH(0); }
                    } else { WCUP_BRANCH(0); }
                } else { WCUP_BRANCH(0); }
            } else { WCUP_BRANCH(0); }
        } else { WCUP_BRANCH(1); }
    }
    WCUP_VALSTR(r ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("has_valid_token");
    return r;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <fn> <arg> [arg2 ...]\n", argv[0]);
        return 2;
    }
    const char *fn = argv[1];
    int retmode = (getenv("RET_ONLY") != NULL);
    if (strcmp(fn, "is_public_path") == 0) {
        for (int i = 2; i < argc; i++) { bool r = is_public_path(argv[i]); if (retmode) printf("%d\n", r?1:0); }
    } else if (strcmp(fn, "is_loopback_client") == 0) {
        for (int i = 2; i < argc; i++) { bool r = is_loopback_client((unsigned int)strtoul(argv[i], NULL, 10)); if (retmode) printf("%d\n", r?1:0); }
    } else if (strcmp(fn, "has_valid_token") == 0) {
        for (int i = 2; i < argc; i++) {
            char *p = strchr(argv[i], '|');
            if (p) { *p = '\0'; bool r = has_valid_token(argv[i], p + 1); if (retmode) printf("%d\n", r?1:0); }
            else { bool r = has_valid_token(argv[i], ""); if (retmode) printf("%d\n", r?1:0); }
        }
    } else {
        fprintf(stderr, "unknown fn %s\n", fn);
        return 4;
    }
    return 0;
}
