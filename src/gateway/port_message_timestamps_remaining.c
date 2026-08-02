/*
 * port_message_timestamps_remaining.c — Port of gateway/message_timestamps.py
 * timestamp surface. Coercion, formatting, prefix stripping, prefix
 * parsing.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: coerce_message_timestamp @ gateway/message_timestamps.py:coerce_message_timestamp */
long mts_coerce_message_timestamp(const char *value) {
    /* Python: epoch number or iso parse; -1 on failure. */
    if (!value || !*value) return -1;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end != value && *end == '\0' && v > 0) return v;
    /* try iso: YYYY-MM-DD... */
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) >= 3) {
        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_year = y - 1900;
        t.tm_mon = mo - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min = mi;
        t.tm_sec = s;
        time_t tt = timegm(&t);
        if (tt > 0) return (long)tt;
    }
    return -1;
}

/* PoP: format_message_timestamp @ gateway/message_timestamps.py:format_message_timestamp */
char *mts_format_message_timestamp(const char *value) {
    /* Python: [Tue 2026-04-28 13:40:53 CEST]. */
    long epoch = mts_coerce_message_timestamp(value);
    if (epoch < 0) return strdup("");
    time_t t = (time_t)epoch;
    struct tm lt;
    localtime_r(&t, &lt);
    char buf[128];
    strftime(buf, sizeof(buf), "[%a %Y-%m-%d %H:%M:%S %Z]", &lt);
    return strdup(buf);
}

/* PoP: strip_leading_message_timestamps @ gateway/message_timestamps.py:strip_leading_message_timestamps */
char *mts_strip_leading_message_timestamps(const char *content) {
    /* Python: strip one or more leading prefixes. */
    if (!content) return strdup("");
    const char *p = content;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '[') {
            const char *close = strchr(p, ']');
            if (!close) break;
            p = close + 1;
            continue;
        }
        /* iso prefix: 2026-04-28T13:40:53 followed by space */
        int y, mo, d;
        if (sscanf(p, "%d-%d-%d", &y, &mo, &d) == 3) {
            const char *q = p;
            while (*q && *q != ' ') q++;
            if (*q == ' ') { p = q + 1; continue; }
        }
        break;
    }
    return strdup(p);
}

/* PoP: render_user_content_with_timestamp @ gateway/message_timestamps.py:render_user_content_with_timestamp */
char *mts_render_user_content_with_timestamp(const char *content, const char *timestamp) {
    /* Python: exactly one prefix. */
    if (!content) return strdup("");
    char *fmt = mts_format_message_timestamp(timestamp);
    char *out = NULL;
    asprintf(&out, "%s %s", fmt && *fmt ? fmt : "", content);
    free(fmt);
    return out;
}

/* PoP: _parse_timestamp_prefix @ gateway/message_timestamps.py:_parse_timestamp_prefix */
char *mts_parse_timestamp_prefix(const char *text) {
    /* Python: human or iso prefix match. */
    if (!text) return NULL;
    if (text[0] == '[') {
        const char *close = strchr(text, ']');
        if (close) return strndup(text + 1, (size_t)(close - text - 1));
    }
    int y, mo, d, h, mi, s;
    if (sscanf(text, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) >= 3) {
        const char *q = text;
        while (*q && *q != ' ') q++;
        return strndup(text, (size_t)(q - text));
    }
    return NULL;
}

/* PoP: _parse_timestamp_match @ gateway/message_timestamps.py:_parse_timestamp_match */
long mts_parse_timestamp_match(const char *match_text) {
    /* Python: match → epoch. */
    if (!match_text) return -1;
    return mts_coerce_message_timestamp(match_text);
}
