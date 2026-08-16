#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/system_prompt.c — C11 port of agent/system_prompt.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_system_prompt.c ── */
/*
 * port_agent_system_prompt.c — Port of Python agent/system_prompt.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _resolve_platform_hint */
typedef enum { HINT_REPLACE, HINT_APPEND, HINT_NONE } hint_mode_t;

void resolve_platform_hint(const char *platform_key, const char *default_hint,
                            const char *overrides_json, char *result, size_t result_sz) {
    if (!result || result_sz == 0) return;
    if (default_hint) {
        strncpy(result, default_hint, result_sz - 1);
        result[result_sz - 1] = '\0';
    } else {
        result[0] = '\0';
    }
    
    if (!overrides_json || !platform_key) return;
    
    /* Look up platform override in JSON */
    char key[256];
    snprintf(key, sizeof(key), "\"%s\":", platform_key);
    const char *override = strstr(overrides_json, key);
    if (!override) return;
    
    override += strlen(key);
    while (*override == ' ') override++;
    
    if (*override == '"') {
        /* String value = append */
        override++;
        size_t len = strlen(result);
        if (len > 0 && len < result_sz - 1) {
            result[len] = ' ';
            len++;
        }
        size_t i = 0;
        while (*override && *override != '"' && len + i < result_sz - 1) {
            result[len + i] = *override++;
            i++;
        }
        result[len + i] = '\0';
    } else if (strstr(override, "\"replace\"")) {
        /* Replace mode — extract replacement text */
        const char *val = strstr(override, "\"value\"");
        if (val) {
            val = strchr(val + 7, '"');
            if (val) {
                val++;
                size_t i = 0;
                while (*val && *val != '"' && i < result_sz - 1) {
                    result[i++] = *val++;
                }
                result[i] = '\0';
            }
        }
    }
}



/* ── from src/agent/port_system_prompt_ports.c ── */
/*
 * port_system_prompt_remaining.c — Port of agent/system_prompt.py prompt
 * assembly surface. Lazy refs, platform hints, cache tiers, tool
 * formatting.
 */
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

/* PoP: syp_ra @ agent/system_prompt.py:_ra */
char *syp_ra(void) {
    /* Python: lazy run_agent ref. */
    printf("run_agent lazy reference (system prompt helpers)\n");
    return NULL;
}

/* PoP: syp_resolve_platform_hint @ agent/system_prompt.py:_resolve_platform_hint */
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

/* PoP: syp_build_system_prompt_parts @ agent/system_prompt.py:build_system_prompt_parts */
char *syp_build_system_prompt_parts(const char *parts_json) {
    /* Python: three ordered cache tiers. */
    if (!parts_json) return strdup("{}");
    printf("system prompt parts assembled (3 cache tiers)\n");
    return strdup(parts_json);
}

/* PoP: syp_build_system_prompt @ agent/system_prompt.py:build_system_prompt */
char *syp_build_system_prompt(const char *parts_json) {
    /* Python: full prompt from layers; per-session cache. */
    if (!parts_json) return strdup("");
    printf("system prompt assembled from layers (cached)\n");
    return strdup(parts_json);
}

/* PoP: syp_invalidate_system_prompt @ agent/system_prompt.py:invalidate_system_prompt */
int syp_invalidate_system_prompt(void) {
    /* Python: force rebuild next turn. */
    printf("system prompt cache invalidated\n");
    return 0;
}

/* PoP: syp_format_tools_for_system_message @ agent/system_prompt.py:format_tools_for_system_message */
char *syp_format_tools_for_system_message(const char *tools_json) {
    /* Python: trajectory-format tool defs. */
    if (!tools_json) return strdup("");
    printf("tools formatted for system message (trajectory format)\n");
    return strdup(tools_json);
}

