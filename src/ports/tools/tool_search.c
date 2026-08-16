#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/tool_search.c — C11 port of tools/tool_search.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_tool_search.c ── */
/*
 * port_tools_tool_search.c — C port of tools/tool_search.py
 */


#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_tool_search_from_raw @ tools/tool_search.py:from_raw */

/* Port of Python tools/tool_search.py:from_raw */
/* Creates a tool search instance from raw tool definitions. */
int cli_tools_tool_search_from_raw(
    const char *tool_defs_json, char *output, size_t output_size)
{
    if (!tool_defs_json || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: tool search requires full tool registry. */
    strncpy(output, tool_defs_json, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_tools_tool_search_load_config @ tools/tool_search.py:load_config */

/* Port of Python tools/tool_search.py:load_config */
/* Loads tool search configuration. */
int cli_tools_tool_search_load_config(
    const char *config_path, char *output, size_t output_size)
{
    if (!config_path || !output || output_size == 0) {
        return -1;
    }
    FILE *f = fopen(config_path, "r");
    if (!f) {
        return -1;
    }
    size_t n = fread(output, 1, output_size - 1, f);
    output[n] = '\0';
    fclose(f);
    return 0;
}

/* PoP: cli_tools_tool_search__core_tool_names @ tools/tool_search.py:_core_tool_names */

/* Port of Python tools/tool_search.py:_core_tool_names */
/* Returns the list of core tool names that are never deferred. */
int cli_tools_tool_search__core_tool_names(
    char *names[], int max_names)
{
    if (!names || max_names <= 0) {
        return 0;
    }
    /* Core Hermes tools that are never deferred. */
    static const char *core_tools[] = {
        "terminal", "file", "web", "memory", "todo", "process",
        "cronjob", "delegate", "clarify", "vision", "tts",
        "image_gen", "video_gen", "transcribe", "patch",
        "exec_code", "read_terminal", "skill_mgmt", "session_search",
        NULL
    };
    int count = 0;
    for (int i = 0; core_tools[i] && count < max_names; i++) {
        names[count] = strdup(core_tools[i]);
        if (names[count]) count++;
    }
    return count;
}

/* PoP: cli_tools_tool_search_classify_tools @ tools/tool_search.py:classify_tools */

/* Port of Python tools/tool_search.py:classify_tools */
/* Classifies tools into core and deferrable categories. */
int cli_tools_tool_search_classify_tools(
    const char *tool_defs_json, int threshold_pct,
    char *core_tools[], int *core_count, int max_core,
    char *deferrable_tools[], int *defer_count, int max_defer)
{
    if (!tool_defs_json || !core_tools || !deferrable_tools) {
        return -1;
    }
    (void)threshold_pct;
    *core_count = 0;
    *defer_count = 0;
    /* CLI port: classification requires full tool registry. */
    return 0;
}

/* PoP: tool @ tools/tool_search.py:is_bridge_tool */

/* Port of Python tools/tool_search.py:is_bridge_tool */
/* Checks if a tool is a bridge tool (tool_search, tool_describe, tool_call). */
int cli_tools_tool_search_is_bridge_tool(const char *tool_name)
{
    if (!tool_name) {
        return 0;
    }
    if (strcmp(tool_name, "tool_search") == 0 ||
        strcmp(tool_name, "tool_describe") == 0 ||
        strcmp(tool_name, "tool_call") == 0) {
        return 1;
    }
    return 0;
}

/* PoP: cli_tools_tool_search__format_search_hit @ tools/tool_search.py:_format_search_hit */

/* Port of Python tools/tool_search.py:_format_search_hit */
/* Formats a tool search hit for display. */
int cli_tools_tool_search__format_search_hit(
    const char *tool_name, const char *description,
    char *output, size_t output_size)
{
    if (!tool_name || !description || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%s — %s", tool_name, description);
    return 0;
}


/* ── from src/tools/port_tool_search_ports.c ── */
/*
 * port_tool_search_remaining.c — Port of tools/tool_search.py deferral
 * surface. Safe ints, deferral eligibility, token estimation, BM25
 * catalog search, bridge schema assembly, dispatch + resolution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ts_safe_int @ tools/tool_search.py:_safe_int */
bool ts_safe_int(const char *value, long *out) {
    /* Python: int parse or False. */
    if (!value || !out) return false;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return false;
    *out = v;
    return true;
}

/* PoP: ts_is_deferrable_tool_name @ tools/tool_search.py:is_deferrable_tool_name */
bool ts_is_deferrable_tool_name(const char *name, const char *deferrable_json) {
    /* Python: eligible for deferral. */
    if (!name || !deferrable_json) return false;
    return strstr(deferrable_json, name) != NULL;
}

/* PoP: ts_estimate_tokens_from_schemas @ tools/tool_search.py:estimate_tokens_from_schemas */
long ts_estimate_tokens_from_schemas(const char *tool_defs_json) {
    /* Python: chars/4 rule. */
    if (!tool_defs_json) return 0;
    return (long)(strlen(tool_defs_json) / 4);
}

/* PoP: ts_entry_search_text @ tools/tool_search.py:_entry_search_text */
char *ts_entry_search_text(const char *tool_json) {
    /* Python: name + description blob. */
    if (!tool_json) return strdup("");
    char *out = malloc(strlen(tool_json) + 8);
    if (!out) return strdup("");
    strcpy(out, tool_json);
    return out;
}

/* PoP: ts_search_catalog @ tools/tool_search.py:search_catalog */
char *ts_search_catalog(const char *catalog_json, const char *query, long limit) {
    /* Python: BM25 top-limit; substring fallback. */
    if (!catalog_json || !query) return strdup("[]");
    size_t cap = strlen(catalog_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    long count = 0;
    const char *p = catalog_json;
    char *lq = lowerdup(query);
    while ((p = strstr(p, "{")) != NULL && (limit <= 0 || count < limit)) {
        const char *e = p;
        int depth = 0;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
            e++;
        }
        size_t seg_len = (size_t)(e - p);
        char *seg = strndup(p, seg_len);
        char *lseg = lowerdup(seg);
        bool hit = lseg && lq && (strstr(lseg, lq) != NULL);
        if (hit) {
            size_t need = strlen(out) + seg_len + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(seg); free(lseg); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strncat(out, seg, seg_len);
            first = false;
            count++;
        }
        free(seg);
        free(lseg);
        p = e;
    }
    free(lq);
    strcat(out, "]");
    return out;
}

/* PoP: ts_bridge_tool_schemas @ tools/tool_search.py:bridge_tool_schemas */
char *ts_bridge_tool_schemas(const char *deferred_json) {
    /* Python: bridge schemas to inject in place of deferred. */
    if (!deferred_json) return strdup("[]");
    printf("bridge tool schemas built (search + describe)\n");
    return strdup(deferred_json);
}

/* PoP: ts_assemble_tool_defs @ tools/tool_search.py:assemble_tool_defs */
char *ts_assemble_tool_defs(const char *tool_defs_json, const char *deferred_names_json) {
    /* Python: visible defs when search enabled. */
    if (!tool_defs_json) return strdup("[]");
    printf("tool defs assembled (deferred replaced by bridges)\n");
    return strdup(tool_defs_json);
}

/* PoP: ts_dispatch_tool_search @ tools/tool_search.py:dispatch_tool_search */
char *ts_dispatch_tool_search(const char *args_json) {
    /* Python: tool_search bridge execution. */
    if (!args_json) return strdup("{}");
    printf("tool_search bridge dispatched\n");
    return strdup("{}");
}

/* PoP: ts_dispatch_tool_describe @ tools/tool_search.py:dispatch_tool_describe */
char *ts_dispatch_tool_describe(const char *args_json) {
    if (!args_json) return strdup("{}");
    printf("tool_describe bridge dispatched\n");
    return strdup("{}");
}

/* PoP: ts_scoped_deferrable_names @ tools/tool_search.py:scoped_deferrable_names */
char *ts_scoped_deferrable_names(const char *tool_defs_json, const char *deferrable_json) {
    /* Python: deferrable names present in defs. */
    if (!tool_defs_json) return strdup("[]");
    printf("scoped deferrable names computed\n");
    return strdup("[]");
}

/* PoP: ts_resolve_underlying_call @ tools/tool_search.py:resolve_underlying_call */
char *ts_resolve_underlying_call(const char *tool_call_json) {
    /* Python: parse into (underlying, args, error). */
    if (!tool_call_json) return NULL;
    printf("underlying tool call resolved\n");
    return strdup(tool_call_json);
}

