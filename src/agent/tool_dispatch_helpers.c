/* Slermes C port — agent/tool_dispatch_helpers.py (pure mutation-tracking helpers)
 *
 * Faithful port of _neutralize_delimiters and _extract_landed_file_mutation_paths.
 * _extract_landed_file_mutation_paths depends on _extract_file_mutation_targets
 * (re-implemented here) and a minimal JSON parse of the result string.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
#include "hermes_json.h"
#include "libtooldispatch/tool_dispatch_helpers.h"

/* From port_agent_core_helpers.c (agent/tool_dispatch_helpers.py:_canonical_path). */
extern char *tdh_canonical_path(const char *raw_path, const char *execution_cwd);

/* File-mutating tool names (FILE_MUTATING_TOOL_NAMES in Python). */
static const char *FILE_MUTATING_TOOLS[] = { "write_file", "patch", NULL };

/* PoP: agent_tool_dispatch_neutralize_delimiters @ agent/tool_dispatch_helpers.py:_neutralize_delimiters */
char *agent_tool_dispatch_neutralize_delimiters(const char *content)
{
    if (!content) return NULL;
    /* Replace every case-insensitive occurrence of "untrusted_tool_result"
     * with "untrusted-tool-result" (only the underscore becomes a hyphen;
     * all other characters, including case, are preserved). */
    size_t clen = strlen(content);
    char *out = malloc(clen + 1);
    size_t o = 0;
    for (size_t i = 0; i < clen; ) {
        bool match = false;
        if (i + 21 <= clen) {
            const char *lit = "untrusted_tool_result";
            match = true;
            for (int k = 0; k < 21; k++) {
                char c = content[i + k];
                if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                if (c != lit[k]) { match = false; break; }
            }
        }
        if (match) {
            /* The Python .sub() replaces with a FIXED lowercase string
             * "untrusted-tool-result" regardless of the matched case. */
            const char *rep = "untrusted-tool-result";
            for (int k = 0; k < 21; k++) out[o++] = rep[k];
            i += 21;
        } else {
            out[o++] = content[i++];
        }
    }
    out[o] = '\0';
    return out;
}

static bool is_file_mutating(const char *tool_name)
{
    for (int i = 0; FILE_MUTATING_TOOLS[i]; i++)
        if (strcmp(tool_name, FILE_MUTATING_TOOLS[i]) == 0) return true;
    return false;
}

/* Minimal JSON string/array extraction for files_modified / resolved_path.
 * Only handles the specific shapes the Python code reads. */
static bool json_get_string_field(const char *json, const char *key, char *out, size_t outsz)
{
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"[ \t]*:[ \t]*\"", key);
    regex_t re; if (regcomp(&re, pat, REG_EXTENDED) != 0) return false;
    regmatch_t m; char *buf = strdup(json); bool found = false;
    if (regexec(&re, buf, 1, &m, 0) == 0) {
        char *start = buf + m.rm_eo - 1;  /* the value's opening quote */
        if (start >= buf && *start == '"') {
            size_t i = 1, o = 0;
            while (start[i] && start[i] != '"') {
                if (start[i] == '\\' && start[i+1]) {
                    char e = start[i+1];
                    if (e == 'n') out[o++] = '\n';
                    else if (e == 't') out[o++] = '\t';
                    else if (e == 'r') out[o++] = '\r';
                    else out[o++] = e;   /* \\ and \" and \/ */
                    i += 2;
                } else {
                    if (o < outsz - 1) out[o++] = start[i];
                    i++;
                }
            }
            out[o] = '\0'; found = true;
        }
    }
    free(buf); regfree(&re); return found;
}

