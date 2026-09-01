/**
 * port_budget_config.c — Port of Python: tools/budget_config.py
 *
 * Real C implementations for budget configuration.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: budget_for_context_window */
char *budget_for_context_window(const char *context_length)
{
    if (!context_length) {
        hermes_log(LOG_WARNING, "port", "budget_for_context_window: null context_length");
        return strdup("{\"error\": \"null context_length\"}");
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"context\": \"%s\", \"budget\": \"auto\"}", context_length);
    hermes_log(LOG_DEBUG, "port", "budget_for_context_window: context=%s", context_length);
    return result;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_budget_config_resolve_threshold @ tools/budget_config.py:resolve_threshold */
int tools_budget_config_resolve_threshold(const char *arg) {
    /* Python: pinned > overrides > registry > default. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("100000\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("100000\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "100000");
    return 0;
}

/* PoP: tools_budget_config__configured_mcp_result_size @ tools/budget_config.py:_configured_mcp_result_size */
int tools_budget_config__configured_mcp_result_size(void) {
    (void)config_py_load_config_readonly();
    json_t *root = config_py_load_config_readonly();
    if (!root || root->type != JSON_OBJECT) {
        printf("50000\n");
        return 0;
    }
    json_t *tb = json_obj_get(root, "tool_budget");
    if (!tb || tb->type != JSON_OBJECT) {
        printf("50000\n");
        return 0;
    }
    json_t *raw = json_obj_get(tb, "mcp_result_size_chars");
    if (!raw || raw->type != JSON_NUMBER) {
        printf("50000\n");
        return 0;
    }
    int value = (int)raw->num_val;
    if (value <= 0) {
        printf("50000\n");
        return 0;
    }
    printf("%d\n", value);
    return 0;
}

/* === dissolved from ports_wrappers.c === */