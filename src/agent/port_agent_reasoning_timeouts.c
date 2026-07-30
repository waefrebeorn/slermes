/* Slermes C port — agent/reasoning_timeouts.py (pure reasoning-model floor lookup) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <regex.h>

/* Faithful copy of _REASONING_STALE_TIMEOUT_FLOORS (slug, floor_seconds). */
typedef struct { const char *slug; int floor; } rt_floor_t;
static const rt_floor_t RT_FLOORS[] = {
    {"nemotron-3-ultra", 600},
    {"nemotron-3-super", 600},
    {"nemotron-3-nano", 300},
    {"deepseek-r1", 600},
    {"deepseek-reasoner", 600},
    {"qwq-32b", 300},
    {"qwen3", 180},
    {"o1", 600},
    {"o1-mini", 600},
    {"o1-pro", 600},
    {"o1-preview", 600},
    {"o3", 600},
    {"o3-pro", 600},
    {"o3-mini", 300},
    {"o4-mini", 300},
    {"claude-opus-4", 240},
    {"claude-sonnet-4.5", 180},
    {"claude-sonnet-4.6", 180},
    {"grok-4-fast-reasoning", 300},
    {"grok-4.20-reasoning", 300},
    {"grok-4-fast-non-reasoning", 180},
};
#define RT_N (int)(sizeof(RT_FLOORS)/sizeof(RT_FLOORS[0]))

/* PoP: _get_pattern @ agent/reasoning_timeouts.py:_get_pattern */
/* Build the wrapper regex `^slug($|[-._])` for a slug — mirrors Python's
 * _get_pattern (start-of-slug + slug + end-or-separator right anchor).
 * Returns 0 on success, non-zero on overflow. */
static int rt_get_pattern(const char *slug, char *out, size_t outsz)
{
    int n = snprintf(out, outsz, "^%s($|[-._])", slug);
    return (n < 0 || (size_t)n >= outsz) ? -1 : 0;
}

/* PoP: _match_any @ agent/reasoning_timeouts.py:_match_any */
/* Return the floor (seconds) for the first matching slug, else -1.0.
 * Longest slug wins on shared prefixes (o3-mini beats o3). */
static double rt_match_any(const char *slug)
{
    int idx[RT_N];
    for (int i = 0; i < RT_N; i++) idx[i] = i;
    for (int i = 0; i < RT_N; i++)
        for (int j = i + 1; j < RT_N; j++)
            if (strlen(RT_FLOORS[idx[j]].slug) > strlen(RT_FLOORS[idx[i]].slug)) {
                int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
            }
    for (int k = 0; k < RT_N; k++) {
        const char *sl = RT_FLOORS[idx[k]].slug;
        char pat[256];
        if (rt_get_pattern(sl, pat, sizeof(pat)) != 0) continue;
        regex_t re;
        if (regcomp(&re, pat, REG_EXTENDED | REG_NOSUB) != 0) continue;
        int rc = regexec(&re, slug, 0, NULL, 0);
        regfree(&re);
        if (rc == 0) return (double)RT_FLOORS[idx[k]].floor;
    }
    return -1.0;
}

/* PoP: agent_reasoning_timeouts_get_floor @ agent/reasoning_timeouts.py:get_reasoning_stale_timeout_floor */
double agent_reasoning_timeouts_get_floor(const char *model)
{
    if (!model || !*model) return -1.0;
    char name[1024];
    size_t n = 0;
    for (const char *p = model; *p && n + 1 < sizeof(name); p++) name[n++] = (char)tolower((unsigned char)*p);
    name[n] = '\0';
    /* strip leading/trailing whitespace */
    size_t s = 0, e = n;
    while (s < e && (name[s] == ' ' || name[s] == '\t')) s++;
    while (e > s && (name[e-1] == ' ' || name[e-1] == '\t' || name[e-1] == '\n' || name[e-1] == '\r')) e--;
    name[e] = '\0';
    if (name[s] == '\0') return -1.0;
    const char *slug = name + s;
    /* strip aggregator prefix (everything before and including last '/') */
    const char *slash = strrchr(slug, '/');
    if (slash) slug = slash + 1;
    if (!*slug) return -1.0;

    double floor = rt_match_any(slug);
    return floor;
}
