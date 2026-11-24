/*
 * delegate_tool_progress.c - C home for upstream Hermes tools/delegate_tool_progress.py.
 * PoP: w16_delegateto_dd6882_normalize_event @ tools/delegate_tool_progress.py:_normalize_event
 * PoP: w16_delegateto_dd6882_format_duration @ tools/delegate_tool_progress.py:_format_duration
 * PoP: w16_delegateto_dd6882_short @ tools/delegate_tool_progress.py:_short
 * PoP: w16_delegateto_dd6882_child_task_message @ tools/delegate_tool_progress.py:child_task_message
 * PoP: w16_delegateto_dd6882_child_system_prompt @ tools/delegate_tool_progress.py:_build_child_system_prompt
 *
 * Pure formatting/normalisation helpers. Context-file discovery and display callbacks stay in
 * Python; the discovered context-file text is injected as `context_files` so this port stays
 * deterministic on any host.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_delegateto_dd6882.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *strip(const char *s) {
    if (!s) return dupstr("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = 0; }
    return p;
}
typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) { size_t nc = b->cap ? b->cap * 2 : 128; char *np = (char *)realloc(b->p, nc); if (!np) return; b->p = np; b->cap = nc; }
    b->p[b->n++] = c;
}
static void bputs(buf_t *b, const char *s) { for (const char *q = s; q && *q; q++) bput(b, *q); }
static char *bdone(buf_t *b) { if (!b->p) b->p = (char *)calloc(1, 1); if (b->p) b->p[b->n] = 0; return b->p; }
static long long pyround(double x) {
    double f = floor(x), frac = x - f;
    long long b = (long long)f;
    if (frac > 0.5) return b + 1;
    if (frac < 0.5) return b;
    return (b % 2 == 0) ? b : b + 1;
}

static char *dupn_local(const char *s, size_t n) { char *p = (char *)malloc(n + 1); if (p) { if (n) memcpy(p, s, n); p[n] = 0; } return p; }

static const char *const _CONTEXT_FILES_INTRO = "\nThe workspace's project context files are reproduced below. Their conventions and invariants are binding for your work in this workspace.\n\n";
static const char *const _COMPLETION_INSTRUCTIONS = "\nComplete this task using the tools available to you. When finished, provide a clear, concise summary of:\n- What you did\n- What you found or accomplished\n- Any files you created or modified\n- Any issues encountered\n\nImportant workspace rule: Never assume a repository lives at /workspace/... or any other container-style path unless the task/context explicitly gives that path. If no exact local path is provided, discover it first before issuing git/workdir-specific commands.\n\nKeep your final summary tight: lead with outcomes, prefer bullet points over paragraphs, and don't replay your whole process. Your response is returned to the parent agent as a summary, and overlong summaries crowd out the parent's context window.";
static const char *const _ORCHESTRATOR_BLOCK = "\n## Subagent Spawning (Orchestrator Role)\nYou have access to the `delegate_task` tool and CAN spawn your own subagents to parallelize independent work.\n\nWHEN to delegate:\n- The goal decomposes into 2+ independent subtasks that can run in parallel (e.g. research A and B simultaneously).\n- A subtask is reasoning-heavy and would flood your context with intermediate data.\n\nWHEN NOT to delegate:\n- Single-step mechanical work — do it directly.\n- Trivial tasks you can execute in one or two tool calls.\n- Re-delegating your entire assigned goal to one worker (that's just pass-through with no value added).\n\nCoordinate your workers' results and synthesize them before reporting back to your parent. You are responsible for the final summary, not your workers.\n\n";
static const char *const _LEAF_CHILDREN_NOTE = "Your own children MUST be leaves (cannot delegate further) because they will reach the maximum nesting depth. The system manages this automatically; no parameter override is available.";
static const char *const _NESTED_CHILDREN_NOTE = "Your own children can themselves delegate because depth remains. The system determines this automatically from the nesting depth; children at the maximum depth cannot delegate further. No parameter override is available.";

static const char *const LIFECYCLE[] = {
    "subagent.spawn_requested", "subagent.start", "subagent.complete", "subagent.text"
};
static const char *const DELEGATE_EVENTS[] = {
    "delegate.task_progress", "delegate.task_thinking", "delegate.tool_started", "delegate.tool_completed"
};

struct json_t *w16_delegateto_dd6882_normalize_event(const char *event) {
    if (!event) return json_null();
    for (size_t i = 0; i < 4; i++) if (!strcmp(event, LIFECYCLE[i])) return json_string(event);
    static const char *const LEGACY_FROM[] = {"_thinking", "reasoning.available", "tool.started", "tool.completed", "subagent_progress"};
    static const char *const LEGACY_TO[] = {"delegate.task_thinking", "delegate.task_thinking", "delegate.tool_started", "delegate.tool_completed", "delegate.task_progress"};
    for (size_t i = 0; i < 5; i++) if (!strcmp(event, LEGACY_FROM[i])) return json_string(LEGACY_TO[i]);
    for (size_t i = 0; i < 4; i++) if (!strcmp(event, DELEGATE_EVENTS[i])) return json_string(event);
    return json_null();
}

struct json_t *w16_delegateto_dd6882_format_duration(const struct json_t *seconds) {
    if (!seconds || seconds->type != JSON_NUMBER || seconds->num_val <= 0) return json_string("");
    double s = seconds->num_val;
    char b[64];
    if (s >= 120) snprintf(b, sizeof(b), "%lld min", pyround(s / 60.0));
    else snprintf(b, sizeof(b), "%llds", pyround(s));
    return json_string(b);
}

struct json_t *w16_delegateto_dd6882_short(const char *text, long long n) {
    if (!text) text = "";
    size_t len = strlen(text);
    if (n < 0) n = 0;
    if (len > (size_t)n) {
        char *p = dupn_local(text, (size_t)n);
        buf_t b = {0}; bputs(&b, p ? p : ""); bputs(&b, "...");
        free(p);
        return json_string(bdone(&b));
    }
    return json_string(text);
}

struct json_t *w16_delegateto_dd6882_child_task_message(const char *goal, const struct json_t *context) {
    if (!goal) goal = "";
    const char *ctx = (context && context->type == JSON_STRING && context->str_val) ? context->str_val : "";
    char *trimmed = strip(ctx);
    if (!trimmed[0]) { free(trimmed); return json_string(goal); }
    buf_t b = {0};
    bputs(&b, "CONTEXT:\n"); bputs(&b, trimmed); bputs(&b, "\n\nTASK:\n"); bputs(&b, goal);
    char *out = bdone(&b); free(trimmed);
    return json_string(out);
}

struct json_t *w16_delegateto_dd6882_child_system_prompt(const struct json_t *opts) {
    const char *ws = json_get_str(opts, "workspace_path", "");
    char *wstripped = strip(ws);
    const char *role = json_get_str(opts, "role", "leaf");
    long long max_depth = (long long)json_get_num(opts, "max_spawn_depth", 2);
    long long child_depth = (long long)json_get_num(opts, "child_depth", 1);
    const char *ctx_files_raw = json_get_str(opts, "context_files", "");
    char *ctx_files = strip(ctx_files_raw);
    buf_t b = {0};
    bputs(&b, "You are a focused subagent working on a specific delegated task.");
    if (wstripped[0]) {
        bputs(&b, "\n\nWORKSPACE PATH:\n"); bputs(&b, ws);
        bputs(&b, "\nUse this exact path for local repository/workdir operations unless the task explicitly says otherwise.");
        if (ctx_files[0]) { bputs(&b, "\n"); bputs(&b, _CONTEXT_FILES_INTRO); bputs(&b, ctx_files); }
    }
    bputs(&b, "\n"); bputs(&b, _COMPLETION_INSTRUCTIONS);
    if (!strcmp(role, "orchestrator")) {
        const char *note = (child_depth + 1 >= max_depth) ? _LEAF_CHILDREN_NOTE : _NESTED_CHILDREN_NOTE;
        char line[256];
        snprintf(line, sizeof(line), "NOTE: You are at depth %lld. The delegation tree is capped at max_spawn_depth=%lld. ", child_depth, max_depth);
        bputs(&b, "\n"); bputs(&b, _ORCHESTRATOR_BLOCK); bputs(&b, line); bputs(&b, note);
    }
    char *out = bdone(&b);
    free(wstripped); free(ctx_files);
    return json_string(out);
}

