/*
 * redact_match.c - redaction pattern / value matchers.
 *
 * Split from redact.c (soul.md: self-contained modules). JWT / key-value /
 * free-text-key detection. No file-scope state.
 */
#include "redact_match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* ================================================================
 *  JWT Detection
 * ================================================================ */

/* Check if a token looks like a JWT (three base64 segments separated by dots) */ bool looks_like_jwt(const char *start, size_t len) {
    if (len < 30) return false;

    int dots = 0;
    for (size_t i = 0; i < len && i < 200; i++) {
        if (start[i] == '.') dots++;
        if (dots > 2) break;
        /* Only base64url characters expected */
        if (!isalnum((unsigned char)start[i]) && start[i] != '-' &&
            start[i] != '_' && start[i] != '.')
            return false;
    }
    return dots >= 2;
}


/* ================================================================
 *  Pattern matching helpers
 * ================================================================ */

/* Check if prefix appears as a key:value pattern in text */ const char *find_key_value(const char *text, const char *key,
                                   size_t key_len, size_t *val_len) {
    if (!text || !key || key_len == 0) return NULL;

    const char *p = text;
    while (*p) {
        /* Try to find key (case-insensitive) */
        const char *found = strstr(p, key);
        if (!found) return NULL;

        /* Check it's a key context (followed by =, :, ", or whitespace) */
        const char *after = found + key_len;
        if (*after == '=' || *after == ':' || *after == '"' || *after == '\'') {
            /* Skip separator */
            if (*after == '=' || *after == ':') after++;
            while (*after == ' ' || *after == '\t' || *after == '"' || *after == '\'')
                after++;

            /* Find end of value */
            const char *val_start = after;
            const char *val_end = after;
            while (*val_end && *val_end != ',' && *val_end != '\n' &&
                   *val_end != '\r' && *val_end != '}' && *val_end != ']' &&
                   *val_end != ' ' && *val_end != '\t')
                val_end++;

            *val_len = (size_t)(val_end - val_start);
            if (*val_len > 0) return val_start;
        }
        p = found + 1;
    }
    return NULL;
}


/* ================================================================
 *  Redaction engine
 * ================================================================ */

/* Redact a single value in-place within the string.
 * Returns the number of characters replaced. */ size_t redact_value(char *text, const char *val_start, size_t val_len,
                           size_t max_show) {
    if (!text || !val_start || val_len == 0) return 0;

    size_t offset = (size_t)(val_start - text);
    size_t show = max_show < val_len ? max_show : val_len / 4;
    if (show > 32) show = 32;

    char redacted[512];
    int n;
    if (show > 0) {
        n = snprintf(redacted, sizeof(redacted), "%.*s***REDACTED***",
                     (int)show, val_start);
    } else {
        n = snprintf(redacted, sizeof(redacted), "***REDACTED***");
    }

    size_t redacted_len = (size_t)n;
    size_t remaining = strlen(text + offset + val_len) + 1;

    /* Check buffer bounds */
    if (offset + redacted_len + remaining > 65536)
        return 0;

    /* Shift and replace */
    memmove(text + offset + redacted_len, text + offset + val_len, remaining);
    memcpy(text + offset, redacted, redacted_len);

    return redacted_len > val_len ? redacted_len - val_len : 0;
}


/* ================================================================
 *  Free-text prefix matching (S14 U10 parity)
 * ================================================================ */

/* Check if a prefix that looks like an API key token appears in free text
 * (not as a key:value pair, but as a bare token in the text).
 * Returns the start of the value, or NULL if not found. */ const char *find_free_text_key(const char *text, const char *prefix,
                                       size_t prefix_len, size_t *val_len,
                                       size_t min_len) {
    if (!text || !prefix || prefix_len == 0) return NULL;

    const char *p = text;
    while (*p) {
        const char *found = strstr(p, prefix);
        if (!found) return NULL;

        const char *after = found + prefix_len;

        /* Only match if followed by alphanumeric (not key:value context) */
        if (isalnum((unsigned char)*after) || *after == '_' || *after == '-') {
            /* Find end of value */
            const char *val_start = after;
            const char *val_end = after;
            while (*val_end && (isalnum((unsigned char)*val_end) ||
                   *val_end == '_' || *val_end == '-' || *val_end == '.'))
                val_end++;

            *val_len = (size_t)(val_end - val_start);
            if (*val_len >= min_len) return val_start;
        }
        p = found + 1;
    }
    return NULL;
}