/* PoP: agent_tool_dispatch_extract_landed_file_mutation_paths @ agent/tool_dispatch_helpers.py:_extract_landed_file_mutation_paths */
char **agent_tool_dispatch_extract_landed_file_mutation_paths(const char *tool_name,
                                                              const char *args_json,
                                                              const char *result, int *out_n)
{
    /* Parse path/mode/patch from args_json (mirrors Python's args dict). */
    char path[1024]; path[0] = '\0';
    char mode[64]; mode[0] = '\0';
    char patch[8192]; patch[0] = '\0';
    if (args_json) {
        json_get_string_field(args_json, "path", path, sizeof(path));
        json_get_string_field(args_json, "mode", mode, sizeof(mode));
        json_get_string_field(args_json, "patch", patch, sizeof(patch));
    }
    int targets_n = 0;
    char *args_buf = NULL;
    /* rebuild minimal args json for the lib ABI */
    size_t ab_len = strlen(path)+strlen(mode)+strlen(patch)+64;
    args_buf = malloc(ab_len);
    snprintf(args_buf, ab_len, "{\"path\":\"%s\",\"mode\":\"%s\",\"patch\":%s}",
             path, mode, patch[0] ? patch : "\"\"");
    char **targets = extract_file_mutation_targets(tool_name, args_buf,
                                                   (size_t *)&targets_n);
    free(args_buf);
    if (!is_file_mutating(tool_name) || !result) { *out_n = targets_n; return targets; }
    /* try JSON parse of result */
    char *rbuf = strdup(result);
    /* trim leading whitespace */
    char *rp = rbuf; while (*rp == ' ' || *rp == '\t' || *rp == '\n' || *rp == '\r') rp++;
    if (rp[0] == '{') {
        /* files_modified array */
        regex_t re; regcomp(&re, "\"files_modified\"[ \t]*:[ \t]*\\[", REG_EXTENDED);
        regmatch_t m;
        if (regexec(&re, rp, 1, &m, 0) == 0) {
            char *arr = rp + m.rm_eo;
            /* collect strings until matching ] */
            int cap = targets_n + 4, n = targets_n;
            targets = realloc(targets, sizeof(char *) * cap);
            char *p = arr;
            while (*p && *p != ']') {
                if (*p == '"') {
                    p++;
                    size_t i = 0; char val[1024];
                    while (*p && *p != '"') {
                        if (*p == '\\' && p[1]) { val[i++] = p[1]; p += 2; } else val[i++] = *p++;
                    }
                    val[i] = '\0'; p++;
                    if (val[0]) { if (n>=cap){cap*=2;targets=realloc(targets,sizeof(char*)*cap);} targets[n++]=strdup(val); }
                } else p++;
            }
            targets = realloc(targets, sizeof(char *) * (n + 1));
            targets[n] = NULL;
            *out_n = n;
            free(rbuf);
            regfree(&re);
            return targets;
        }
        regfree(&re);
        /* resolved_path */
        char rv[1024];
        if (json_get_string_field(rp, "resolved_path", rv, sizeof(rv)) && rv[0]) {
            targets = realloc(targets, sizeof(char *) * (targets_n + 2));
            targets[targets_n++] = strdup(rv);
            targets[targets_n] = NULL;
            *out_n = targets_n;
            free(rbuf);
            return targets;
        }
    }
    free(rbuf);
    *out_n = targets_n;
    return targets;
}

/* PoP: array @ agent/tool_dispatch_helpers.py:_extract_parallel_scope_paths */
/* Return every canonical path this call reserves for overlap checks. For
 * "patch" in V4A mode, scope comes from patch-body "*** Update/Add/Delete/
 * Move File:" headers (via extract_file_mutation_targets). Otherwise the
 * "path" arg is used; search_files with no path reserves "." (cwd) rather
 * than demoting to a sequential barrier. Dedupes by canonical path. Returns
 * a malloc'd NULL-terminated array (or NULL when the tool isn't path-scoped).
 * execution_cwd may be NULL (fall back to process cwd). */
