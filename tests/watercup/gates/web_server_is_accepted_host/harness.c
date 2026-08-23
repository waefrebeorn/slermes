/* gate: web_server_is_accepted_host
 * Faithful traced copy of src/hermes_cli/web_server_auth.c::ws_is_accepted_host
 * (LIVE twin: hermes_cli/web_server.py::_is_accepted_host).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: null host_header/bound_host            -> false
 *   V1: trimmed host header
 *   B2: bracketed IPv6 form?
 *   B3: ']' found?                             (bracket path)
 *   V2: host_only (lowercased)
 *   B4: bound is 0.0.0.0 / ::  -> true
 *   B5: bound is loopback -> B6 host_only loopback?
 *   B7: exact host match
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static const char *k_loopback_hosts[] = { "localhost", "127.0.0.1", "::1", NULL };

static bool is_loopback_value(const char *h) {
    if (!h) return false;
    for (int i = 0; k_loopback_hosts[i]; i++)
        if (strcasecmp(h, k_loopback_hosts[i]) == 0) return true;
    return false;
}

static bool ws_is_accepted_host(const char *host_header, const char *bound_host,
                                char *vbuf, size_t vcap) {
    WCUP_BEGIN("ws_is_accepted_host");
    bool result = false;
    if (!host_header || !bound_host) { WCUP_BRANCH(1); goto emit; }
    WCUP_BRANCH(0);

    /* strip trailing whitespace (LIVE strips both ends via .strip()) */
    char hbuf[256];
    size_t n = strlen(host_header);
    if (n >= sizeof(hbuf)) n = sizeof(hbuf) - 1;
    memcpy(hbuf, host_header, n); hbuf[n] = '\0';
    while (n > 0 && (hbuf[n-1]==' '||hbuf[n-1]=='\t'||hbuf[n-1]=='\r'||hbuf[n-1]=='\n'))
        hbuf[--n] = '\0';
    snprintf(vbuf, vcap, "%s", hbuf);
    WCUP_VALSTR(vbuf);

    char host_only[256];
    if (hbuf[0] == '[') {
        WCUP_BRANCH(1);
        char *close = strchr(hbuf, ']');
        if (close) {
            WCUP_BRANCH(1);
            size_t len = (size_t)(close - (hbuf + 1));
            if (len >= sizeof(host_only)) len = sizeof(host_only) - 1;
            memcpy(host_only, hbuf + 1, len); host_only[len] = '\0';
        } else {
            WCUP_BRANCH(0);
            size_t len = strlen(hbuf) - 2;
            if (len >= sizeof(host_only)) len = sizeof(host_only) - 1;
            memcpy(host_only, hbuf + 1, len); host_only[len] = '\0';
        }
    } else {
        WCUP_BRANCH(0);
        char *colon = strrchr(hbuf, ':');
        if (colon) *colon = '\0';
        strncpy(host_only, hbuf, sizeof(host_only) - 1);
        host_only[sizeof(host_only) - 1] = '\0';
    }
    for (char *p = host_only; *p; p++) *p = (char)tolower((unsigned char)*p);
    snprintf(vbuf, vcap, "%s", host_only);
    WCUP_VALSTR(vbuf);

    /* B4: all-interfaces bind */
    if (strcasecmp(bound_host, "0.0.0.0") == 0 || strcasecmp(bound_host, "::") == 0) {
        WCUP_BRANCH(1);
        result = true;
        goto emit;
    }
    WCUP_BRANCH(0);

    char bound_lc[256];
    strncpy(bound_lc, bound_host, sizeof(bound_lc) - 1);
    bound_lc[sizeof(bound_lc) - 1] = '\0';
    for (char *p = bound_lc; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* B5: loopback bind */
    if (is_loopback_value(bound_lc)) {
        WCUP_BRANCH(1);
        result = is_loopback_value(host_only);   /* B6 */
        WCUP_BRANCH(result);
        goto emit;
    }
    WCUP_BRANCH(0);

    /* B7: exact match */
    result = strcmp(host_only, bound_lc) == 0;
    WCUP_BRANCH(result);

emit:
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_is_accepted_host");
out:
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_is_accepted_host \"<header>\\t<bound>\"...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", argv[i]);
        char *tab = strchr(tmp, '\t');
        if (!tab) { fprintf(stderr, "bad input (need tab): %s\n", argv[i]); return 2; }
        *tab = '\0';
        char vbuf[512];
        bool r = ws_is_accepted_host(tmp, tab + 1, vbuf, sizeof vbuf);
        (void)r;
    }
    return 0;
}
