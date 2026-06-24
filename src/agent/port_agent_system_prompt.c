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

