/*
 * display_config.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway_display_config.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Display config — resolve display settings
 *  Port of Python gateway/display_config.py.
 * ================================================================ */

/* Port of Python gateway/display_config.py _normalise(setting, value).
 *
 * Faithful to LIVE Python. IMPORTANT: the C caller (resolve_display_setting)
 * only invokes this for JSON_STRING values; bool/number JSON types are
 * handled by the caller directly. So this function receives *string* values
 * and must reproduce Python's behaviour for string inputs:
 *   - tool_progress: str(value).lower()   (a string is never `is False`/`is
 *     True`, so the "off"/"all" branches never trigger for string input)
 *   - show_reasoning/streaming/interim_assistant_messages/
 *     long_running_notifications/busy_ack_detail/cleanup_progress:
 *     value.lower() in {true,1,yes,on} -> bool True/False (rendered "true"/
 *     "false" for the JSON layer)
 *   - tool_progress_grouping: accumulate|separate (else accumulate)
 *   - reasoning_style: code|blockquote|subtext (else code)
 *   - tool_preview_length: int(value) on success, 0 on failure
 *   - default: passthrough
 * Verified byte-equal to LIVE Python (string-input contract) via
 * tests/sta_oracle_display_config.py. */
/* PoP: gateway_display_config_normalise @ gateway/display_config.py:_normalise */
char *normalise_display_value(const char *setting, const char *value) {
    if (!value) value = "";
    if (!setting) setting = "";

    /* tool_progress: str(value).lower() (string input never hits bool identity) */
    if (strcmp(setting, "tool_progress") == 0) {
        char *buf = strdup(value);
        if (!buf) return NULL;
        for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
        return buf;
    }

    /* boolean-ish settings: value.lower() in {true,1,yes,on} -> True/False.
       Rendered as "true"/"false" for the JSON string layer. */
    static const char *bool_settings[] = {
        "show_reasoning", "streaming", "interim_assistant_messages",
        "long_running_notifications", "busy_ack_detail", "cleanup_progress", NULL
    };
    for (int i = 0; bool_settings[i]; i++) {
        if (strcmp(setting, bool_settings[i]) == 0) {
            int truthy = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                          strcasecmp(value, "yes")  == 0 || strcasecmp(value, "on") == 0);
            int falsy  = (strcasecmp(value, "false") == 0 || strcmp(value, "0") == 0 ||
                          strcasecmp(value, "no")    == 0 || strcasecmp(value, "off") == 0);
            if (truthy) return strdup("true");
            if (falsy)  return strdup("false");
            /* unrecognised string -> Python bool(value) -> True for non-empty */
            return strdup(*value ? "true" : "false");
        }
    }

    /* tool_progress_grouping: accumulate | separate (else accumulate) */
    if (strcmp(setting, "tool_progress_grouping") == 0) {
        char *buf = strdup(value);
        if (!buf) return NULL;
        for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
        if (strcmp(buf, "separate") == 0) return buf;
        free(buf);
        return strdup("accumulate");
    }

    /* reasoning_style: code | blockquote | subtext (else code) */
    if (strcmp(setting, "reasoning_style") == 0) {
        char *buf = strdup(value);
        if (!buf) return NULL;
        for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
        if (strcmp(buf, "blockquote") == 0 || strcmp(buf, "subtext") == 0) return buf;
        free(buf);
        return strdup("code");
    }

    /* tool_preview_length: int(value) on success, 0 on failure */
    if (strcmp(setting, "tool_preview_length") == 0) {
        char *end = NULL;
        long n = strtol(value, &end, 10);
        if (end == value || *end != '\0') return strdup("0");
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", n);
        return strdup(buf);
    }

    /* default: passthrough */
    return strdup(value);
}

/* ================================================================
 *  Display config — resolve_display_setting()
 *  Port of Python gateway/display_config.py resolve_display_setting().
 * ================================================================ */

/* Resolve a display setting with per-platform override support.
 * Resolution order: platform override -> global setting -> default.
 * Returns a char* (caller must free). NULL if fallback was NULL.
 * AG26: Port of Python gateway/display_config.py:resolve_display_setting().
 */
/* PoP: resolve_display_setting @ gateway/display_config.py:resolve_display_setting */
char *resolve_display_setting(json_node_t *user_config,
                               const char *platform_key,
                               const char *setting,
                               const char *fallback) {
    if (!user_config || !setting) return fallback ? strdup(fallback) : NULL;
    json_node_t *display = json_object_get(user_config, "display");
    if (!display || display->type != JSON_OBJECT)
        return fallback ? strdup(fallback) : NULL;
    if (platform_key && *platform_key) {
        json_node_t *platforms = json_object_get(display, "platforms");
        if (platforms && platforms->type == JSON_OBJECT) {
            json_node_t *plat = json_object_get(platforms, platform_key);
            if (plat && plat->type == JSON_OBJECT) {
                json_node_t *val = json_object_get(plat, setting);
                if (val) {
                    if (val->type == JSON_STRING)
                        return normalise_display_value(setting, val->str_val);
                    if (val->type == JSON_BOOL)
                        return strdup(val->bool_val ? "true" : "false");
                    if (val->type == JSON_NUMBER) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%.0f", val->num_val);
                        return strdup(buf);
                    }
                }
            }
        }
    }
    json_node_t *global = json_object_get(display, setting);
    if (global) {
        if (global->type == JSON_STRING)
            return normalise_display_value(setting, global->str_val);
        if (global->type == JSON_BOOL)
            return strdup(global->bool_val ? "true" : "false");
        if (global->type == JSON_NUMBER) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f", global->num_val);
            return strdup(buf);
        }
    }
    return fallback ? strdup(fallback) : NULL;
}

