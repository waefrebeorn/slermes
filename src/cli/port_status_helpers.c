/*
 * port_status_helpers.c
 *
 * Faithful C11 port of the PURE helper from hermes_cli/status.py:
 *   _format_iso_timestamp  ->  sta_format_iso_timestamp
 *
 * Converts an ISO-8601 timestamp string to the local timezone and formats
 * it as "%Y-%m-%d %H:%M:%S %Z". Pure datetime logic -- no I/O, no
 * network, no env/catalog reads. Carries its PoP annotation.
 *
 * Behaviour (mirrors the Python exactly):
 *   - non-string or empty/whitespace value  -> "(unknown)"
 *   - trailing "Z" is normalized to "+00:00"
 *   - naive timestamps (no offset) are assumed UTC
 *   - unparseable -> returns the original string unchanged
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* PoP: sta_format_iso_timestamp @ hermes_cli/status.py:_format_iso_timestamp */
void sta_format_iso_timestamp(const char *value, char out[64])
{
    out[0] = '\0';
    if (!value || !*value) { strcpy(out, "(unknown)"); return; }

    const char *orig = value;   /* Python echoes the original on parse failure */

    /* strip leading/trailing whitespace */
    while (*value == ' ' || *value == '\t') value++;
    char buf[64];
    size_t n = 0;
    for (; *value && n < sizeof(buf) - 1; value++) buf[n++] = *value;
    buf[n] = '\0';
    while (n > 0 && (buf[n-1] == ' ' || buf[n-1] == '\t')) buf[--n] = '\0';
    if (n == 0) { strcpy(out, "(unknown)"); return; }

    /* normalize trailing Z -> +00:00 */
    if (n > 0 && buf[n-1] == 'Z') {
        if (n + 5 < sizeof(buf)) {
            buf[n-1] = '+'; buf[n] = '0'; buf[n+1] = '0';
            buf[n+2] = ':'; buf[n+3] = '0'; buf[n+4] = '0';
            buf[n+5] = '\0'; n += 5;
        }
    }

    /* parse YYYY-MM-DD [T| ] HH:MM[:SS] [+HH:MM | -HH:MM] */
    int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
    int off_sign = 0, off_h = 0, off_m = 0;

    if (sscanf(buf, "%d-%d-%d", &Y, &M, &D) != 3) { strcpy(out, orig); return; }

    /* locate the time substring (after the date, past a T or space) */
    char *ts = NULL;
    for (char *q = buf; *q; q++) {
        if ((*q == 'T' || *q == ' ') && isdigit((unsigned char)*(q+1))) { ts = q + 1; break; }
    }
    if (ts) sscanf(ts, "%d:%d:%d", &h, &m, &s);

    /* locate the offset (last standalone + or - after the date part) */
    char *off = NULL;
    for (char *q = buf + 4; *q; q++) {
        if ((*q == '+' || *q == '-') &&
            (isdigit((unsigned char)*(q+1)) || (isdigit((unsigned char)*(q+1)) && *(q+2) == ':')))
            off = q;
    }
    if (off && sscanf(off, "%c%d:%d", (char*)&off_sign, &off_h, &off_m) >= 3) {
        /* parsed */
    } else {
        off_sign = 0; off_h = 0; off_m = 0;
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = Y - 1900;
    tm.tm_mon  = M - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min  = m;
    tm.tm_sec  = s;
    tm.tm_isdst = -1;

    /* epoch assuming this wall-clock is UTC, then subtract the offset.
     * naive / Z => offset seconds == 0 => treated as UTC. */
    time_t epoch = timegm(&tm);
    if (epoch == (time_t)-1) { strcpy(out, orig); return; }
    int off_secs = (off_sign == '-')
        ? -(off_h * 3600 + off_m * 60)
        :  (off_h * 3600 + off_m * 60);
    epoch -= off_secs;

    struct tm local;
    localtime_r(&epoch, &local);
    strftime(out, 64, "%Y-%m-%d %H:%M:%S %Z", &local);
}
