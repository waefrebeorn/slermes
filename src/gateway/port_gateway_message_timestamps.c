/*
 * port_gateway_message_timestamps.c — Port of Python gateway/message_timestamps.py
 */
#include <stdio.h>
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

