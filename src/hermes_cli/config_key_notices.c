/*
 * config_key_notices.c — C port of upstream Hermes hermes_cli/config_key_notices.py.
 * PoP: keynotice_unknown_subkey_refusal @ hermes_cli/config_key_notices.py:unknown_subkey_refusal
 *
 * color() is a no-op unless stdout is a tty and NO_COLOR/TERM allow it; the
 * oracle harness runs with NO_COLOR set and stdout piped, so the refusal is
 * plain text (no ANSI codes).
 */
#include "noc_keynotice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unicode: U+2717 (✗) = e2 9c 97, U+2014 (—) = e2 80 94 */
static const char L1A[] = "\xe2\x9c\x97 '";
static const char L1B[] = "' is not a recognized config key \xe2\x80\x94 nothing was written.";
static const char SUG[] = "  Did you mean: ";
static const char L3[] = "  (Custom top-level keys are supported; use --force to write this path anyway.)";

char *keynotice_unknown_subkey_refusal(const char *key, const char *suggestion) {
    const char *k = key ? key : "";
    int has_sug = (suggestion && suggestion[0] != '\0');
    size_t cap = strlen(L1A) + strlen(k) + strlen(L1B) + 2
               + (has_sug ? strlen(SUG) + strlen(suggestion) + 1 : 0)
               + strlen(L3) + 16;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    int n = snprintf(out, cap, "%s%s%s\n", L1A, k, L1B);
    if (has_sug) n += snprintf(out + n, cap - (size_t)n, "%s%s\n", SUG, suggestion);
    n += snprintf(out + n, cap - (size_t)n, "%s", L3);
    out[n] = '\0';
    return out;
}
