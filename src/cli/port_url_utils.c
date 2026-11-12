/*
 * port_url_utils.c — pure helpers from hermes_cli/url_utils.py.
 */

#include "port_url_utils.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Python str.replace('%', '%25'). */
static char *escape_pct(const char *s)
{
    size_t extra = 0;
    for (const char *p = s; *p; p++)
        if (*p == '%') extra += 2;
    char *out = (char *)malloc(strlen(s) + extra + 1);
    if (!out) return NULL;
    char *o = out;
    for (const char *p = s; *p; p++) {
        if (*p == '%') { *o++ = '%'; *o++ = '2'; *o++ = '5'; }
        else *o++ = *p;
    }
    *o = '\0';
    return out;
}

/* "[" + escape_pct(s) + "]" — bracket an IPv6 literal. */
static char *bracket_ipv6(const char *s)
{
    char *esc = escape_pct(s);
    if (!esc) return NULL;
    size_t elen = strlen(esc);
    char *out = (char *)malloc(elen + 3);
    if (!out) { free(esc); return NULL; }
    out[0] = '[';
    memcpy(out + 1, esc, elen);
    out[elen + 1] = ']';
    out[elen + 2] = '\0';
    free(esc);
    return out;
}

/* PoP: format_url_host @ hermes_cli/url_utils.py:format_url_host */
char *format_url_host(const char *host)
{
    if (!host)
        return NULL;

    /* host.strip() */
    const char *s = host;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;

    char *h = (char *)malloc(len + 1);
    if (!h) return NULL;
    memcpy(h, s, len);
    h[len] = '\0';

    int bracketed = (len >= 2 && h[0] == '[' && h[len - 1] == ']');
    char *inner = NULL;
    const char *candidate = h;
    if (bracketed) {
        inner = (char *)malloc(len - 1);
        if (!inner) { free(h); return NULL; }
        memcpy(inner, h + 1, len - 2);
        inner[len - 2] = '\0';
        candidate = inner;
    }

    int has_colon = (strchr(candidate, ':') != NULL);
    char *result;
    if (bracketed) {
        if (has_colon)
            result = bracket_ipv6(candidate);
        else { result = h; h = NULL; }        /* already bracketed: unchanged */
    } else if (has_colon) {
        result = bracket_ipv6(candidate);
    } else {
        result = h; h = NULL;                 /* plain host: stripped form */
    }
    free(inner);
    free(h);
    return result;
}
