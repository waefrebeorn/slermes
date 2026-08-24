/*
 * acp_adapter_tools.c — canonical C11 port of acp_adapter/tools.py
 * (tool-kind mapping, tool-call ids, human titles, result formatting).
 *
 * The Python side wraps ACP SDK objects; the C port produces the same JSON
 * wire shapes (ACP transport is JSON-RPC).
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── tool kind ───────────────────────────────────────────────────────── */

/* PoP: acpt_get_tool_kind @ acp_adapter/tools.py:get_tool_kind (TOOL_KIND_MAP) */
void acpt_get_tool_kind(const char *tool_name, char *out, size_t out_sz) {
    static const struct { const char *tool, *kind; } MAP[] = {
        {"terminal","execute"}, {"code_execution_tool","execute"},
        {"read_file","read"},   {"read_extract","read"},
        {"write_file","edit"},  {"patch","edit"}, {"file_tools","edit"},
        {"search_files","search"}, {"web_search","fetch"},
        {"web_extract","fetch"}, {"browser_exec","other"},
        {"delegate_task","think"}, {NULL,NULL}
    };
    for (int i = 0; MAP[i].tool; i++)
        if (strcmp(tool_name, MAP[i].tool) == 0) {
            snprintf(out, out_sz, "%s", MAP[i].kind); return;
        }
    snprintf(out, out_sz, "other");
}

/* PoP: acpt_make_tool_call_id @ acp_adapter/tools.py:make_tool_call_id */
void acpt_make_tool_call_id(char *out, size_t out_sz) {
    unsigned seed = (unsigned)time(NULL) ^ (unsigned)(size_t)out;
    snprintf(out, out_sz, "tc-%x%08x", seed ^ (unsigned)(seed<<7), seed*2654435761u);
}

/* ── tool title ──────────────────────────────────────────────────────── */

static void json_first_str(json_t *obj, const char *key, char *out, size_t sz) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    snprintf(out, sz, "%s",
        (v && json_is_string(v)) ? json_string_value(v) : "?");
}

/* PoP: acpt_build_tool_title @ acp_adapter/tools.py:build_tool_title */
void acpt_build_tool_title(const char *tool_name, const char *args_json,
                           char *out, size_t out_sz) {
    json_t *args = args_json ? json_parse(args_json, NULL) : NULL;
    if (strcmp(tool_name, "terminal") == 0) {
        char cmd[128]; json_first_str(args, "command", cmd, sizeof cmd);
        size_t n = strlen(cmd);
        snprintf(out, out_sz, n > 80 ? "terminal: %.77s..."
                                     : "terminal: %s", cmd);
    } else if (strcmp(tool_name, "read_file") == 0 || strcmp(tool_name,"read_extract")==0) {
        char p[512]; json_first_str(args, "path", p, sizeof p);
        snprintf(out, out_sz, "read: %s", p);
    } else if (strcmp(tool_name, "write_file") == 0) {
        char p[512]; json_first_str(args, "path", p, sizeof p);
        snprintf(out, out_sz, "write: %s", p);
    } else if (strcmp(tool_name, "patch") == 0) {
        char mode[32], path[512];
        json_first_str(args, "mode", mode, sizeof mode);
        json_first_str(args, "path", path, sizeof path);
        if (!mode[0]) strcpy(mode, "replace");
        snprintf(out, out_sz, "patch (%s): %s", mode, path);
    } else if (strcmp(tool_name, "search_files") == 0) {
        char pat[256]; json_first_str(args, "pattern", pat, sizeof pat);
        snprintf(out, out_sz, "search: %s", pat);
    } else if (strcmp(tool_name, "web_search") == 0) {
        char q[256]; json_first_str(args, "query", q, sizeof q);
        snprintf(out, out_sz, "web search: %s", q);
    } else {
        snprintf(out, out_sz, "%s", tool_name);
    }
    if (args) json_free(args);
}

/* ── result formatting helpers ───────────────────────────────────────── */

/* PoP: acpt_truncate_text @ acp_adapter/tools.py:_truncate_text (limit 5000) */
void acpt_truncate_text(const char *text, int limit, char *out, size_t out_sz) {
    int n = text ? (int)strlen(text) : 0;
    if (limit <= 0) limit = 5000;
    if (n <= limit) snprintf(out, out_sz, "%s", text ? text : "");
    else            snprintf(out, out_sz, "%.*s…", limit, text ? text : "");
}

/* PoP: acpt_fenced_text @ acp_adapter/tools.py:_fenced_text */
void acpt_fenced_text(const char *text, const char *language,
                      char *out, size_t out_sz) {
    snprintf(out, out_sz, "```%s\n%s\n```", language?language:"", text?text:"");
}

/* PoP: acpt_tool_result_failed @ acp_adapter/tools.py:_tool_result_failed */
int acpt_tool_result_failed(const char *result_json, const char *tool_name) {
    (void)tool_name;
    if (!result_json || !*result_json) return 1;
    json_t *r = json_parse(result_json, NULL);
    if (!r) return 1; /* unparseable = failed */
    json_t *ok = json_obj_get(r, "ok");
    int failed = !(ok && ok->type == JSON_BOOL && ok->bool_val);
    json_free(r);
    return failed;
}
