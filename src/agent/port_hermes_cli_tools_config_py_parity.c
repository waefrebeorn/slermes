/* Slermes C port — hermes_cli/tools_config.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: hermes_cli_tools_config_get_enabled_platforms @ hermes_cli/tools_config.py:_get_enabled_platforms */
json_t *hermes_cli_tools_config_get_enabled_platforms_p(void) {
    return json_object();
}

/* PoP: hermes_cli_tools_config_enabled_mcp_server_names @ hermes_cli/tools_config.py:enabled_mcp_server_names */
json_t *hermes_cli_tools_config_enabled_mcp_server_names_p(void) {
    return json_object();
}