char **agent_tool_dispatch_extract_parallel_scope_paths(
    const char *tool_name, const char *args_json, const char *execution_cwd,
    int *out_n)
{
    *out_n = 0;
    if (!tool_name) return NULL;
    /* _PATH_SCOPED_TOOLS = {read_file, search_files, write_file, patch} */
    const char *PATH_SCOPED[] = { "read_file", "search_files", "write_file", "patch", NULL };
    int scoped_tool = 0;
    for (int i = 0; PATH_SCOPED[i]; i++)
        if (strcmp(tool_name, PATH_SCOPED[i]) == 0) { scoped_tool = 1; break; }
    if (!scoped_tool) return NULL;

    /* Parse path/mode/patch from args_json (mirrors Python's function_args). */
    char path[1024]; path[0] = '\0';
    char mode[64]; mode[0] = '\0';
    char patch[8192]; patch[0] = '\0';
    if (args_json) {
        json_get_string_field(args_json, "path", path, sizeof(path));
        json_get_string_field(args_json, "mode", mode, sizeof(mode));
        json_get_string_field(args_json, "patch", patch, sizeof(patch));
    }

    /* raw_paths accumulation. */
    char *raw_paths[128];
    int nraw = 0;
    /* Python: mode defaults to "replace"; V4A patch branch only when
     * tool=="patch" AND (mode or "replace")=="patch". */
    const char *eff_mode = mode[0] ? mode : "replace";
    if (strcmp(tool_name, "patch") == 0 && strcmp(eff_mode, "patch") == 0) {
        int tn = 0;
        char **targets = extract_file_mutation_targets(tool_name, args_json,
                                                       (size_t *)&tn);
        for (int i = 0; i < tn && nraw < 128; i++) raw_paths[nraw++] = targets[i];
        free_mutation_targets(targets, (size_t)tn);
    } else {
        if (path[0]) {
            raw_paths[nraw++] = strdup(path);
        } else if (strcmp(tool_name, "search_files") == 0) {
            raw_paths[nraw++] = strdup(".");
        }
    }

    /* Canonicalize + dedupe. */
    char **scoped = NULL;
    int n = 0, cap = 0;
    char *seen[128]; int nseen = 0;
    for (int i = 0; i < nraw; i++) {
        char *raw = raw_paths[i];
        if (!raw || !*raw) { free(raw); continue; }
        char *canonical = tdh_canonical_path(raw, execution_cwd);
        free(raw);
        if (!canonical) continue;
        /* dedupe by canonical string */
        int dup = 0;
        for (int s = 0; s < nseen; s++)
            if (strcmp(seen[s], canonical) == 0) { dup = 1; break; }
        if (dup) { free(canonical); continue; }
        if (nseen < 128) seen[nseen++] = strdup(canonical);
        if (n >= cap) { cap = cap ? cap * 2 : 4; scoped = realloc(scoped, sizeof(char *) * cap); }
        scoped[n++] = canonical;
    }
    for (int s = 0; s < nseen; s++) free(seen[s]);
    if (n > 0) {
        scoped = realloc(scoped, sizeof(char *) * (n + 1));
        scoped[n] = NULL;
    } else if (scoped) {
        free(scoped);
        scoped = NULL;
    }
    *out_n = n;
    return scoped;
}


/* === dissolved from ports_wrappers.c === */
/* The following 16 functions are NOT re-implemented here: the faithful C port
 * of agent/tool_dispatch_helpers.py lives in lib/libtooldispatch/
 * (tool_dispatch_helpers.c). These PoP wrappers delegate to that real module
 * so they are no longer silent no-ops. The wrapper ABI is `int` (legacy PoP
 * placeholder); return values map booleans to 1/0 and release any heap result. */

/* PoP: agent_tool_dispatch_helpers_u_is_destructive_command @ agent/tool_dispatch_helpers.py:_is_destructive_command */
int agent_tool_dispatch_helpers_u_is_destructive_command(const char *arg) {
    return is_destructive_command(arg) ? 1 : 0;
}

