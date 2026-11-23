/*
 * activity.c — faithful C11 port of session_rules/activity.py.
 * Gate: sract.
 * PoP: sract_bound_activity_description @ session_rules/activity.py:bound_activity_description
 * PoP: sract_normalize_activity_provenance @ session_rules/activity.py:normalize_activity_provenance
 * PoP: sract_activity_description_max @ session_rules/activity.py:ACTIVITY_DESCRIPTION_MAX
 *
 * bound_activity_description: text = (description or "").strip(); truncate to
 * 119 characters + U+2026 when longer than 120 (Python len counts code points).
 * normalize_activity_provenance: strip + enum lookup, UNKNOWN on miss.
 */
#include "noc_sract.h"
#include <stdlib.h>
#include <string.h>

static const char *const KNOWN[] = {
    "unknown",
    "agent.compression",
    "agent.compression_timeout",
    "agent.compression_cooldown",
    "agent.compression_turnhold",
};
#define NKNOWN ((int)(sizeof(KNOWN) / sizeof(KNOWN[0])))

long long sract_activity_description_max(void) { return 120; }

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* strip ASCII whitespace (bytes that can never occur inside a UTF-8 sequence) */
static char *strip_dup(const char *s) {
    size_t n;
    const char *a, *b;
    if (!s) s = "";
    n = strlen(s);
    a = s;
    b = s + n;
    while (a < b && is_ws((unsigned char)a[0])) a++;
    while (b > a && is_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

char *sract_bound_activity_description(const char *description) {
    char *text = strip_dup(description);   /* (description or "").strip() */
    if (!text) return NULL;
    /* count UTF-8 code points and remember the start of code point #119 */
    size_t cp = 0;
    const char *cut = NULL;
    for (const unsigned char *p = (const unsigned char *)text; *p;) {
        unsigned char c = *p;
        size_t step = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (cp == 119) cut = (const char *)p;
        p += step;
        cp++;
    }
    if (cp <= 120) return text;
    /* longer than budget: first 119 code points + ellipsis */
    size_t prefix = (size_t)(cut - text);
    char *out = (char *)malloc(prefix + 3 + 1);
    if (!out) { free(text); return NULL; }
    memcpy(out, text, prefix);
    out[prefix]     = (char)0xE2; /* U+2026 "…" */
    out[prefix + 1] = (char)0x80;
    out[prefix + 2] = (char)0xA6;
    out[prefix + 3] = '\0';
    free(text);
    return out;
}

const char *sract_normalize_activity_provenance(const char *provenance) {
    char *s = strip_dup(provenance);   /* (provenance or "").strip() */
    if (!s) return "unknown";
    for (int i = 0; i < NKNOWN; i++) {
        if (!strcmp(s, KNOWN[i])) { free(s); return KNOWN[i]; }
    }
    free(s);
    return "unknown";                 /* ValueError -> UNKNOWN */
}
