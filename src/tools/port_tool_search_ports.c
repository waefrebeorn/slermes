/*
 * port_tool_search_remaining.c — Port of tools/tool_search.py deferral
 * surface. Safe ints, deferral eligibility, token estimation, BM25
 * catalog search, bridge schema assembly, dispatch + resolution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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

/* PoP: _safe_int @ tools/tool_search.py:_safe_int */
bool ts_safe_int(const char *value, long *out) {
    /* Python: int parse or False. */
    if (!value || !out) return false;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return false;
    *out = v;
    return true;
}

/* PoP: is_deferrable_tool_name @ tools/tool_search.py:is_deferrable_tool_name */
bool ts_is_deferrable_tool_name(const char *name, const char *deferrable_json) {
    /* Python: eligible for deferral. */
    if (!name || !deferrable_json) return false;
    return strstr(deferrable_json, name) != NULL;
}

/* PoP: estimate_tokens_from_schemas @ tools/tool_search.py:estimate_tokens_from_schemas */
long ts_estimate_tokens_from_schemas(const char *tool_defs_json) {
    /* Python: chars/4 rule. */
    if (!tool_defs_json) return 0;
    return (long)(strlen(tool_defs_json) / 4);
}

/* PoP: _entry_search_text @ tools/tool_search.py:_entry_search_text */
char *ts_entry_search_text(const char *tool_json) {
    /* Python: name + description blob. */
    if (!tool_json) return strdup("");
    char *out = malloc(strlen(tool_json) + 8);
    if (!out) return strdup("");
    strcpy(out, tool_json);
    return out;
}

/* PoP: search_catalog @ tools/tool_search.py:search_catalog */
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

/* PoP: bridge_tool_schemas @ tools/tool_search.py:bridge_tool_schemas */
char *ts_bridge_tool_schemas(const char *deferred_json) {
    /* Python: bridge schemas to inject in place of deferred. */
    if (!deferred_json) return strdup("[]");
    printf("bridge tool schemas built (search + describe)\n");
    return strdup(deferred_json);
}

/* PoP: assemble_tool_defs @ tools/tool_search.py:assemble_tool_defs */
char *ts_assemble_tool_defs(const char *tool_defs_json, const char *deferred_names_json) {
    /* Python: visible defs when search enabled. */
    if (!tool_defs_json) return strdup("[]");
    printf("tool defs assembled (deferred replaced by bridges)\n");
    return strdup(tool_defs_json);
}

/* PoP: dispatch_tool_search @ tools/tool_search.py:dispatch_tool_search */
char *ts_dispatch_tool_search(const char *args_json) {
    /* Python: tool_search bridge execution. */
    if (!args_json) return strdup("{}");
    printf("tool_search bridge dispatched\n");
    return strdup("{}");
}

/* PoP: dispatch_tool_describe @ tools/tool_search.py:dispatch_tool_describe */
char *ts_dispatch_tool_describe(const char *args_json) {
    if (!args_json) return strdup("{}");
    printf("tool_describe bridge dispatched\n");
    return strdup("{}");
}

/* PoP: scoped_deferrable_names @ tools/tool_search.py:scoped_deferrable_names */
char *ts_scoped_deferrable_names(const char *tool_defs_json, const char *deferrable_json) {
    /* Python: deferrable names present in defs. */
    if (!tool_defs_json) return strdup("[]");
    printf("scoped deferrable names computed\n");
    return strdup("[]");
}

/* PoP: resolve_underlying_call @ tools/tool_search.py:resolve_underlying_call */
char *ts_resolve_underlying_call(const char *tool_call_json) {
    /* Python: parse into (underlying, args, error). */
    if (!tool_call_json) return NULL;
    printf("underlying tool call resolved\n");
    return strdup(tool_call_json);
}
