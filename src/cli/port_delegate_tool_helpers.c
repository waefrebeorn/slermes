/*
 * port_delegate_tool_helpers.c
 *
 * Pure, portable helpers ported from tools/delegate_tool.py. These are the
 * string/parse/role-normalization helpers that do NOT touch the live agent,
 * subprocess, or config IO:
 *   - _normalize_role              (role -> 'leaf'|'orchestrator')
 *   - _normalized_runtime_url      (strip + trailing-slash normalize)
 *   - _recover_tasks_from_json_string (parse+validate a tasks JSON string)
 *   - _trim_summary_with_footer    (head+tail window string-trim, no spill IO)
 *
 * The agent/toolset/subagent-coupled functions (_build_child_agent,
 * _strip_blocked_tools, _expand_parent_toolsets, the approval callbacks) are
 * NOT ported — they require the live registry/agent/IO.
 *
 * Module prefix used by the scanner for tools/delegate_tool.py is
 * "delegate_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "libjson/json.h"

/* --- runtime url normalization ----------------------------------------- */
/* PoP: _normalized_runtime_url @ tools/delegate_tool.py:_normalized_runtime_url */
/* str(value or "") then strip() then rstrip('/'). Allocates result (caller
 * frees). Returns malloc'd "" when input is NULL/empty. */
char *delegate_normalized_runtime_url(const char *value)
{
    const char *s = value ? value : "";
    size_t L = strlen(s);
    /* strip leading ws */
    size_t a = 0;
    while (a < L && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) a++;
    size_t b = L;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r')) b--;
    /* rstrip '/' */
    while (b > a && s[b-1] == '/') b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* --- tasks JSON string recovery (parse + validate) --------------------- */
/* PoP: _recover_tasks_from_json_string @ tools/delegate_tool.py:_recover_tasks_from_json_string */
/*
 * Parse a 'tasks' JSON string. Returns 1 and sets *out_json (malloc'd string
 * copy of the parsed array) when it is a non-empty JSON array; otherwise
 * returns 0 and sets *out_err (malloc'd error message). Caller frees whichever
 * is set.
 *
 * Mirrors Python: non-str -> (None,None); empty/whitespace -> guidance msg;
 * JSONDecodeError -> parse-error msg; non-list -> type-error msg; list ->
 * (parsed, None).
 *
 * We validate "is an array" structurally via a single-pass bracket/brace
 * scan after stripping leading whitespace (rejects scalars/objects). We do
 * NOT re-serialize; out_json is the trimmed original (Python returns the
 * parsed object, but the only downstream consumer is delegate_task which
 * re-JSON-loads it, so a faithful trimmed copy is equivalent behavior).
 */
int delegate_recover_tasks_from_json_string(const char *tasks, char **out_json, char **out_err)
{
    *out_json = NULL;
    *out_err = NULL;
    if (!tasks) return 0;
    const char *p = tasks;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) {
        *out_err = strdup("Provide either 'goal' (single task) or 'tasks' (batch).");
        return 0;
    }
    /* Must be a JSON array: leading non-ws char must be '['. */
    if (*p != '[') {
        /* Try a json parse to determine if it's an object/scalar for the msg.
         * Faithful message: "parsed <type> instead". We can't easily reflect
         * the python type name without a full parser, so we classify. */
        const char *q = p;
        while (*q == ' ' || *q == '\t') q++;
        const char *typename;
        if (*q == '{') typename = "dict";
        else if (*q == '"') typename = "str";
        else if (*q == 't' || *q == 'f') typename = "bool";
        else if (*q == 'n') typename = "NoneType";
        else if (*q >= '0' && *q <= '9') typename = "int";
        else typename = "value";
        size_t need = 90 + strlen(typename);
        char *e = malloc(need);
        snprintf(e, need,
                 "tasks must be a JSON array of task objects; parsed %s instead.",
                 typename);
        *out_err = e;
        return 0;
    }
    /* Structural validate: balanced [] and {} with no stray top-level scalar.
     /* Validate it parses as a JSON array using libjson (already linked). */
     const char *end = p + strlen(p);
     while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
     size_t len = (size_t)(end - p);
     char *copy = malloc(len + 1);
     memcpy(copy, p, len);
     copy[len] = '\0';
     char *jerr = NULL;
     json_t *root = json_parse(copy, &jerr);
     if (!root || root->type != JSON_ARRAY) {
         free(copy);
         if (root) json_free(root);
         if (jerr) free(jerr);
         *out_err = strdup(
             "tasks must be a JSON array of task objects; received a string "
             "that could not be parsed as JSON.");
         return 0;
     }
     json_free(root);
     if (jerr) free(jerr);
     *out_json = copy;
     return 1;
}

/* --- summary head/tail trim (footer omitted; pure string math) --------- */
/* PoP: _trim_summary_with_footer @ tools/delegate_tool.py:_trim_summary_with_footer */
/*
 * Snap a summary to a head+tail window (75% head / 25% tail) at line
 * boundaries, and return the model_text. The Python version also spills the
 * full text to a file and appends a footer with the spill path + read_file
 * offset; the spill path is IO-coupled so we omit it and return ONLY the
 * head+tail windowed text (the pure, deterministic part). out_model_text is
 * malloc'd (caller frees).
 */
char *delegate_trim_summary_with_footer(const char *summary, int cap)
{
    if (!summary) summary = "";
    int original_len = (int)strlen(summary);
    if (cap <= 0) cap = original_len > 0 ? original_len : 1;
    int head_budget = (int)((double)cap * 0.75);
    int tail_budget = cap - head_budget;
    if (head_budget < 0) head_budget = 0;
    if (tail_budget < 0) tail_budget = 0;

    int slen = original_len;
    if (slen <= cap) {
        return strdup(summary);
    }
    /* head = summary[:head_budget], tail = summary[-tail_budget:] */
    char *head = malloc(head_budget + 1);
    memcpy(head, summary, head_budget);
    head[head_budget] = '\0';
    /* snap head back to last newline if it's past the halfway point */
    int nl = -1;
    for (int i = 0; i < head_budget; i++) if (head[i] == '\n') nl = i;
    if (nl > head_budget / 2) { head[nl] = '\0'; }
    /* tail */
    char *tail = malloc(tail_budget + 1);
    memcpy(tail, summary + slen - tail_budget, tail_budget);
    tail[tail_budget] = '\0';
    int tnl = -1;
    for (int i = 0; i < tail_budget; i++) if (tail[i] == '\n') { tnl = i; break; }
    if (tnl >= 0 && tnl < tail_budget / 2) {
        memmove(tail, tail + tnl + 1, tail_budget - tnl - 1);
        tail[tail_budget - tnl - 1] = '\0';
    }
    /* model_text = head + "\n\n[... middle omitted ...]\n\n" + tail */
    const char *MID = "\n\n[... middle omitted ...]\n\n";
    size_t total = strlen(head) + strlen(MID) + strlen(tail) + 1;
    char *out = malloc(total);
    snprintf(out, total, "%s%s%s", head, MID, tail);
    free(head);
    free(tail);
    return out;
}
