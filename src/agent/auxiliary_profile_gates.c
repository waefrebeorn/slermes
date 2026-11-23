/*
 * auxiliary_profile_gates.c — C port of upstream Hermes agent/auxiliary_profile_gates.py.
 * PoP: auxprof_contains_profile_reasoning_fields @ agent/auxiliary_profile_gates.py:contains_profile_reasoning_fields
 * PoP: auxprof_profile_forwards_max_tokens @ agent/auxiliary_profile_gates.py:profile_forwards_max_tokens
 *
 * contains_profile_reasoning_fields is a faithful recursive port.
 * profile_forwards_max_tokens cannot perform live plugin discovery from C; its
 * pure decision core is ported and the limitation is documented in the header.
 */
#include "noc_auxprof.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const REASONING_KEYS[] = {
    "reasoning", "reasoning_effort", "thinking", "thinking_config", "thinkingconfig",
    "thinking_budget", "thinkingbudget", "enable_thinking", "think", "verbosity"
};

static char *normalize_key(const char *key) {
    size_t n = key ? strlen(key) : 0, a = 0, b = n;
    while (a < b && isspace((unsigned char)key[a])) a++;
    while (b > a && isspace((unsigned char)key[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    for (size_t i = a; i < b; i++) out[i - a] = (char)tolower((unsigned char)key[i]);
    out[b - a] = 0;
    return out;
}
static int key_is_reasoning(const char *key) {
    char *k = normalize_key(key);
    int found = 0;
    for (size_t i = 0; i < sizeof(REASONING_KEYS) / sizeof(REASONING_KEYS[0]); i++) {
        if (!strcmp(k, REASONING_KEYS[i])) { found = 1; break; }
    }
    free(k);
    return found;
}

int auxprof_contains_profile_reasoning_fields(const json_t *value) {
    if (!value || value->type != JSON_OBJECT) return 0; /* not a dict -> False */
    for (size_t i = 0; i < json_object_size(value); i++) {
        const char *key = json_object_get_key_at(value, i);
        if (key_is_reasoning(key ? key : "")) return 1;
        if (auxprof_contains_profile_reasoning_fields(json_object_get_at(value, i))) return 1;
    }
    return 0;
}

int auxprof_profile_forwards_max_tokens(int profile_known, int forwards_max_tokens) {
    /* live: profile is not None and bool(profile.forwards_max_tokens) */
    return profile_known && forwards_max_tokens;
}
