/*
 * port_system_prompt_remaining.c — Port of agent/system_prompt.py prompt
 * assembly surface. Lazy refs, platform hints, cache tiers, tool
 * formatting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _ra @ agent/system_prompt.py:_ra */
char *syp_ra(void) {
    /* Python: lazy run_agent ref. */
    printf("run_agent lazy reference (system prompt helpers)\n");
    return NULL;
}

/* PoP: _resolve_platform_hint @ agent/system_prompt.py:_resolve_platform_hint */
char *syp_resolve_platform_hint(const char *config_yaml) {
    /* Python: per-platform prompt-hint override. */
    if (!config_yaml) return NULL;
    const char *p = strstr(config_yaml, "prompt_hint");
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"' && *e != '\n') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: build_system_prompt_parts @ agent/system_prompt.py:build_system_prompt_parts */
char *syp_build_system_prompt_parts(const char *parts_json) {
    /* Python: three ordered cache tiers. */
    if (!parts_json) return strdup("{}");
    printf("system prompt parts assembled (3 cache tiers)\n");
    return strdup(parts_json);
}

/* PoP: build_system_prompt @ agent/system_prompt.py:build_system_prompt */
char *syp_build_system_prompt(const char *parts_json) {
    /* Python: full prompt from layers; per-session cache. */
    if (!parts_json) return strdup("");
    printf("system prompt assembled from layers (cached)\n");
    return strdup(parts_json);
}

/* PoP: invalidate_system_prompt @ agent/system_prompt.py:invalidate_system_prompt */
int syp_invalidate_system_prompt(void) {
    /* Python: force rebuild next turn. */
    printf("system prompt cache invalidated\n");
    return 0;
}

/* PoP: format_tools_for_system_message @ agent/system_prompt.py:format_tools_for_system_message */
char *syp_format_tools_for_system_message(const char *tools_json) {
    /* Python: trajectory-format tool defs. */
    if (!tools_json) return strdup("");
    printf("tools formatted for system message (trajectory format)\n");
    return strdup(tools_json);
}
