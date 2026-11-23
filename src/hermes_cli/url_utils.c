/*
 * url_utils.c — C port of upstream Hermes hermes_cli/url_utils.py.
 * PoP: urlfmt_format_url_host @ hermes_cli/url_utils.py:format_url_host
 */
#include "noc_urlfmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v';
}
static void trim(const char *s, char *out, size_t cap) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && is_ws(s[a])) a++;
    while (b > a && is_ws(s[b-1])) b--;
    size_t k = 0;
    while (a < b && k + 1 < cap) out[k++] = s[a++];
    out[k] = 0;
}
static char *escape_pct(const char *s) {
    size_t extra = 0;
    for (const char *p = s; *p; p++) if (*p == '%') extra++;
    char *out = (char *)malloc(strlen(s) + 2 * extra + 1);
    size_t k = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '%') { out[k++]='%'; out[k++]='2'; out[k++]='5'; }
        else out[k++] = *p;
    }
    out[k] = 0;
    return out;
}
static char *bracket(const char *inner) {
    char *e = escape_pct(inner);
    size_t cap = strlen(e) + 4;
    char *out = (char *)malloc(cap);
    snprintf(out, cap, "[%s]", e);
    free(e);
    return out;
}
char *urlfmt_format_url_host(const char *host) {
    char h[4096];
    trim(host ? host : "", h, sizeof h);
    size_t n = strlen(h);
    if (n >= 2 && h[0] == '[' && h[n-1] == ']') {
        char inner[4096];
        memcpy(inner, h + 1, n - 2);
        inner[n - 2] = 0;
        if (strchr(inner, ':')) return bracket(inner);
        return strdup(h);
    }
    if (strchr(h, ':')) return bracket(h);
    return strdup(h);
}
