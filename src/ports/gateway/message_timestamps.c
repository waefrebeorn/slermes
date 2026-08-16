#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/message_timestamps.c — C11 port of gateway/message_timestamps.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/gateway/port_gateway_message_timestamps.c ── */
/*
 * port_gateway_message_timestamps.c — Port of Python gateway/message_timestamps.py
 */

#include <stdio.h>
#include "hermes_gateway_core.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _parse_timestamp_match */
typedef struct {
    char timestamp[64];
    int start;
    int end;
    bool found;
} timestamp_match_t;

timestamp_match_t parse_timestamp_match(const char *text) {
    timestamp_match_t result = {0};
    if (!text) return result;
    
    /* Match [HH:MM] or [HH:MM:SS] patterns */
    const char *p = text;
    while (*p) {
        if (*p == '[') {
            const char *close = strchr(p + 1, ']');
            if (close && (close - p) <= 10) {
                /* Check if content looks like a timestamp */
                int digits = 0, colons = 0;
                for (const char *q = p + 1; q < close; q++) {
                    if (*q >= '0' && *q <= '9') digits++;
                    else if (*q == ':') colons++;
                }
                if (digits >= 4 && colons >= 1) {
                    size_t len = close - p - 1;
                    if (len < 63) {
                        memcpy(result.timestamp, p + 1, len);
                        result.timestamp[len] = '\0';
                        result.start = p - text;
                        result.end = close - text;
                        result.found = true;
                    }
                    break;
                }
            }
        }
        p++;
    }
    return result;
}


/* Port of Python: _parse_timestamp_prefix */
bool parse_timestamp_prefix(const char *text, char *timestamp_out, size_t out_sz) {
    if (!text || !timestamp_out || out_sz == 0) return false;
    
    timestamp_match_t match = parse_timestamp_match(text);
    if (!match.found) return false;
    
    strncpy(timestamp_out, match.timestamp, out_sz - 1);
    timestamp_out[out_sz - 1] = '\0';
    return true;
}


/* Port of Python: coerce_message_timestamp */
void coerce_message_timestamp(const char *input, char *output, size_t out_sz) {
    if (!input || !output || out_sz == 0) return;
    
    /* Coerce various timestamp formats to standard [HH:MM] */
    timestamp_match_t match = parse_timestamp_match(input);
    if (match.found) {
        strncpy(output, match.timestamp, out_sz - 1);
    } else {
        /* Use current time */
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        snprintf(output, out_sz, "%02d:%02d", tm->tm_hour, tm->tm_min);
    }
    output[out_sz - 1] = '\0';
}


/* Port of Python: format_message_timestamp */
void format_message_timestamp(const char *timestamp, char *output, size_t out_sz) {
    if (!timestamp || !output || out_sz == 0) return;
    
    /* Format timestamp as [HH:MM] prefix */
    if (strlen(timestamp) < out_sz - 3) {
        snprintf(output, out_sz, "[%s]", timestamp);
    }
}


/* Port of Python: render_user_content_with_timestamp */
void render_user_content_with_timestamp(const char *content, const char *timestamp,
                                         char *output, size_t out_sz) {
    if (!content || !output || out_sz == 0) return;
    
    char ts[64];
    if (timestamp) {
        strncpy(ts, timestamp, 63);
        ts[63] = '\0';
    } else {
        coerce_message_timestamp(content, ts, sizeof(ts));
    }
    
    snprintf(output, out_sz, "[%s] %s", ts, content);
}


/* Port of Python: strip_leading_message_timestamps */
void strip_leading_message_timestamps(const char *text, char *output, size_t out_sz) {
    if (!text || !output || out_sz == 0) return;
    
    const char *p = text;
    /* Skip leading [timestamp] patterns */
    while (*p) {
        timestamp_match_t match = parse_timestamp_match(p);
        if (match.found && match.start == 0) {
            p = text + match.end + 1;
            while (*p == ' ') p++;
        } else {
            break;
        }
    }
    
    strncpy(output, p, out_sz - 1);
    output[out_sz - 1] = '\0';
}



/* ── from src/gateway/port_message_timestamps_ports.c ── */
/*
 * port_message_timestamps_remaining.c — Port of gateway/message_timestamps.py
 * timestamp surface. Coercion, formatting, prefix stripping, prefix
 * parsing.
 */
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

/* PoP: mts_coerce_message_timestamp @ gateway/message_timestamps.py:coerce_message_timestamp */
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

/* PoP: mts_format_message_timestamp @ gateway/message_timestamps.py:format_message_timestamp */
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

/* PoP: mts_strip_leading_message_timestamps @ gateway/message_timestamps.py:strip_leading_message_timestamps */
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

/* PoP: mts_render_user_content_with_timestamp @ gateway/message_timestamps.py:render_user_content_with_timestamp */
char *mts_render_user_content_with_timestamp(const char *content, const char *timestamp) {
    /* Python: exactly one prefix. */
    if (!content) return strdup("");
    char *fmt = mts_format_message_timestamp(timestamp);
    char *out = NULL;
    asprintf(&out, "%s %s", fmt && *fmt ? fmt : "", content);
    free(fmt);
    return out;
}

/* PoP: mts_parse_timestamp_prefix @ gateway/message_timestamps.py:_parse_timestamp_prefix */
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

/* PoP: mts_parse_timestamp_match @ gateway/message_timestamps.py:_parse_timestamp_match */
long mts_parse_timestamp_match(const char *match_text) {
    /* Python: match → epoch. */
    if (!match_text) return -1;
    return mts_coerce_message_timestamp(match_text);
}

