/*
 * port_agent_reasoning_timeouts.c — C port of agent/reasoning_timeouts.py
 *
 * Pure per-reasoning-model stale-timeout floor resolver. No config load,
 * no network, no async — just slug-table regex matching. Faithful port of
 * get_reasoning_stale_timeout_floor / _match_any / _get_pattern.
 *
 * The Python module anchors each slug at the start of the model name
 * (after stripping an aggregator prefix like "openai/") and right-anchors
 * at end-of-string or a "-", ".", "_" separator. We reproduce that exactly
 * with a length-descending slug scan so the longest matching slug wins
 * (e.g. "o3-mini" beats "o3").
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* (slug, floor_seconds). Mirrors _REASONING_STALE_TIMEOUT_FLOORS. */
typedef struct {
    const char *slug;
    double floor;
} reasoning_floor_t;

static const reasoning_floor_t REASONING_FLOORS[] = {
    {"nemotron-3-ultra",         600.0},
    {"nemotron-3-super",         600.0},
    {"nemotron-3-nano",          300.0},
    {"deepseek-r1",              600.0},
    {"deepseek-reasoner",        600.0},
    {"qwq-32b",                  300.0},
    {"qwen3",                    180.0},
    {"o1",                       600.0},
    {"o1-mini",                  600.0},
    {"o1-pro",                   600.0},
    {"o1-preview",               600.0},
    {"o3",                       600.0},
    {"o3-pro",                   600.0},
    {"o3-mini",                  300.0},
    {"o4-mini",                  300.0},
    {"claude-opus-4",            240.0},
    {"claude-sonnet-4.5",        180.0},
    {"claude-sonnet-4.6",        180.0},
    {"grok-4-fast-reasoning",    300.0},
    {"grok-4.20-reasoning",      300.0},
    {"grok-4-fast-non-reasoning",180.0},
};
static const size_t N_FLOORS = sizeof(REASONING_FLOORS) / sizeof(REASONING_FLOORS[0]);

/* Lazily-sorted (length-descending) copy so the longest slug wins. */
static reasoning_floor_t SORTED_FLOORS[sizeof(REASONING_FLOORS) / sizeof(REASONING_FLOORS[0])];
static int g_sorted = 0;

static int cmp_floor_len_desc(const void *a, const void *b) {
    const reasoning_floor_t *fa = (const reasoning_floor_t *)a;
    const reasoning_floor_t *fb = (const reasoning_floor_t *)b;
    int la = (int)strlen(fa->slug), lb = (int)strlen(fb->slug);
    return (lb - la); /* descending */
}

static void ensure_sorted(void) {
    if (g_sorted) return;
    memcpy(SORTED_FLOORS, REASONING_FLOORS, sizeof(REASONING_FLOORS));
    qsort(SORTED_FLOORS, N_FLOORS, sizeof(reasoning_floor_t), cmp_floor_len_desc);
    g_sorted = 1;
}

/* PoP: reasoning_timeouts_slug_matches @ agent/reasoning_timeouts.py:_get_pattern */
/* Returns 1 if `slug` matches at the start of `model_lower` with a
 * [-._] / end-of-string right anchor (faithful to the Python regex
 * `^slug(?:$|[-._])`; the Python _get_pattern compiles this per slug). */
static int reasoning_timeouts_slug_matches(const char *slug, const char *model_lower) {
    if (!slug || !model_lower) return 0;
    size_t slen = strlen(slug);
    if (slen == 0) return 0;
    if (strncmp(model_lower, slug, slen) != 0) return 0;
    char c = model_lower[slen];
    return (c == '\0' || c == '-' || c == '.' || c == '_');
}

/* PoP: reasoning_timeouts_match_any @ agent/reasoning_timeouts.py:_match_any */
/* Return the floor (seconds) for the first matching slug, or -1.0 if none. */
double reasoning_timeouts_match_any(const char *model_lower) {
    if (!model_lower || !*model_lower) return -1.0;
    ensure_sorted();
    for (size_t i = 0; i < N_FLOORS; i++) {
        if (reasoning_timeouts_slug_matches(SORTED_FLOORS[i].slug, model_lower)) {
            return SORTED_FLOORS[i].floor;
        }
    }
    return -1.0;
}

/* PoP: reasoning_timeouts_get_floor @ agent/reasoning_timeouts.py:get_reasoning_stale_timeout_floor */
/* Resolve the stale-timeout floor for a (possibly prefixed) model slug.
 * Returns the floor in seconds, or -1.0 when the model is not in the
 * allowlist / empty / not a string. Faithful to the Python Optional[float]
 * return (None -> -1.0 sentinel; all real floors are >= 180). */
double reasoning_timeouts_get_floor(const char *model) {
    if (!model || !*model) return -1.0;

    /* Copy + lowercase + trim leading whitespace. */
    char buf[1024];
    size_t n = strlen(model);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    size_t j = 0;
    /* skip leading whitespace */
    const char *p = model;
    while (*p && isspace((unsigned char)*p)) p++;
    for (; *p && j < sizeof(buf) - 1; p++) {
        buf[j++] = (char)tolower((unsigned char)*p);
    }
    buf[j] = '\0';

    if (buf[0] == '\0') return -1.0;

    /* Strip aggregator prefix (everything before and including the last '/'). */
    char *slash = strrchr(buf, '/');
    const char *name = slash ? slash + 1 : buf;

    return reasoning_timeouts_match_any(name);
}
