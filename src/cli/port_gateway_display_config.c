/*
 * port_gateway_display_config.c — C port of gateway/display_config.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_gateway_display_config__normalise @ gateway/display_config.py:_normalise */

/*
 * Display setting value for normalized results.
 */
typedef struct {
    char   str_val[256];
    int    int_val;
    bool   bool_val;
    int    type; /* 0=string, 1=int, 2=bool */
} display_value_t;

/*
 * _normalise: Normalise YAML quirks for display settings.
 *
 * p1 = setting name string
 * p2 = value string (YAML-parsed)
 * p3 = pointer to display_value_t for result
 *
 * Returns: pointer to display_value_t with normalized value.
 */
void* cli_gateway_display_config__normalise(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;

    const char *setting = (const char *)p1;
    const char *value = (const char *)p2;
    display_value_t *result = (display_value_t *)p3;

    if (!setting || !value || !result) return result;

    memset(result, 0, sizeof(*result));

    if (strcmp(setting, "tool_progress") == 0) {
        /* False -> "off", True -> "all", else lowercase string */
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 ||
            strcmp(value, "no") == 0 || strcmp(value, "off") == 0) {
            strcpy(result->str_val, "off");
            result->type = 0;
        } else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                   strcmp(value, "yes") == 0 || strcmp(value, "all") == 0) {
            strcpy(result->str_val, "all");
            result->type = 0;
        } else {
            /* Lowercase the value */
            size_t len = strlen(value);
            if (len >= sizeof(result->str_val)) len = sizeof(result->str_val) - 1;
            for (size_t i = 0; i < len; i++) {
                result->str_val[i] = tolower((unsigned char)value[i]);
            }
            result->str_val[len] = '\0';
            result->type = 0;
        }
    } else if (strcmp(setting, "show_reasoning") == 0 ||
               strcmp(setting, "streaming") == 0 ||
               strcmp(setting, "interim_assistant_messages") == 0 ||
               strcmp(setting, "long_running_notifications") == 0 ||
               strcmp(setting, "busy_ack_detail") == 0 ||
               strcmp(setting, "cleanup_progress") == 0) {
        /* Boolean: "true"/"1"/"yes"/"on" -> true */
        result->bool_val = (strcmp(value, "true") == 0 ||
                            strcmp(value, "1") == 0 ||
                            strcmp(value, "yes") == 0 ||
                            strcmp(value, "on") == 0);
        result->type = 2;
    } else if (strcmp(setting, "tool_preview_length") == 0) {
        result->int_val = atoi(value);
        result->type = 1;
    } else {
        /* Unknown setting: pass through as string */
        strncpy(result->str_val, value, sizeof(result->str_val) - 1);
        result->str_val[sizeof(result->str_val) - 1] = '\0';
        result->type = 0;
    }

    return result;
}