/* PoP: agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe @ agent/tool_dispatch_helpers.py:_is_mcp_tool_parallel_safe */
int agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe(const char *arg) {
    return is_mcp_tool_parallel_safe(arg) ? 1 : 0;
}

/* PoP: agent_tool_dispatch_helpers_u_plan_tool_batch_segments @ agent/tool_dispatch_helpers.py:_plan_tool_batch_segments */
int agent_tool_dispatch_helpers_u_plan_tool_batch_segments(const char *arg) {
    char *segs = plan_tool_batch_segments(arg, NULL);
    int ok = (segs != NULL) ? 1 : 0;
    free(segs);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_u_should_parallelize_tool_batch @ agent/tool_dispatch_helpers.py:_should_parallelize_tool_batch */
int agent_tool_dispatch_helpers_u_should_parallelize_tool_batch(const char *arg) {
    /* arg is a JSON array of {"name":..,"arguments":..}; reuse the segment planner. */
    char *segs = plan_tool_batch_segments(arg, NULL);
    int parallel = 0;
    if (segs) {
        /* "parallel" appears when the whole batch is one parallel segment. */
        parallel = (strstr(segs, "\"parallel\"") != NULL) ? 1 : 0;
        free(segs);
    }
    return parallel;
}

/* PoP: agent_tool_dispatch_helpers_u_extract_parallel_scope_path @ agent/tool_dispatch_helpers.py:_extract_parallel_scope_path */
int agent_tool_dispatch_helpers_u_extract_parallel_scope_path(const char *arg) {
    /* arg is a JSON {"name":..,"arguments":..} object; decompose it. */
    char *jerr = NULL;
    json_t *obj = json_parse(arg ? arg : "{}", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!obj || obj->type != JSON_OBJECT) { if (obj) json_free(obj); return 0; }
    const char *name = json_get_str(obj, "name", "");
    const char *args = json_get_str(obj, "arguments", "{}");
    parallel_scope_path_t p = extract_parallel_scope_path(name, args);
    int ok = p.ok ? 1 : 0;
    free_parallel_scope_path(&p);
    json_free(obj);
    return ok;
}

/* PoP: extract_parallel_scope_paths @ agent/tool_dispatch_helpers.py:_extract_parallel_scope_paths */
/* Wrapper exposing the lib's extract_parallel_scope_paths (returns a JSON
 * array of canonical scope-path strings for a path-scoped tool call). */
char *agent_tool_dispatch_helpers_u_extract_parallel_scope_paths(
    const char *tool_name, const char *args_json)
{
    size_t n = 0;
    char **paths = extract_parallel_scope_paths(tool_name, args_json, &n);
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++)
        json_append(arr, json_string(paths[i] ? paths[i] : ""));
    char *out = json_serialize(arr);
    json_free(arr);
    free_scope_paths(paths, n);
    return out ? out : strdup("[]");
}

