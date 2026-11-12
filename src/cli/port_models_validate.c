/*
 * port_models_validate.c — Port of hermes_cli/models_validate.py::_non_public_host:
 * classify a host as a user's own endpoint rather than a vendor catalog host.
 * Loopback, LAN, single-label and RFC1918-ish IPv4 addresses all qualify.
 */

#include "port_models_validate.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + (ls - lf), suffix) == 0;
}

/* PoP: mv_non_public_host @ hermes_cli/models_validate.py:_non_public_host */
int mv_non_public_host(const char *host)
{
    char buf[512];
    size_t n = 0;

    const char *p = host ? host : "";
    for (; *p && n + 1 < sizeof(buf); p++)
        buf[n++] = (char)tolower((unsigned char)*p);
    while (n > 0 && buf[n - 1] == '.')
        n--;                       /* host.rstrip(".") */
    buf[n] = '\0';

    if (n == 0)
        return 0;                  /* bool("") -> False */
    if (strcmp(buf, "localhost") == 0 || strcmp(buf, "127.0.0.1") == 0 ||
        strcmp(buf, "::1") == 0 || strcmp(buf, "0.0.0.0") == 0 ||
        ends_with(buf, ".localhost"))
        return 1;
    if (ends_with(buf, ".local") || ends_with(buf, ".lan") ||
        ends_with(buf, ".internal") || ends_with(buf, ".home") ||
        ends_with(buf, ".localdomain"))
        return 1;
    if (!strchr(buf, '.'))
        return 1;                  /* single-label host */

    /* host.split(".") — only a bare 4-octet all-digit form can be an IPv4 literal */
    char *parts[8];
    int np = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ".", &save); tok && np < 8;
         tok = strtok_r(NULL, ".", &save))
        parts[np++] = tok;

    if (np == 4) {
        int all_digits = 1;
        int octets[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4 && all_digits; i++) {
            if (!parts[i][0]) {
                all_digits = 0;
                break;
            }
            for (const char *q = parts[i]; *q; q++) {
                if (!isdigit((unsigned char)*q)) {
                    all_digits = 0;
                    break;
                }
            }
            if (all_digits)
                octets[i] = atoi(parts[i]);
        }
        if (all_digits) {
            if (octets[0] == 10 || octets[0] == 127)
                return 1;
            if (octets[0] == 192 && octets[1] == 168)
                return 1;
            if (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31)
                return 1;
        }
    }
    return 0;
}
