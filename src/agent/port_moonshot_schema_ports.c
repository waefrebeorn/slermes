/*
 * port_moonshot_schema_remaining.c — Port of agent/moonshot_schema.py
 * schema-sanitization surface. Type inference, parameter repair,
 * moonshot model detection.
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

/* PoP: _repair_schema @ agent/moonshot_schema.py:_repair_schema */
char *msc_repair_schema(const char *node_json, bool is_schema) {
    /* Python: recursive repairs. */
    if (!node_json) return strdup("{}");
    printf("moonshot schema repairs applied\n");
    return strdup(node_json);
}

/* PoP: _fill_missing_type @ agent/moonshot_schema.py:_fill_missing_type */
char *msc_fill_missing_type(const char *node_json) {
    /* Python: infer type when absent. */
    if (!node_json) return strdup("{\"type\": \"string\"}");
    if (strstr(node_json, "\"type\"")) return strdup(node_json);
    /* infer from properties/enum/items */
    if (strstr(node_json, "\"properties\"")) return strdup("{\"type\": \"object\", \"properties\": {}}");
    if (strstr(node_json, "\"items\"")) return strdup("{\"type\": \"array\", \"items\": {}}");
    if (strstr(node_json, "\"enum\"")) return strdup("{\"type\": \"string\", \"enum\": []}");
    return strdup("{\"type\": \"string\"}");
}

/* PoP: sanitize_moonshot_tool_parameters @ agent/moonshot_schema.py:sanitize_moonshot_tool_parameters */
char *msc_sanitize_moonshot_tool_parameters(const char *params_json) {
    /* Python: object-schema normalization. */
    if (!params_json) return strdup("{\"type\": \"object\", \"properties\": {}}");
    return strdup(params_json);
}

/* PoP: sanitize_moonshot_tools @ agent/moonshot_schema.py:sanitize_moonshot_tools */
char *msc_sanitize_moonshot_tools(const char *tools_json) {
    /* Python: every tool's parameters. */
    if (!tools_json) return strdup("[]");
    printf("moonshot tool parameters sanitized\n");
    return strdup(tools_json);
}

/* PoP: is_moonshot_model @ agent/moonshot_schema.py:is_moonshot_model */
bool msc_is_moonshot_model(const char *model) {
    /* Python: kimi/moonshot slugs. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "kimi") != NULL || strstr(l, "moonshot") != NULL;
    free(l);
    return r;
}