/* PoP: agent_tool_dispatch_helpers_u_paths_overlap @ agent/tool_dispatch_helpers.py:_paths_overlap */
int agent_tool_dispatch_helpers_u_paths_overlap(const char *arg) {
    /* arg is a JSON [left, right] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *l = json_get(arr, 0), *r = json_get(arr, 1);
    int ov = 0;
    if (l && r && l->type == JSON_STRING && r->type == JSON_STRING)
        ov = paths_overlap(l->str_val, r->str_val) ? 1 : 0;
    json_free(arr);
    return ov;
}

/* PoP: agent_tool_dispatch_helpers_u_is_multimodal_tool_result @ agent/tool_dispatch_helpers.py:_is_multimodal_tool_result */
int agent_tool_dispatch_helpers_u_is_multimodal_tool_result(const char *arg) {
    return is_multimodal_tool_result(arg) ? 1 : 0;
}

/* PoP: agent_tool_dispatch_helpers_u_multimodal_text_summary @ agent/tool_dispatch_helpers.py:_multimodal_text_summary */
int agent_tool_dispatch_helpers_u_multimodal_text_summary(const char *arg) {
    char *s = multimodal_text_summary_from_json(arg);
    int ok = (s != NULL) ? 1 : 0;
    free(s);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal @ agent/tool_dispatch_helpers.py:_append_subdir_hint_to_multimodal */
int agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal(const char *arg) {
    /* arg is a JSON [result_json, hint] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int ok = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        char *res = append_subdir_hint_to_multimodal(a->str_val, b->str_val);
        ok = (res != NULL) ? 1 : 0;
        free(res);
    }
    json_free(arr);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_u_extract_file_mutation_targets @ agent/tool_dispatch_helpers.py:_extract_file_mutation_targets */
int agent_tool_dispatch_helpers_u_extract_file_mutation_targets(const char *arg) {
    /* arg is a JSON [tool_name, args_json] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int n = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        size_t cnt = 0;
        char **t = extract_file_mutation_targets(a->str_val, b->str_val, &cnt);
        n = (int)cnt;
        free_mutation_targets(t, cnt);
    }
    json_free(arr);
    return n;
}

/* PoP: agent_tool_dispatch_helpers_u_extract_error_preview @ agent/tool_dispatch_helpers.py:_extract_error_preview */
int agent_tool_dispatch_helpers_u_extract_error_preview(const char *arg) {
    /* arg is a JSON [result, max_len] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0);
    json_t *m = json_get(arr, 1);
    int len = 180;
    if (m && m->type == JSON_NUMBER) len = (int)m->num_val;
    int ok = 0;
    if (a && a->type == JSON_STRING) {
        char *e = extract_error_preview(a->str_val, (size_t)len);
        ok = (e != NULL) ? 1 : 0;
        free(e);
    }
    json_free(arr);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_u_trajectory_normalize_msg @ agent/tool_dispatch_helpers.py:_trajectory_normalize_msg */
int agent_tool_dispatch_helpers_u_trajectory_normalize_msg(const char *arg) {
    char *s = trajectory_normalize_msg_json(arg);
    int ok = (s != NULL) ? 1 : 0;
    free(s);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_make_tool_result_message @ agent/tool_dispatch_helpers.py:make_tool_result_message */
int agent_tool_dispatch_helpers_make_tool_result_message(const char *arg) {
    /* arg is a JSON [name, content, tool_call_id] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    const char *name = json_get_str(arr, "name", "");
    const char *content = json_get_str(arr, "content", "");
    const char *tcid = json_get_str(arr, "tool_call_id", "");
    char *msg = make_tool_result_message_json(name, content, tcid);
    int ok = (msg != NULL) ? 1 : 0;
    free(msg);
    json_free(arr);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_u_is_untrusted_tool @ agent/tool_dispatch_helpers.py:_is_untrusted_tool */
int agent_tool_dispatch_helpers_u_is_untrusted_tool(const char *arg) {
    return is_untrusted_tool(arg) ? 1 : 0;
}

/* PoP: agent_tool_dispatch_helpers_u_tool_output_risk_metadata @ agent/tool_dispatch_helpers.py:_tool_output_risk_metadata */
int agent_tool_dispatch_helpers_u_tool_output_risk_metadata(const char *arg) {
    /* arg is a JSON [tool_name, content] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int ok = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        char *meta = tool_output_risk_metadata(a->str_val, b->str_val);
        ok = (meta != NULL) ? 1 : 0;
        free(meta);
    }
    json_free(arr);
    return ok;
}

/* PoP: agent_tool_dispatch_helpers_u_maybe_wrap_untrusted @ agent/tool_dispatch_helpers.py:_maybe_wrap_untrusted */
int agent_tool_dispatch_helpers_u_maybe_wrap_untrusted(const char *arg) {
    /* arg is a JSON [name, content] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int ok = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        char *w = maybe_wrap_untrusted(a->str_val, b->str_val);
        ok = (w != NULL) ? 1 : 0;
        free(w);
    }
    json_free(arr);
    return ok;
}
