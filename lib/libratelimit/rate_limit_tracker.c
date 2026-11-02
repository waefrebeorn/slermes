
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* PoP: lower_headers @ agent/rate_limit_tracker.py:lower_headers */
char *rate_limit_lower_headers(const char *key) {
    if (!key) return NULL;
    size_t n = strlen(key);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)key[i]);
    out[n] = '\0';
    return out;
}

/* PoP: has_rate_limit_headers @ agent/rate_limit_tracker.py:has_rate_limit_headers */
int rate_limit_has_rate_limit_headers(const char *header_name) {
    if (!header_name) return 0;
    /* case-insensitive prefix check */
    const char *p = header_name;
    /* skip leading spaces */
    while (*p == ' ') p++;
    /* check prefix x-ratelimit- (case-insensitive) */
    const char *prefix = "x-ratelimit-";
    for (int i = 0; prefix[i]; i++) {
        if (tolower((unsigned char)p[i]) != prefix[i]) return 0;
    }
    return 1;
}
