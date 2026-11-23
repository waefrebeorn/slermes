/*
 * turn_recovery.c — C home for upstream Hermes agent/turn_recovery.py.
 * Ported behavior: the pure provider image-dimension parser. All recovery
 * handlers mutate agent state / messages and stay in Python.
 * PoP: w6_turnrecover_image_error_max_dimension @ agent/turn_recovery.py:_image_error_max_dimension
 */
#include "noc_w6_turnrecover_012061.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

static int ascii_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* math.isqrt for the small (<= 7 digit) limits this parser accepts. */
static long long small_isqrt(long long n) {
    long long r = (long long)sqrt((double)n);
    while ((r + 1) * (r + 1) <= n) r++;
    while (r * r > n) r--;
    return r;
}

static int in_range(long long d) {
    return (d >= 512 && d <= 8000) ? (int)d : -1; /* -1 == Python None */
}

int w6_turnrecover_image_error_max_dimension(
    const char *error, const char *message, const char *body)
{
    char text[32768];
    size_t n = 0;
    const char *values[3];
    size_t i;
    const char *p;

    values[0] = error;
    values[1] = message;
    values[2] = body;
    for (i = 0; i < 3; i++) {
        const char *v = values[i];
        size_t len;
        if (!v || !*v) continue; /* Python: `if value:` */
        len = strlen(v);
        if (n + len + 1 >= sizeof(text)) len = sizeof(text) - n - 1;
        if (n) text[n++] = ' '; /* " ".join(parts) */
        for (; len; len--, v++) text[n++] = (char)tolower((unsigned char)*v);
    }
    text[n] = '\0';

    /* OpenAI Codex Responses reports a tile-patch budget, not a pixel ceiling:
     * per-side cap = isqrt(limit) * 32. */
    if (strstr(text, "patches after processing") != NULL) {
        const char *q = strstr(text, "exceeding the limit of");
        int ndig = 0;
        long long num = 0;
        if (!q) return -1;
        q += strlen("exceeding the limit of");
        while (ascii_isspace(*q)) q++; /* \s* */
        while (*q >= '0' && *q <= '9') {
            if (ndig < 7) num = num * 10 + (*q - '0'); /* greedy \d{2,7} */
            ndig++;
            q++;
        }
        if (ndig < 2) return -1;
        return in_range(small_isqrt(num) * 32);
    }

    if (!strstr(text, "image") || !strstr(text, "dimension") ||
        !strstr(text, "max allowed size")) return -1;

    p = strstr(text, "max allowed size");
    if (!p) return -1;
    p += strlen("max allowed size");
    if (*p != ':') {
        /* Optional (?:\s+for [^:]+)? before the colon. */
        const char *r = p;
        while (ascii_isspace(*r)) r++;
        if (r == p || strncmp(r, "for", 3) != 0) return -1;
        r += 3;
        if (*r != ' ') return -1;          /* the literal space in `for ` */
        r++;
        if (*r == ':' || *r == '\0') return -1; /* [^:]+ needs >=1 char */
        while (*r && *r != ':') r++;
        if (*r != ':') return -1;
        p = r;
    }
    p++; /* consume ':' */
    while (ascii_isspace(*p)) p++; /* \s* */
    {
        int ndig = 0;
        long long num = 0;
        while (*p >= '0' && *p <= '9') {
            if (ndig < 5) num = num * 10 + (*p - '0'); /* greedy \d{3,5} */
            ndig++;
            p++;
        }
        if (ndig < 3) return -1;
        while (ascii_isspace(*p)) p++;
        if (strncmp(p, "pixel", 5) != 0) return -1; /* pixels? */
        return in_range(num);
    }
}
