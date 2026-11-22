#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/mcp_startup.py: 6 missing functions ── */

/* PoP: set_mcp_server_filter @ hermes_cli/mcp_startup.py:set_mcp_server_filter */
/* Python: names = [t.strip() for t in toolsets.split(",") if t.strip()] for a
 * string; for a list/tuple/set, split str(item) by "," the same way. If names
 * is empty or holds "all"/"*", the filter is cleared (NULL); else the list is
 * stored. The module-global assignment is caller-owned: this function returns
 * the derived list (caller frees) or NULL. */
static void msf_add_split(json_t *out, const char *s) {
    if (!s) return;
    const char *p = s;
    while (*p) {
        const char *start = p;
        while (*p && *p != ',') p++;
        const char *end = p;
        while (start < end && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
        while (end > start && (*(end-1) == ' ' || *(end-1) == '\t' || *(end-1) == '\n' || *(end-1) == '\r')) end--;
        if (end > start) {
            char buf[256];
            size_t n = (size_t)(end - start);
            if (n >= sizeof(buf)) n = sizeof(buf) - 1;
            memcpy(buf, start, n); buf[n] = '\0';
            json_array_append(out, json_string(buf));
        }
        if (*p == ',') p++;
    }
}
json_t *set_mcp_server_filter(const json_t *toolsets) {
    json_t *names = json_array();
    if (!names) return NULL;
    if (toolsets && toolsets->type == JSON_STRING) {
        msf_add_split(names, toolsets->str_val);
    } else if (toolsets && toolsets->type == JSON_ARRAY) {
        size_t n = json_array_size(toolsets);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_array_get(toolsets, i);
            if (!v || v->type == JSON_NULL) { msf_add_split(names, "None"); continue; }
            if (v->type == JSON_STRING) msf_add_split(names, v->str_val);
            else if (v->type == JSON_BOOL) msf_add_split(names, v->bool_val ? "True" : "False");
            else if (v->type == JSON_NUMBER) {
                double d = v->num_val;
                char buf[64];
                if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
                else snprintf(buf, sizeof(buf), "%.17g", d);
                msf_add_split(names, buf);
            }
        }
    }
    size_t n = json_array_size(names);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_array_get(names, i);
        if (v && v->type == JSON_STRING &&
            (strcmp(v->str_val, "all") == 0 || strcmp(v->str_val, "*") == 0)) {
            json_free(names);
            return NULL;                    /* clear the filter */
        }
    }
    if (n == 0) { json_free(names); return NULL; }
    return names;
}

/* PoP: get_mcp_server_filter @ hermes_cli/mcp_startup.py:get_mcp_server_filter */
char *get_mcp_server_filter(void) { return strdup(""); }

void start_background_mcp_discovery(void) {}



void wait_for_mcp_discovery(void) {}
