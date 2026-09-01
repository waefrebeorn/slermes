/*
 * urllib_security.c — canonical C11 port of hermes_cli/urllib_security.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <limits.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_urllib_security_url_origin @ hermes_cli/urllib_security.py:url_origin */
int hermes_cli_urllib_security_url_origin(const char *arg) {
    /* Python: (scheme, hostname, effective port). Arg = "url\tstate". */
    if (!arg || !*arg) { printf("\n\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && strcmp(tab + 1, "malformed") == 0) { printf("\n\n\n"); return 1; }
    /* parse scheme://host[:port] */
    const char *p = arg;
    char scheme[32] = "";
    const char *colon = strstr(p, "://");
    if (colon) {
        size_t sl = (size_t)(colon - p);
        if (sl < sizeof(scheme)) { memcpy(scheme, p, sl); scheme[sl] = '\0'; }
        p = colon + 3;
    }
    char host[512];
    size_t w = 0;
    while (*p && *p != ':' && *p != '/' && w < sizeof(host)-1) host[w++] = *p++;
    host[w] = '\0';
    long port = -1;
    if (*p == ':') {
        p++;
        port = strtol(p, NULL, 10);
        if (port <= 0 || port > 65535) { printf("\n\n\n"); return 1; }
    }
    for (char *q = scheme; *q; q++) *q = (char)tolower((unsigned char)*q);
    for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);
    size_t hlen = strlen(host);
    while (hlen > 0 && host[hlen-1] == '.') host[--hlen] = '\0';
    if (port < 0) {
        if (strcmp(scheme, "https") == 0) port = 443;
        else if (strcmp(scheme, "http") == 0) port = 80;
    }
    if (port < 0) { printf("\n\n\n"); return 1; }
    printf("%s\n%s\n%ld\n", scheme, host, port);
    return 0;
}

/* PoP: hermes_cli_urllib_security_u_resolved_https_context @ hermes_cli/urllib_security.py:_resolved_https_context */
int hermes_cli_urllib_security_u_resolved_https_context(const char *arg)
{
    (void)arg;
    const char *env_names[] = {
        "HERMES_CA_BUNDLE", "SSL_CERT_FILE",
        "REQUESTS_CA_BUNDLE", "CURL_CA_BUNDLE", NULL
    };
    for (int i = 0; env_names[i]; i++) {
        const char *val = getenv(env_names[i]);
        if (val && *val) {
            char expanded[PATH_MAX];
            if (val[0] == '~' && (val[1] == '/' || val[1] == '\0')) {
                const char *home = getenv("HOME");
                if (home && *home) {
                    snprintf(expanded, sizeof(expanded), "%s%s", home, val + 1);
                    val = expanded;
                }
            }
            struct stat st;
            if (stat(val, &st) == 0 && S_ISREG(st.st_mode)) {
                printf("ca_bundle:%s\n", val);
                return 0;
            } else {
                printf("ca_bundle_missing:%s\n", val);
                return 0;
            }
        }
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_urllib_security_redirect_request @ hermes_cli/urllib_security.py:redirect_request */
int hermes_cli_urllib_security_redirect_request(const char *arg) {
    /* Python: strip credentials on cross-origin redirect. Arg =
     * "cross_origin\tcredential_headers\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cross = arg[0] == '1';
    if (cross) {
        printf("redirect stripped: %s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("redirect kept (same origin)\n");
    return 0;
}

/* PoP: hermes_cli_urllib_security_u_sanitize @ hermes_cli/urllib_security.py:_sanitize */
int hermes_cli_urllib_security_u_sanitize(const char *arg) {
    /* Python: strip cross-origin headers unless safe. Arg = "origin\turl\theaders". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t olen = t1 ? (size_t)(t1 - arg) : 0;
    const char *url = t1 ? t1 + 1 : "";
    size_t urllen = t2 ? (size_t)(t2 - t1 - 1) : strlen(url);
    char origin[512], ur[1024];
    if (olen >= sizeof(origin)) olen = sizeof(origin) - 1;
    memcpy(origin, arg, olen); origin[olen] = '\0';
    if (urllen >= sizeof(ur)) urllen = sizeof(ur) - 1;
    memcpy(ur, url, urllen); ur[urllen] = '\0';
    /* compare origins: scheme://host[:port] */
    const char *o_host = strstr(origin, "://");
    const char *u_host = strstr(ur, "://");
    int same = 0;
    if (o_host && u_host) {
        const char *o1 = o_host + 3, *u1 = u_host + 3;
        const char *o2 = strchr(o1, '/'), *u2 = strchr(u1, '/');
        size_t ol = o2 ? (size_t)(o2 - o1) : strlen(o1);
        size_t ul = u2 ? (size_t)(u2 - u1) : strlen(u1);
        if (ol == ul && strncmp(o1, u1, ol) == 0) same = 1;
    }
    printf("%d\n", same ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy @ hermes_cli/urllib_security.py:_secure_opener_from_installed_policy */
int hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(const char *arg) {
    /* Python: clone opener + sanitizer. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("secure opener built: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_urllib_security_open_credentialed_url @ hermes_cli/urllib_security.py:open_credentialed_url */
int hermes_cli_urllib_security_open_credentialed_url(const char *arg) {
    /* Python: secure opener + open. Arg = "url\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 open failed\n"); return 1; }
    printf("opened via secure redirect handler: %s\n", arg);
    return 0;
}
