/*
 * gateway_run_match.c — provider-error pattern matchers + secret redaction.
 *
 * Split from port_gateway_run.c (soul.md: self-contained modules). Pure
 * string matching/redaction; no file-scope state.
 */
#include "gateway_run_match.h"
#include <string.h>
#include <ctype.h>

bool match_provider_error_re(const char *text)
{
    /* r"(api\s+(?:call\s+)?failed|provider\s+authentication\s+failed|
     *    non-retryable\s+error|rate\s+limited\s+after\s+\d+\s+retries|
     *    error\s+code\s*:|\bhttp\s*\d{3}\b|incorrect\s+api\s+key|
     *    invalid\s+api\s+key)" re.IGNORECASE */
    const char *patterns[] = {
        "api failed", "api call failed",
        "provider authentication failed",
        "non-retryable error",
        "rate limited after ",
        "error code",
        "error code:",
        "incorrect api key",
        "invalid api key",
        NULL
    };
    if (!text) return false;
    /* Case-insensitive substring matching */
    char lower[512];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    for (int i = 0; patterns[i]; i++) {
        if (strstr(lower, patterns[i]))
            return true;
    }
    /* Also check for http\s*ddd pattern */
    const char *h = strstr(lower, "http");
    if (h) {
        h += 4;
        while (*h == ' ') h++;
        if (*h >= '0' && *h <= '9') return true;
    }
    return false;
}

bool match_auth_error_re(const char *text)
{
    if (!text) return false;
    char lower[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    if (strstr(lower, "provider authentication failed")) return true;
    if (strstr(lower, "incorrect api key")) return true;
    if (strstr(lower, "invalid api key")) return true;
    if (strstr(lower, " 401")) return true;
    if (strstr(lower, "401 ")) return true;
    return false;
}

bool match_policy_re(const char *text)
{
    if (!text) return false;
    char lower[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    const char *patterns[] = {
        "cybersecurity risk", "security policy", "safety policy",
        "policy violation", "violate", "violates", "violated", "violation",
        "blocked because", "blocked by", "blocked under",
        "request was blocked", "request was rejected", "request blocked",
        "request rejected", "disallowed", "moderation",
        NULL
    };
    for (int i = 0; patterns[i]; i++) {
        if (strstr(lower, patterns[i]))
            return true;
    }
    return false;
}

bool match_rate_limit_re(const char *text)
{
    if (!text) return false;
    char lower[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    if (strstr(lower, "rate limit")) return true;
    if (strstr(lower, "rate-limited")) return true;
    if (strstr(lower, " 429")) return true;
    if (strstr(lower, "quota")) return true;
    if (strstr(lower, "usage limit")) return true;
    return false;
}

void apply_secret_patterns(const char *text, char *out, size_t out_size)
{
    /* Simple approach: scan for known secret patterns and replace.
     * C doesn't have regex built-in; use simple substring matching
     * for the known prefixes. This is the "belt" pass; the main
     * redaction happens via redact_sensitive_text() which uses full regex. */
    if (!out || out_size == 0) return;
    const char *p = text ? text : "";
    char buf[8192];
    size_t pos = 0;

    while (*p && pos < sizeof(buf) - 1) {
        /* Check known key prefixes */
        int sk_len = 0;

        /* sk-[A-Za-z0-9]... OpenAI style */
        if (p[0] == 's' && p[1] == 'k' && p[2] == '-') {
            sk_len = 3;
            while (sk_len < 50 && p[sk_len] &&
                   (isalnum((unsigned char)p[sk_len]) || p[sk_len] == '_' || p[sk_len] == '-'))
                sk_len++;
            if (sk_len >= 15) {
                for (int i = 0; i < sk_len && pos < sizeof(buf) - 1; i++) {
                    buf[pos++] = (i < 3) ? p[i] : '*';
                }
                p += sk_len;
                continue;
            }
        }

        buf[pos++] = *p;
        p++;
    }
    buf[pos] = '\0';

    size_t n = strlen(buf);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, buf, n);
    out[n] = '\0';
}
