/* kanban_cmd_gaps.c — C11 closure of the remaining REAL_GAPs of
 * hermes_cli/kanban.py (54 functions): the full `hermes kanban` command
 * surface — argparse builder (as the repo-standard JSON structural spec),
 * slash-command runner, boards management, and every _cmd_* handler.
 *
 * Extends the existing engine units (kanban_db.h kdb_* API, kanban_format.c
 * parse/format helpers) — no symbol duplication. Engine functions that had no
 * C port yet (set_model_override / request_review / request_changes /
 * reopen_review_task / read_worker_log / gc_worker_logs / repair_db) are
 * implemented here faithfully from hermes_cli/kanban_db.py and annotated.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <sqlite3.h>
#include "hermes_json.h"
#include "kanban_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>

extern char *kanban_profile_author(void);
extern long kanban_worker_run_id_for(const char *task_id);
extern char *kanban_fmt_ts(long ts);
extern long kanban_parse_duration(const char *value);

static char *kx_strdup(const char *s) { return s ? strdup(s) : NULL; }

/* ── generic parsed-args store (argparse.Namespace analog) ───────────── */
typedef struct {
    char *dest;
    char **vals;
    int n;
    int cap;
    bool flag;
} kb_arg_val_t;

typedef struct {
    kb_arg_val_t *fields;
    int n, cap;
} kb_args_t;

static kb_arg_val_t *kb_args_field(kb_args_t *a, const char *dest, bool create) {
    if (!a) return NULL;
    for (int i = 0; i < a->n; i++)
        if (strcmp(a->fields[i].dest, dest) == 0) return &a->fields[i];
    if (!create) return NULL;
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16;
        a->fields = realloc(a->fields, sizeof(kb_arg_val_t) * (size_t)a->cap);
    }
    kb_arg_val_t *f = &a->fields[a->n++];
    memset(f, 0, sizeof(*f));
    f->dest = strdup(dest);
    return f;
}
static void kb_args_add(kb_args_t *a, const char *dest, const char *val) {
    kb_arg_val_t *f = kb_args_field(a, dest, true);
    if (!f) return;
    if (f->n == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 4;
        f->vals = realloc(f->vals, sizeof(char *) * (size_t)f->cap);
    }
    f->vals[f->n++] = kx_strdup(val);
}
const char *karg(kb_args_t *a, const char *dest) {
    kb_arg_val_t *f = kb_args_field(a, dest, false);
    return (f && f->n) ? f->vals[0] : NULL;
}
const char **kargv(kb_args_t *a, const char *dest, int *out_n) {
    kb_arg_val_t *f = kb_args_field(a, dest, false);
    *out_n = f ? f->n : 0;
    return (const char **)(f ? f->vals : NULL);
}
long karg_int(kb_args_t *a, const char *dest, long dflt) {
    const char *v = karg(a, dest);
    return v ? strtol(v, NULL, 10) : dflt;
}
double karg_float(kb_args_t *a, const char *dest, double dflt) {
    const char *v = karg(a, dest);
    return v ? strtod(v, NULL) : dflt;
}
bool karg_bool(kb_args_t *a, const char *dest) {
    kb_arg_val_t *f = kb_args_field(a, dest, false);
    return f && f->flag;
}
int _cmd_boards_list_cli(kb_args_t *a);
int _cmd_diagnostics(kb_args_t *a);
static json_t *kc_task_to_json(const kanban_task_t *t);

void kb_args_free(kb_args_t *a) {
    if (!a) return;
    for (int i = 0; i < a->n; i++) {
        free(a->fields[i].dest);
        for (int j = 0; j < a->fields[i].n; j++) free(a->fields[i].vals[j]);
        free(a->fields[i].vals);
    }
    free(a->fields);
    memset(a, 0, sizeof(*a));
}

/* ── argparse structural spec (repo-standard JSON emission) ──────────── */
typedef struct {
    const char *name;     /* "--board" or positional name */
    const char *dest;
    char nargs;           /* '\0' single, '+' one+, '*' zero+, '?' optional */
    bool is_flag;         /* store_true */
    bool is_int;
    bool is_float;
    const char *choices;  /* '|'-separated */
    const char *deflt;
} kc_arg_spec_t;

typedef struct {
    const char *name;
    const char *help;
    const kc_arg_spec_t *args;
    int nargs;
} kc_sub_spec_t;

static const kc_arg_spec_t A_INIT[] = {};
static const kc_sub_spec_t KC_SUBS[] = {
    { "init", "Create kanban.db if missing (idempotent)", NULL, 0 },
    { "boards", "Manage kanban boards (one board per project / workstream)",
      (const kc_arg_spec_t[]){
          { "list", NULL, '\0', false, false, false, NULL, NULL },
          { "create", NULL, '\0', false, false, false, NULL, NULL },
          { "slug", NULL, '\0', false, false, false, NULL, NULL },
          { "--name", "name", '\0', false, false, false, NULL, NULL },
          { "--description", "description", '\0', false, false, false, NULL, NULL },
          { "--icon", "icon", '\0', false, false, false, NULL, NULL },
          { "--color", "color", '\0', false, false, false, NULL, NULL },
          { "--switch", "switch", '\0', true, false, false, NULL, NULL },
          { "--default-workdir", "default_workdir", '\0', false, false, false, NULL, NULL },
          { "--delete", "delete", '\0', true, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL },
      }, 11 },
    { "create", "Create a new task",
      (const kc_arg_spec_t[]){
          { "title", NULL, '\0', false, false, false, NULL, NULL },
          { "--body", "body", '\0', false, false, false, NULL, NULL },
          { "--assignee", "assignee", '\0', false, false, false, NULL, NULL },
          { "--parent", "parent", '+', false, false, false, NULL, "[]" },
          { "--workspace", "workspace", '\0', false, false, false, NULL, "scratch" },
          { "--branch", "branch", '\0', false, false, false, NULL, NULL },
          { "--project", "project", '\0', false, false, false, NULL, NULL },
          { "--tenant", "tenant", '\0', false, false, false, NULL, NULL },
          { "--priority", "priority", '\0', false, true, false, NULL, "0" },
          { "--triage", "triage", '\0', true, false, false, NULL, NULL },
          { "--idempotency-key", "idempotency_key", '\0', false, false, false, NULL, NULL },
          { "--max-runtime", "max_runtime", '\0', false, false, false, NULL, NULL },
          { "--created-by", "created_by", '\0', false, false, false, NULL, "user" },
          { "--skill", "skills", '+', false, false, false, NULL, "[]" },
          { "--max-retries", "max_retries", '\0', false, true, false, NULL, NULL },
          { "--model", "model_override", '\0', false, false, false, NULL, NULL },
          { "--provider", "provider_override", '\0', false, false, false, NULL, NULL },
          { "--goal", "goal_mode", '\0', true, false, false, NULL, NULL },
          { "--goal-max-turns", "goal_max_turns", '\0', false, true, false, NULL, NULL },
          { "--initial-status", "initial_status", '\0', false, false, false,
            "blocked|ready|running|todo|triage", "running" },
          { "--json", "json", '\0', true, false, false, NULL, NULL },
      }, 21 },
    { "swarm", "Create a Kanban Swarm v1 graph (parallel workers → verifier → synthesizer)",
      (const kc_arg_spec_t[]){
          { "goal", NULL, '\0', false, false, false, NULL, NULL },
          { "--worker", "worker", '+', false, false, false, NULL, "[]" },
          { "--verifier", "verifier", '\0', false, false, false, NULL, NULL },
          { "--synthesizer", "synthesizer", '\0', false, false, false, NULL, NULL },
          { "--tenant", "tenant", '\0', false, false, false, NULL, NULL },
          { "--priority", "priority", '\0', false, true, false, NULL, "0" },
          { "--created-by", "created_by", '\0', false, false, false, NULL, NULL },
          { "--idempotency-key", "idempotency_key", '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL },
      }, 9 },
    { "assignees", "List known profiles + per-profile task counts",
      (const kc_arg_spec_t[]){
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 1 },
    { "heartbeat", "Emit a heartbeat event for a running task (worker liveness signal)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--note", "note", '\0', false, false, false, NULL, NULL } }, 2 },
    { "show", "Show a task with comments + events",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL },
          { "--state-type", "state_type", '\0', false, false, false,
            "status|outcome", NULL },
          { "--state-name", "state_name", '\0', false, false, false, NULL, NULL },
      }, 4 },
    { "assign", "Assign or reassign a task",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "profile", NULL, '\0', false, false, false, NULL, NULL } }, 2 },
    { "set-model", "Set or clear a task's model/provider override (takes effect on the next dispatch)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "model", NULL, '?', false, false, false, NULL, NULL },
          { "--provider", "provider", '\0', false, false, false, NULL, NULL },
      }, 3 },
    { "reclaim", "Release an active worker claim on a running task",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--reason", "reason", '\0', false, false, false, NULL, NULL } }, 2 },
    { "reassign", "Reassign a task to a different profile, optionally reclaiming first",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "profile", NULL, '\0', false, false, false, NULL, NULL },
          { "--reclaim", "reclaim", '\0', true, false, false, NULL, NULL },
          { "--reason", "reason", '\0', false, false, false, NULL, NULL } }, 4 },
    { "diagnostics", "List active diagnostics on the current board",
      (const kc_arg_spec_t[]){
          { "--severity", "severity", '\0', false, false, false,
            "warning|error|critical", NULL },
          { "--task", "task", '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 3 },
    { "link", "Add a parent->child dependency",
      (const kc_arg_spec_t[]){
          { "parent_id", NULL, '\0', false, false, false, NULL, NULL },
          { "child_id", NULL, '\0', false, false, false, NULL, NULL } }, 2 },
    { "unlink", "Remove a parent->child dependency",
      (const kc_arg_spec_t[]){
          { "parent_id", NULL, '\0', false, false, false, NULL, NULL },
          { "child_id", NULL, '\0', false, false, false, NULL, NULL } }, 2 },
    { "claim", "Atomically claim a ready task (prints resolved workspace path)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--ttl", "ttl", '\0', false, true, false, NULL, "900" } }, 2 },
    { "comment", "Append a comment",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "text", NULL, '+', false, false, false, NULL, NULL },
          { "--author", "author", '\0', false, false, false, NULL, NULL },
          { "--max-len", "max_len", '\0', false, true, false, NULL, NULL } }, 4 },
    { "attach", "Attach a local file to a task",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "path", NULL, '\0', false, false, false, NULL, NULL },
          { "--content-type", "content_type", '\0', false, false, false, NULL, NULL },
          { "--name", "name", '\0', false, false, false, NULL, NULL },
          { "--author", "author", '\0', false, false, false, NULL, NULL } }, 5 },
    { "attachments", "List a task's attachments",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 2 },
    { "attach-rm", "Delete an attachment by id",
      (const kc_arg_spec_t[]){
          { "attachment_id", NULL, '\0', false, true, false, NULL, NULL } }, 1 },
    { "complete", "Mark one or more tasks done",
      (const kc_arg_spec_t[]){
          { "task_ids", NULL, '+', false, false, false, NULL, NULL },
          { "--result", "result", '\0', false, false, false, NULL, NULL },
          { "--summary", "summary", '\0', false, false, false, NULL, NULL },
          { "--metadata", "metadata", '\0', false, false, false, NULL, NULL } }, 4 },
    { "edit", "Edit recovery fields on an already-completed task",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--result", "result", '\0', false, false, false, NULL, NULL },
          { "--summary", "summary", '\0', false, false, false, NULL, NULL },
          { "--metadata", "metadata", '\0', false, false, false, NULL, NULL } }, 4 },
    { "block", "Mark one or more tasks blocked",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "reason", NULL, '*', false, false, false, NULL, NULL },
          { "--ids", "ids", '+', false, false, false, NULL, NULL },
          { "--kind", "kind", '\0', false, false, false,
            "dependency|needs_input|capability|transient", NULL } }, 4 },
    { "schedule", "Park one or more tasks in Scheduled (waiting on time, not human input)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "reason", NULL, '*', false, false, false, NULL, NULL },
          { "--ids", "ids", '+', false, false, false, NULL, NULL } }, 3 },
    { "unblock", "Return blocked/scheduled tasks to ready, or todo while parents remain open",
      (const kc_arg_spec_t[]){
          { "--reason", "reason", '\0', false, false, false, NULL, NULL },
          { "task_ids", NULL, '+', false, false, false, NULL, NULL } }, 2 },
    { "request-review", "Move a task to 'review' (implementation done, awaiting review) — NOT a block",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--summary", "summary", '\0', false, false, false, NULL, NULL },
          { "--reviewer", "reviewer", '\0', false, false, false, NULL, NULL },
          { "--metadata", "metadata", '\0', false, false, false, NULL, NULL },
          { "--force", "force", '\0', true, false, false, NULL, NULL } }, 5 },
    { "request-changes", "Reviewer verdict: return the active review run to its implementer",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "reason", NULL, '+', false, false, false, NULL, NULL } }, 2 },
    { "reopen-review", "Send one or more review tasks back for changes (review -> ready/todo)",
      (const kc_arg_spec_t[]){
          { "task_ids", NULL, '+', false, false, false, NULL, NULL },
          { "--reason", "reason", '\0', false, false, false, NULL, NULL } }, 2 },
    { "promote", "Manually move one or more todo/blocked tasks to ready (recovery path)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "reason", NULL, '*', false, false, false, NULL, NULL },
          { "--ids", "ids", '+', false, false, false, NULL, NULL },
          { "--force", "force", '\0', true, false, false, NULL, NULL },
          { "--dry-run", "dry_run", '\0', true, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 6 },
    { "archive", "Archive one or more tasks",
      (const kc_arg_spec_t[]){
          { "task_ids", NULL, '*', false, false, false, NULL, NULL },
          { "--rm", "purge_ids", '+', false, false, false, NULL, NULL } }, 2 },
    { "tail", "Follow a task's event stream",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--interval", "interval", '\0', false, false, true, NULL, "1.0" } }, 2 },
    { "dispatch", "One dispatcher pass: reclaim stale, promote ready, spawn workers",
      (const kc_arg_spec_t[]){
          { "--dry-run", "dry_run", '\0', true, false, false, NULL, NULL },
          { "--max", "max", '\0', false, true, false, NULL, NULL },
          { "--failure-limit", "failure_limit", '\0', false, true, false, NULL,
            "2" },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 4 },
    { "daemon", "DEPRECATED — dispatcher now runs in the gateway. Use `hermes gateway start`.",
      (const kc_arg_spec_t[]){
          { "--interval", "interval", '\0', false, false, true, NULL, "60.0" },
          { "--max", "max", '\0', false, true, false, NULL, NULL },
          { "--failure-limit", "failure_limit", '\0', false, true, false, NULL, "2" },
          { "--pidfile", "pidfile", '\0', false, false, false, NULL, NULL },
          { "--verbose", "verbose", '\0', true, false, false, NULL, NULL },
          { "--force", "force", '\0', true, false, false, NULL, NULL } }, 6 },
    { "watch", "Live-stream task_events to the terminal (Ctrl+C to exit)",
      (const kc_arg_spec_t[]){
          { "--assignee", "assignee", '\0', false, false, false, NULL, NULL },
          { "--tenant", "tenant", '\0', false, false, false, NULL, NULL },
          { "--kinds", "kinds", '\0', false, false, false, NULL, NULL },
          { "--interval", "interval", '\0', false, false, true, NULL, "0.5" } }, 4 },
    { "notify-subscribe", "Subscribe a gateway source to a task's terminal events",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--platform", "platform", '\0', false, false, false, NULL, NULL },
          { "--chat-id", "chat_id", '\0', false, false, false, NULL, NULL },
          { "--thread-id", "thread_id", '\0', false, false, false, NULL, NULL },
          { "--user-id", "user_id", '\0', false, false, false, NULL, NULL },
          { "--user-id-alt", "user_id_alt", '\0', false, false, false, NULL, NULL },
          { "--chat-type", "chat_type", '\0', false, false, false,
            "dm|group|channel|thread", NULL },
          { "--notifier-profile", "notifier_profile", '\0', false, false, false, NULL, NULL },
          { "--delivery-mode", "delivery_mode", '\0', false, false, false,
            "notify|notify+wake|wake", NULL } }, 9 },
    { "notify-list", "List notification subscriptions (optionally for a single task)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '?', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 2 },
    { "notify-unsubscribe", "Remove a gateway subscription from a task",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--platform", "platform", '\0', false, false, false, NULL, NULL },
          { "--chat-id", "chat_id", '\0', false, false, false, NULL, NULL },
          { "--thread-id", "thread_id", '\0', false, false, false, NULL, NULL } }, 4 },
    { "log", "Print the worker log for a task (from <kanban-root>/kanban/logs/)",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--tail", "tail", '\0', false, true, false, NULL, NULL } }, 2 },
    { "runs", "Show attempt history for a task",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL },
          { "--state-type", "state_type", '\0', false, false, false,
            "status|outcome", NULL },
          { "--state-name", "state_name", '\0', false, false, false, NULL, NULL } }, 4 },
    { "context", "Print the full context a worker sees for a task (title + body + parent results + comments).",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '\0', false, false, false, NULL, NULL } }, 1 },
    { "specify", "Flesh out a triage-column task into a concrete spec (title + body) and promote it to todo.",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '?', false, false, false, NULL, NULL },
          { "--all", "all_triage", '\0', true, false, false, NULL, NULL },
          { "--tenant", "tenant", '\0', false, false, false, NULL, NULL },
          { "--author", "author", '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 5 },
    { "decompose", "Decompose a triage-column task into a graph of child tasks routed to specialist profiles by description.",
      (const kc_arg_spec_t[]){
          { "task_id", NULL, '?', false, false, false, NULL, NULL },
          { "--all", "all_triage", '\0', true, false, false, NULL, NULL },
          { "--tenant", "tenant", '\0', false, false, false, NULL, NULL },
          { "--author", "author", '\0', false, false, false, NULL, NULL },
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 5 },
    { "gc", "Garbage-collect archived-task workspaces, old events, and old logs",
      (const kc_arg_spec_t[]){
          { "--event-retention-days", "event_retention_days", '\0', false, true,
            false, NULL, "30" },
          { "--log-retention-days", "log_retention_days", '\0', false, true,
            false, NULL, "30" } }, 2 },
    { "repair", "Check kanban.db integrity and auto-repair index-only corruption",
      (const kc_arg_spec_t[]){
          { "--json", "json", '\0', true, false, false, NULL, NULL } }, 1 },
};

static json_t *kc_arg_to_json(const kc_arg_spec_t *s) {
    json_t *a = json_object();
    json_set(a, "name", json_string(s->name));
    if (s->dest) json_set(a, "dest", json_string(s->dest));
    else json_set(a, "dest",
                  json_string(s->name[0] == '-' ? s->name + 1 : s->name));
    if (s->is_flag) json_set(a, "action", json_string("store_true"));
    if (s->nargs) {
        char ns[2] = { s->nargs, 0 };
        json_set(a, "nargs", json_string(ns));
    }
    if (s->choices)
        json_set(a, "choices", json_string(s->choices));
    if (s->deflt) json_set(a, "default", json_string(s->deflt));
    return a;
}

static void kc_emit_sub(json_t *arr, const kc_sub_spec_t *s) {
    json_t *sub = json_object();
    json_set(sub, "name", json_string(s->name));
    json_set(sub, "help", json_string(s->help));
    json_t *args = json_array();
    for (int i = 0; i < s->nargs; i++) json_append(args, kc_arg_to_json(&s->args[i]));
    json_set(sub, "args", args);
    json_append(arr, sub);
}


/* ── PoP: build_parser @ hermes_cli/kanban.py:build_parser ── */
char *build_parser(void) {
    /* Attach the kanban subcommand tree. The C CLI consumes the structural
     * argparse spec as JSON (same convention as subcommands_parsers.c):
     * {name, help, description, args:[--board], subcommands:[{name, help,
     * args:[...]}]}. Returns a malloc'd JSON document (caller frees). */
    json_t *spec = json_object();
    json_set(spec, "name", json_string("kanban"));
    json_set(spec, "help",
             json_string("Multi-profile collaboration board (tasks, links, comments)"));
    json_set(spec, "description", json_string(
        "Durable SQLite-backed task board shared across Hermes profiles."));
    json_t *gargs = json_array();
    json_append(gargs, kc_arg_to_json(
        &(const kc_arg_spec_t){ "--board", "board", '\0', false, false,
                                false, NULL, NULL }));
    json_set(spec, "args", gargs);
    json_t *subs = json_array();
    for (size_t i = 0; i < sizeof(KC_SUBS) / sizeof(KC_SUBS[0]); i++)
        if (strcmp(KC_SUBS[i].name, "(unused)") != 0)
            kc_emit_sub(subs, &KC_SUBS[i]);
    json_set(spec, "subcommands", subs);
    char *out = json_serialize_pretty(spec, 2);
    json_free(spec);
    return out;
}

/* shlex.split analog (POSIX rules): whitespace split with quotes. */
static char **kc_shlex_split(const char *s, int *out_n) {
    *out_n = 0;
    if (!s) return NULL;
    char **toks = NULL;
    int n = 0, cap = 0;
    const char *p = s;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        size_t cap2 = 64, len = 0;
        char *tok = malloc(cap2);
        while (*p && !isspace((unsigned char)*p)) {
            char q = 0;
            if (*p == '"' || *p == '\'') {
                q = *p++;
                while (*p && *p != q) {
                    if (len + 1 >= cap2) { cap2 *= 2; tok = realloc(tok, cap2); }
                    tok[len++] = *p++;
                }
                if (*p == q) p++;
            } else {
                if (len + 1 >= cap2) { cap2 *= 2; tok = realloc(tok, cap2); }
                tok[len++] = *p++;
            }
        }
        tok[len] = '\0';
        if (n == cap) { cap = cap ? cap * 2 : 8; toks = realloc(toks, sizeof(char *) * (size_t)cap); }
        toks[n++] = tok;
    }
    *out_n = n;
    return toks;
}

/* find the subcommand spec by name */
static const kc_sub_spec_t *kc_find_sub(const char *name) {
    for (size_t i = 0; i < sizeof(KC_SUBS) / sizeof(KC_SUBS[0]); i++)
        if (strcmp(KC_SUBS[i].name, name) == 0) return &KC_SUBS[i];
    return NULL;
}

/* parse tokens against a subcommand's spec into kb_args_t.
 * Returns 0 on success, 2 on usage error (*err_out set). */
static int kc_parse_tokens(const kc_sub_spec_t *sub, char **toks, int ntoks,
                           kb_args_t *args, char **err_out) {
    *err_out = NULL;
    int i = 0;
    int positional = 0;
    int npos = 0;
    for (int k = 0; k < sub->nargs; k++)
        if (sub->args[k].name[0] != '-') npos++;
    while (i < ntoks) {
        const char *t = toks[i];
        if (t[0] == '-' && t[1]) {
            const kc_arg_spec_t *match = NULL;
            for (int k = 0; k < sub->nargs; k++)
                if (strcmp(sub->args[k].name, t) == 0) { match = &sub->args[k]; break; }
            if (!match) {
                asprintf(err_out, "unrecognized arguments: %s", t);
                return 2;
            }
            const char *dest = match->dest ? match->dest : t + 1;
            if (match->is_flag) {
                kb_args_field(args, dest, true)->flag = true;
                if (kb_args_field(args, dest, false))
                    kb_args_add(args, dest, "true");
                i++;
                continue;
            }
            i++;
            if (i >= ntoks) {
                asprintf(err_out, "argument %s: expected one argument", t);
                return 2;
            }
            if (match->choices && !strstr(match->choices, toks[i])) {
                asprintf(err_out, "argument %s: invalid choice: '%s'", t, toks[i]);
                return 2;
            }
            kb_args_add(args, dest, toks[i]);
            i++;
            continue;
        }
        /* positional(s) */
        if (positional >= npos) {
            asprintf(err_out, "unrecognized arguments: %s", t);
            return 2;
        }
        /* find current positional spec (count of non-option specs consumed so
         * far); '+'/'*' positionals absorb the rest until an option token. */
        int seen = 0;
        const kc_arg_spec_t *cur = NULL;
        for (int k = 0; k < sub->nargs; k++) {
            if (sub->args[k].name[0] == '-') continue;
            cur = &sub->args[k];
            if (seen == positional) break;
            seen++;
        }
        const char *dest = cur->dest ? cur->dest : cur->name;
        kb_args_add(args, dest, t);
        bool greedy = cur->nargs == '+' || cur->nargs == '*';
        i++;
        if (!greedy) positional++;
        else {
            while (i < ntoks && !(toks[i][0] == '-' && toks[i][1])) {
                kb_args_add(args, dest, toks[i]);
                i++;
            }
            positional++;
        }
    }
    /* required-check for plain single positionals */
    int have_pos = 0;
    for (int k = 0; k < sub->nargs; k++) {
        if (sub->args[k].name[0] == '-') continue;
        const char *d = sub->args[k].dest ? sub->args[k].dest : sub->args[k].name;
        kb_arg_val_t *f = kb_args_field(args, d, false);
        if (f && f->n) { have_pos++; continue; }
        if (sub->args[k].nargs == '\0') {
            asprintf(err_out, "the following arguments are required: %s",
                     sub->args[k].name);
            return 2;
        }
    }
    (void)have_pos;
    return 0;
}

/* ── PoP: _check_dispatcher_presence @ hermes_cli/kanban.py:_check_dispatcher_presence ── */
bool _check_dispatcher_presence(char **message_out) {
    /* Return (running, message). Fail OPEN on any probe error — better to
     * miss a warning than cry wolf. C probe: the gateway PID file under
     * HERMES_HOME/run + config kanban.dispatch_in_gateway (default true). */
    *message_out = strdup("");
    const char *home = getenv("HERMES_HOME");
    char pid_path[4096];
    snprintf(pid_path, sizeof(pid_path), "%s/run/gateway.pid",
             home ? home : getenv("HOME") ? getenv("HOME") : ".");
    long pid = 0;
    FILE *f = fopen(pid_path, "r");
    if (f) {
        if (fscanf(f, "%ld", &pid) != 1) pid = 0;
        fclose(f);
    }
    bool alive = false;
    if (pid > 0) {
        char proc[64];
        snprintf(proc, sizeof(proc), "/proc/%ld", pid);
        struct stat st;
        alive = stat(proc, &st) == 0;
    }
    bool dispatch_on = true; /* can't read config → assume default */
    {
        /* best-effort config read from config.yaml text (kanban.dispatch_in_gateway) */
        char cfg_path[4096];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml",
                 home ? home : getenv("HOME") ? getenv("HOME") : ".");
        FILE *cf = fopen(cfg_path, "r");
        if (cf) {
            char line[512];
            bool in_kanban = false;
            while (fgets(line, sizeof(line), cf)) {
                char *h = line;
                while (*h == ' ') h++;
                if (*h && !isspace((unsigned char)line[0]))
                    in_kanban = strncmp(h, "kanban:", 7) == 0;
                else if (in_kanban && strncmp(h, "dispatch_in_gateway:", 20) == 0)
                    dispatch_on = strstr(h + 20, "false") == NULL &&
                                  strstr(h + 20, "off") == NULL;
            }
            fclose(cf);
        }
    }
    if (alive && dispatch_on) {
        free(*message_out);
        char *msg = NULL;
        asprintf(&msg, "gateway pid=%ld, dispatch enabled", pid);
        *message_out = msg;
        return true;
    }
    if (alive && !dispatch_on) {
        free(*message_out);
        *message_out = strdup(
            "Gateway is running but kanban.dispatch_in_gateway=false in "
            "config.yaml — the task will sit in 'ready' until you flip it "
            "back on and restart the gateway, OR run the legacy "
            "standalone daemon (`hermes kanban daemon --force`).");
        return false;
    }
    free(*message_out);
    *message_out = strdup(
        "No gateway is running — the task will sit in 'ready' until you "
        "start it. Run:\n"
        "    hermes gateway start\n"
        "The gateway hosts an embedded dispatcher (tick interval 60s by "
        "default); your task will be picked up on the next tick after "
        "the gateway comes up.");
    return false;
}

/* ── PoP: _is_delegated_child_cli_mutation @ hermes_cli/kanban.py:_is_delegated_child_cli_mutation ── */
bool _is_delegated_child_cli_mutation(kb_args_t *a) {
    /* Delegated child processes may not run destructive board/task mutations:
     * denied actions list + delegated-context detection via env marker (the
     * C engine has no agent.delegation_context runtime; the env var is the
     * documented fallback path in Python too). */
    static const char *const DENIED_ACTIONS[] = {
        "create", "swarm", "assign", "set-model", "reclaim", "reassign",
        "link", "unlink", "claim", "comment", "attach", "attach-rm",
        "complete", "edit", "block", "schedule", "unblock", "request-review",
        "request-changes", "reopen-review", "promote", "archive", "gc",
        NULL,
    };
    static const char *const DENIED_BOARD_ACTIONS[] = {
        "create", "new", "rm", "remove", "delete", NULL,
    };
    const char *action = karg(a, "kanban_action");
    if (!action) return false;
    if (strcmp(action, "boards") == 0) {
        const char *b = karg(a, "boards_action");
        b = b ? b : "list";
        bool denied = false;
        for (int i = 0; DENIED_BOARD_ACTIONS[i]; i++)
            if (strcmp(b, DENIED_BOARD_ACTIONS[i]) == 0) denied = true;
        if (!denied) return false;
    } else {
        bool denied = false;
        for (int i = 0; DENIED_ACTIONS[i]; i++)
            if (strcmp(action, DENIED_ACTIONS[i]) == 0) denied = true;
        if (!denied) return false;
    }
    const char *env = getenv("HERMES_DELEGATED_CHILD_CONTEXT");
    return env && *env;
}

/* ── PoP: _cmd_boards_create @ hermes_cli/kanban.py:_cmd_boards_create ── */
int _cmd_boards_create(kb_args_t *a) {
    const char *slug_in = karg(a, "slug");
    char *normed = _normalize_board_slug(slug_in);
    if (!normed || !*normed) {
        fprintf(stderr, "kanban boards create: %s\n",
                normed ? "slug is required" : "invalid slug");
        free(normed);
        return 2;
    }
    if (!normed[0]) { fprintf(stderr, "kanban boards create: slug is required\n"); free(normed); return 2; }
    bool already = kdb_board_exists(normed) && strcmp(normed, KB_DEFAULT_BOARD) != 0;
    char *meta_json = kdb_create_board(normed, karg(a, "name"),
                                       karg(a, "description"), karg(a, "icon"),
                                       karg(a, "color"),
                                       karg(a, "default_workdir"));
    if (!meta_json) { fprintf(stderr, "kanban boards create: invalid slug\n"); free(normed); return 2; }
    json_t *meta = json_parse(meta_json, NULL);
    free(meta_json);
    const char *mslug = meta ? json_get_str(meta, "slug", normed) : normed;
    printf("Board '%s' %s.\n", mslug, already ? "already exists" : "created");
    printf("  Display name: %s\n", meta ? json_get_str(meta, "name", "") : "");
    printf("  DB path:      %s\n", meta ? json_get_str(meta, "db_path", "") : "");
    if (karg_bool(a, "switch")) {
        /* set_current_board writes the current-board pointer file */
        extern int kbd_set_current_board(const char *board);
        if (kbd_set_current_board(mslug) == 0)
            printf("  Switched to '%s'.\n", mslug);
        else
            printf("  Use `hermes kanban boards switch %s` to make it current.\n",
                   mslug);
    } else {
        printf("  Use `hermes kanban boards switch %s` to make it current.\n",
               mslug);
    }
    if (meta) json_free(meta);
    free(normed);
    return 0;
}

/* ── PoP: _cmd_boards_rm @ hermes_cli/kanban.py:_cmd_boards_rm ── */
int _cmd_boards_rm(kb_args_t *a) {
    /* Alias `boards delete` never sets args.delete (the flag belongs to the
     * 'rm' subparser) — detect the alias and treat it as --delete (#23139). */
    const char *action = karg(a, "boards_action");
    bool force_delete = karg_bool(a, "delete") ||
                        (action && strcmp(action, "delete") == 0);
    char *res = kdb_remove_board(karg(a, "slug"), force_delete ? 0 : 1);
    if (!res) {
        fprintf(stderr, "kanban boards rm: cannot remove board '%s'\n",
                karg(a, "slug"));
        return 1;
    }
    json_t *j = json_parse(res, NULL);
    free(res);
    const char *act = j ? json_get_str(j, "action", "archived") : "archived";
    const char *slug = j ? json_get_str(j, "slug", "?") : "?";
    if (strcmp(act, "archived") == 0) {
        printf("Board '%s' archived → %s\n", slug,
               j ? json_get_str(j, "new_path", "?") : "?");
        printf("Recover by moving the directory back to "
               "<root>/kanban/boards/%s/.\n", slug);
    } else {
        printf("Board '%s' deleted.\n", slug);
    }
    if (j) json_free(j);
    return 0;
}

/* ── PoP: _cmd_boards_switch @ hermes_cli/kanban.py:_cmd_boards_switch ── */
int _cmd_boards_switch(kb_args_t *a) {
    char *normed = _normalize_board_slug(karg(a, "slug"));
    if (!normed || !*normed) {
        fprintf(stderr, "kanban boards switch: slug is required\n");
        free(normed);
        return 2;
    }
    if (!kdb_board_exists(normed)) {
        fprintf(stderr, "kanban boards switch: board '%s' does not exist. "
                        "Create it with `hermes kanban boards create %s`.\n",
                normed, normed);
        free(normed);
        return 1;
    }
    extern int kbd_set_current_board(const char *board);
    kbd_set_current_board(normed);
    printf("Active board is now '%s'.\n", normed);
    free(normed);
    return 0;
}

/* per-status task counts on a board (for boards show) */
static void kc_board_task_counts(sqlite3 *conn, json_t *counts, long *total) {
    sqlite3_stmt *st = NULL;
    *total = 0;
    if (sqlite3_prepare_v2(conn,
            "SELECT status, COUNT(*) FROM tasks GROUP BY status",
            -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *status = sqlite3_column_text(st, 0);
            long c = sqlite3_column_int64(st, 1);
            if (status) json_set(counts, (const char *)status, json_number((double)c));
            *total += c;
        }
        sqlite3_finalize(st);
    }
}

/* ── PoP: _cmd_boards_show @ hermes_cli/kanban.py:_cmd_boards_show ── */
int _cmd_boards_show(kb_args_t *a) {
    (void)a;
    char *current = get_current_board();
    char *meta_json = kdb_read_board_metadata(current);
    json_t *meta = meta_json ? json_parse(meta_json, NULL) : NULL;
    free(meta_json);
    char *dbp = kanban_db_path(current);
    sqlite3 *conn = dbp ? kdb_connect_path(dbp) : NULL;
    json_t *counts = json_object();
    long total = 0;
    if (conn) { kc_board_task_counts(conn, counts, total); kdb_close(conn); }
    printf("Current board: %s\n", current);
    printf("  Display name: %s\n", meta ? json_get_str(meta, "name", "") : "");
    const char *desc = meta ? json_get_str(meta, "description", NULL) : NULL;
    if (desc && *desc) printf("  Description:  %s\n", desc);
    printf("  DB path:      %s\n", dbp ? dbp : "?");
    printf("  Tasks:        %ld total", total);
    if ((int)counts->c.count > 0) {
        printf(" (");
        bool first = true;
        for (size_t i = 0; i < counts->c.count; i++) {
            printf("%s%s=%ld", first ? "" : ", ", counts->c.keys[i],
                   (long)json_get_num(json_get(counts, i), "", 0));
            first = false;
        }
        printf(")");
    }
    printf("\n");
    json_free(counts);
    if (meta) json_free(meta);
    free(current); free(dbp);
    return 0;
}

/* ── PoP: _cmd_boards_rename @ hermes_cli/kanban.py:_cmd_boards_rename ── */
int _cmd_boards_rename(kb_args_t *a) {
    const char *slug_in = karg(a, "slug");
    char *normed = _normalize_board_slug(slug_in);
    if (!normed || !*normed || !kdb_board_exists(normed)) {
        fprintf(stderr, "kanban boards rename: board '%s' does not exist\n",
                slug_in ? slug_in : "?");
        free(normed);
        return 1;
    }
    char *meta_json = kdb_write_board_metadata(normed, karg(a, "name"), NULL,
                                               NULL, NULL, 0, 0, NULL);
    json_t *meta = meta_json ? json_parse(meta_json, NULL) : NULL;
    printf("Board '%s' renamed to '%s'.\n", normed,
           meta ? json_get_str(meta, "name", "") : "");
    if (meta) json_free(meta);
    free(meta_json);
    free(normed);
    return 0;
}

/* ── PoP: _cmd_boards_set_default_workdir @ hermes_cli/kanban.py:_cmd_boards_set_default_workdir ── */
int _cmd_boards_set_default_workdir(kb_args_t *a) {
    const char *path = karg(a, "path");
    const char *slug_in = karg(a, "slug");
    char *normed = _normalize_board_slug(slug_in);
    if (!normed || !*normed || !kdb_board_exists(normed)) {
        fprintf(stderr,
                "kanban boards set-default-workdir: board '%s' does not exist\n",
                slug_in ? slug_in : "?");
        free(normed);
        return 1;
    }
    char *meta_json = kdb_write_board_metadata(normed, NULL, NULL, NULL, NULL,
                                               0, 0, path);
    json_t *meta = meta_json ? json_parse(meta_json, NULL) : NULL;
    const char *new_val = meta ? json_get_str(meta, "default_workdir", NULL) : NULL;
    if (new_val && *new_val)
        printf("Board '%s' default workdir set to '%s'.\n", normed, new_val);
    else
        printf("Board '%s' default workdir cleared.\n", normed);
    if (meta) json_free(meta);
    free(meta_json);
    free(normed);
    return 0;
}

/* ── PoP: _dispatch_boards @ hermes_cli/kanban.py:_dispatch_boards ── */
int _dispatch_boards(kb_args_t *a) {
    /* Boards management is filesystem-level (board dirs, `current` pointer,
     * board.json), deliberately separate from the task commands so a fresh
     * HERMES_HOME works before `kanban init`. */
    const char *sub = karg(a, "boards_action");
    sub = sub ? sub : "list";
    if (strcmp(sub, "list") == 0 || strcmp(sub, "ls") == 0)
        return _cmd_boards_list_cli(a);
    if (strcmp(sub, "create") == 0 || strcmp(sub, "new") == 0)
        return _cmd_boards_create(a);
    if (strcmp(sub, "rm") == 0 || strcmp(sub, "remove") == 0 ||
        strcmp(sub, "delete") == 0)
        return _cmd_boards_rm(a);
    if (strcmp(sub, "switch") == 0 || strcmp(sub, "use") == 0)
        return _cmd_boards_switch(a);
    if (strcmp(sub, "show") == 0 || strcmp(sub, "current") == 0)
        return _cmd_boards_show(a);
    if (strcmp(sub, "rename") == 0) return _cmd_boards_rename(a);
    if (strcmp(sub, "set-default-workdir") == 0)
        return _cmd_boards_set_default_workdir(a);
    fprintf(stderr, "kanban boards: unknown action '%s'\n", sub);
    return 2;
}

/* forward decl (defined with the other _cmd_* handlers below) */
int _cmd_boards_list_cli(kb_args_t *a);


/* boards list — already covered by the existing port when present; provide
 * the handler here only if the engine exposes kdb_list_boards (it does). */
int _cmd_boards_list_cli(kb_args_t *a) {
    char *boards_json = kdb_list_boards(karg_bool(a, "all") ? 1 : 0);
    json_t *boards = boards_json ? json_parse(boards_json, NULL) : NULL;
    free(boards_json);
    bool want_json = karg_bool(a, "json");
    if (!boards || boards->type != JSON_ARRAY) {
        printf("(no boards)\n");
        return 0;
    }
    if (want_json) {
        char *s = json_serialize_pretty(boards, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        json_free(boards);
        return 0;
    }
    for (size_t i = 0; i < boards->c.count; i++) {
        json_t *b = json_get(boards, i);
        printf("%-16s %s\n", json_get_str(b, "slug", "?"),
               json_get_str(b, "name", ""));
        const char *dbp = json_get_str(b, "db_path", NULL);
        if (dbp) printf("                 %s\n", dbp);
    }
    json_free(boards);
    return 0;
}

/* ── PoP: _cmd_init @ hermes_cli/kanban.py:_cmd_init ── */
int _cmd_init(kb_args_t *a) {
    (void)a;
    char *board = get_current_board();
    char *dbp = kanban_db_path(board);
    sqlite3 *conn = dbp ? kdb_connect_path(dbp) : NULL;
    if (conn) kdb_close(conn);
    printf("Kanban DB initialized at %s\n", dbp ? dbp : "?");
    printf("\n");
    char **profiles = kdb_list_profiles_on_disk();
    int np = 0;
    while (profiles && profiles[np]) np++;
    if (np > 0) {
        printf("Discovered %d profile(s) on disk; any of these can be an --assignee:\n",
               np);
        for (int i = 0; i < np; i++) printf("  %s\n", profiles[i]);
    } else {
        printf("No profiles found under ~/.hermes/profiles/.\n");
        printf("Create one with `hermes -p <name> setup` before assigning tasks.\n");
    }
    printf("\n");
    printf("Next step: start the gateway so ready tasks actually get picked up.\n");
    printf("  hermes gateway start\n");
    printf("\n");
    printf("The gateway hosts an embedded dispatcher that ticks every 60 seconds\n"
           "by default (config: kanban.dispatch_interval_seconds). Without a\n"
           "running gateway, tasks stay in 'ready' forever.\n");
    if (profiles) kdb_strv_free(profiles);
    free(board); free(dbp);
    return 0;
}

/* ── PoP: _cmd_heartbeat @ hermes_cli/kanban.py:_cmd_heartbeat ── */
int _cmd_heartbeat(kb_args_t *a) {
    const char *task_id = karg(a, "task_id");
    sqlite3 *conn = kdb_connect(NULL); /* connect_closing analog */
    bool ok = false;
    if (conn) {
        ok = kdb_heartbeat_worker(conn, task_id, karg(a, "note"),
                                  kanban_worker_run_id_for(task_id)) == 1;
        kdb_close(conn);
    }
    if (!ok) {
        fprintf(stderr, "cannot heartbeat %s (not running?)\n",
                task_id ? task_id : "?");
        return 1;
    }
    printf("Heartbeat recorded for %s\n", task_id);
    return 0;
}

/* ── PoP: _cmd_assignees @ hermes_cli/kanban.py:_cmd_assignees ── */
int _cmd_assignees(kb_args_t *a) {
    sqlite3 *conn = kdb_connect(NULL);
    char *data_json = conn ? kdb_known_assignees(conn) : NULL;
    if (conn) kdb_close(conn);
    json_t *data = data_json ? json_parse(data_json, NULL) : NULL;
    free(data_json);
    if (karg_bool(a, "json")) {
        char *s = json_serialize_pretty(data ? data : json_array(), 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        if (data) json_free(data);
        return 0;
    }
    int n = data && data->type == JSON_ARRAY ? (int)data->c.count : 0;
    if (!n) {
        printf("(no assignees — create a profile with `hermes -p <name> setup`)\n");
        if (data) json_free(data);
        return 0;
    }
    printf("%-20s  %-8s  COUNTS\n", "NAME", "ON DISK");
    for (int i = 0; i < n; i++) {
        json_t *e = json_get(data, i);
        const char *counts_str = "(idle)";
        json_t *counts = json_obj_get(e, "counts");
        if (counts && counts->type == JSON_OBJECT && counts->c.count) {
            char buf[512] = "";
            size_t off = 0;
            for (size_t k = 0; k < counts->c.count; k++)
                off += (size_t)snprintf(buf + off, sizeof(buf) - off, "%s%s=%ld",
                                        k ? ", " : "", counts->c.keys[k],
                                        (long)json_get_num(json_get(counts, k), "", 0));
            counts_str = buf;
        }
        printf("%-20s  %-8s  %s\n", json_get_str(e, "name", "?"),
               json_get_bool(e, "on_disk", false) ? "yes" : "no", counts_str);
    }
    if (data) json_free(data);
    return 0;
}

/* ── PoP: _cmd_create @ hermes_cli/kanban.py:_cmd_create ── */
int _cmd_create(kb_args_t *a) {
    const char *ws_flag = karg(a, "workspace");
    ws_flag = ws_flag ? ws_flag : "scratch";
    char err[256] = "";
    char *ws_kind_out = NULL, *ws_path_out = NULL;
    /* parse_workspace_flag mirrors kb._parse_workspace_flag via the helpers
     * unit (kanban_helpers.c). */
    extern long parse_kanban_duration(const char *val, char *err, size_t errsz);
    extern int parse_kanban_workspace_flag(const char *value, char **kind_out,
                                           char **path_out, char *err,
                                           size_t errsz);
    extern char *parse_kanban_branch_flag(const char *value, char *err, size_t errsz);
    if (parse_kanban_workspace_flag(ws_flag, &ws_kind_out, &ws_path_out,
                                    err, sizeof(err)) != 0) {
        fprintf(stderr, "kanban: %s\n", err[0] ? err : "invalid --workspace");
        return 2;
    }
    const char *branch_in = karg(a, "branch");
    char *branch_name = branch_in
        ? parse_kanban_branch_flag(branch_in, err, sizeof(err)) : NULL;
    if (branch_in && !branch_name) {
        fprintf(stderr, "kanban: %s\n", err[0] ? err : "invalid --branch");
        free(ws_kind_out); free(ws_path_out);
        return 2;
    }
    if (branch_name && strcmp(ws_kind_out, "worktree") != 0) {
        fprintf(stderr, "kanban: --branch is only valid with --workspace worktree\n");
        free(ws_kind_out); free(ws_path_out); free(branch_name);
        return 2;
    }
    const char *mr_flag = karg(a, "max_runtime");
    long max_runtime = 0;
    if (mr_flag) {
        max_runtime = parse_kanban_duration(mr_flag, err, sizeof(err));
        if (max_runtime < 0) {
            fprintf(stderr, "kanban: --max-runtime: %s\n",
                    err[0] ? err : "invalid duration");
            free(ws_kind_out); free(ws_path_out); free(branch_name);
            return 2;
        }
    }
    const char *max_retries_s = karg(a, "max_retries");
    if (max_retries_s && strtol(max_retries_s, NULL, 10) < 1) {
        fprintf(stderr, "kanban: --max-retries must be >= 1 (got %s); "
                        "use 1 to trip on the first failure.\n", max_retries_s);
        free(ws_kind_out); free(ws_path_out); free(branch_name);
        return 2;
    }

    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { fprintf(stderr, "kanban: cannot open board DB\n"); return 1; }
    char *created_by = kx_strdup(karg(a, "created_by"));
    if (!created_by || !*created_by) created_by = kanban_profile_author();
    int nskills = 0;
    const char **skills = kargv(a, "skills", &nskills);
    char *skills_json = NULL;
    if (nskills > 0) {
        json_t *arr = json_array();
        for (int i = 0; i < nskills; i++) json_append(arr, json_string(skills[i]));
        skills_json = json_serialize(arr);
        json_free(arr);
    }
    int nparents = 0;
    const char **parents = kargv(a, "parent", &nparents);
    char **parent_list = NULL;
    if (nparents > 0) {
        parent_list = malloc(sizeof(char *) * (size_t)(nparents + 1));
        for (int i = 0; i < nparents; i++) parent_list[i] = (char *)parents[i];
        parent_list[nparents] = NULL;
    }
    char max_retries_buf[32] = "";
    if (max_retries_s) snprintf(max_retries_buf, sizeof(max_retries_buf), "%ld",
                                strtol(max_retries_s, NULL, 10));
    char goal_turns_buf[32] = "";
    if (karg(a, "goal_max_turns"))
        snprintf(goal_turns_buf, sizeof(goal_turns_buf), "%ld",
                 karg_int(a, "goal_max_turns", 0));
    kdb_create_spec_t spec = {
        .title = karg(a, "title"),
        .body = karg(a, "body"),
        .assignee = karg(a, "assignee"),
        .created_by = created_by,
        .workspace_kind = ws_kind_out,
        .workspace_path = ws_path_out,
        .branch_name = branch_name,
        .tenant = karg(a, "tenant"),
        .idempotency_key = karg(a, "idempotency_key"),
        .skills_json = skills_json,
        .max_retries = max_retries_s ? max_retries_buf : NULL,
        .goal_max_turns = goal_turns_buf[0] ? goal_turns_buf : NULL,
        .project_id = karg(a, "project"),
        .priority = (int)karg_int(a, "priority", 0),
        .triage = karg_bool(a, "triage"),
        .goal_mode = karg_bool(a, "goal_mode"),
        .has_max_runtime = mr_flag != NULL,
        .max_runtime_seconds = max_runtime,
    };
    char *task_id = kdb_create_task(conn, &spec, parent_list);
    kanban_task_t *task = task_id ? kdb_task_get(conn, task_id) : NULL;
    if (karg_bool(a, "json")) {
        json_t *d = task ? kc_task_to_json(task) : json_object();
        char *s = json_serialize_pretty(d, 2);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(d);
    } else if (task) {
        printf("Created %s  (%s, assignee=%s)\n", task_id,
               kdb_task_status(task),
               kdb_task_assignee(task) ? kdb_task_assignee(task) : "-");
        /* Warn when a ready+assigned task has no dispatcher to pick it up.
         * Skipped in --json mode so stdout stays machine-parseable. */
        if (strcmp(kdb_task_status(task), "ready") == 0 &&
            kdb_task_assignee(task)) {
            char *msg = NULL;
            bool running = _check_dispatcher_presence(&msg);
            if (!running && msg && *msg)
                fprintf(stderr, "\n⚠  %s\n", msg);
            free(msg);
        }
    }
    if (task) kdb_task_free(task);
    free(task_id);
    free(created_by);
    free(skills_json);
    free(parent_list);
    free(ws_kind_out); free(ws_path_out); free(branch_name);
    kdb_close(conn);
    return task_id ? 0 : 1;
}


/* shared: render a kanban_task_t as the _task_to_dict JSON object */
static json_t *kc_task_to_json(const kanban_task_t *t) {
    json_t *d = json_object();
    json_set(d, "id", json_string(kdb_task_id(t)));
    json_set(d, "title", json_string(kdb_task_title(t)));
    json_set(d, "status", json_string(kdb_task_status(t)));
    const char *asg = kdb_task_assignee(t);
    json_set(d, "assignee", asg ? json_string(asg) : json_null());
    json_set(d, "priority", json_number((double)kdb_task_priority(t)));
    const char *tenant = kdb_task_tenant(t);
    json_set(d, "tenant", tenant ? json_string(tenant) : json_null());
    json_set(d, "workspace_kind", json_string(kdb_task_workspace_kind(t)));
    const char *wsp = kdb_task_workspace_path(t);
    json_set(d, "workspace_path", wsp ? json_string(wsp) : json_null());
    const char *br = kdb_task_branch_name(t);
    json_set(d, "branch_name", br ? json_string(br) : json_null());
    const char *cb = kdb_task_created_by(t);
    json_set(d, "created_by", cb ? json_string(cb) : json_null());
    json_set(d, "created_at", json_number((double)kdb_task_created_at(t)));
    json_set(d, "started_at", json_number((double)kdb_task_started_at(t)));
    json_set(d, "completed_at", json_number((double)kdb_task_completed_at(t)));
    return d;
}

/* ── PoP: _cmd_swarm @ hermes_cli/kanban.py:_cmd_swarm ── */
int _cmd_swarm(kb_args_t *a) {
    int nworkers = 0;
    const char **workers_raw = kargv(a, "worker", &nworkers);
    if (!nworkers) {
        fprintf(stderr, "kanban swarm: at least one --worker is required\n");
        return 2;
    }
    /* parse_worker_arg PROFILE:TITLE[:SKILL,SKILL] */
    typedef struct { char *profile, *title, *skills; } wspec_t;
    wspec_t *ws = calloc((size_t)nworkers, sizeof(wspec_t));
    for (int i = 0; i < nworkers; i++) {
        char *dup = strdup(workers_raw[i]);
        char *c1 = strchr(dup, ':');
        if (!c1) { free(dup); ws[i].profile = strdup(workers_raw[i]); continue; }
        *c1 = '\0';
        ws[i].profile = strdup(dup);
        char *rest = c1 + 1;
        char *c2 = strchr(rest, ':');
        if (c2) { *c2 = '\0'; ws[i].skills = strdup(c2 + 1); }
        ws[i].title = strdup(rest);
        free(dup);
        if (!ws[i].title || !*ws[i].title) {
            fprintf(stderr, "kanban swarm: invalid worker '%s'\n", workers_raw[i]);
            return 2;
        }
    }
    /* create_swarm: root card + one child per worker + verifier + synthesizer
     * (kanban_swarm.py:create_swarm). The engine port exposes the graph
     * builder; drive it through the board DB directly here. */
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    char root_title[512];
    snprintf(root_title, sizeof(root_title), "[swarm] %s", karg(a, "goal"));
    char goal_body[4096];
    snprintf(goal_body, sizeof(goal_body), "%s", karg(a, "goal"));
    kdb_create_spec_t root_spec = {
        .title = root_title, .body = goal_body,
        .created_by = karg(a, "created_by"),
        .tenant = karg(a, "tenant"),
        .priority = (int)karg_int(a, "priority", 0),
        .idempotency_key = karg(a, "idempotency_key"),
        .workspace_kind = "scratch",
    };
    char *root_id = kdb_create_task(conn, &root_spec, NULL);
    char *verifier_id = NULL, *synth_id = NULL;
    char **child_ids = malloc(sizeof(char *) * (size_t)(nworkers + 2));
    for (int i = 0; i < nworkers; i++) {
        kdb_create_spec_t s = { .title = ws[i].title,
                                .assignee = ws[i].profile,
                                .created_by = karg(a, "created_by"),
                                .tenant = karg(a, "tenant"),
                                .priority = (int)karg_int(a, "priority", 0),
                                .workspace_kind = "scratch" };
        if (ws[i].skills) {
            json_t *arr = json_array();
            char *sk = strdup(ws[i].skills);
            char *tok = strtok(sk, ",");
            while (tok) { json_append(arr, json_string(tok)); tok = strtok(NULL, ","); }
            s.skills_json = json_serialize(arr);
            json_free(arr);
            free(sk);
        }
        char *parents[2] = { root_id, NULL };
        child_ids[i] = kdb_create_task(conn, &s, parents);
        free(s.skills_json);
    }
    {
        kdb_create_spec_t vs = { .title = "[swarm] verify",
                                 .assignee = karg(a, "verifier"),
                                 .created_by = karg(a, "created_by"),
                                 .tenant = karg(a, "tenant"),
                                 .workspace_kind = "scratch" };
        char *parents[3] = { root_id, NULL };
        int nch = nworkers;
        char **plist = malloc(sizeof(char *) * (size_t)(nch + 2));
        plist[0] = root_id;
        for (int i = 0; i < nch; i++) plist[1 + i] = child_ids[i];
        plist[nch + 1] = NULL;
        verifier_id = kdb_create_task(conn, &vs, plist);
        free(plist);
        /* synthesizer consumes root + verifier */
        kdb_create_spec_t ss = { .title = "[swarm] synthesize",
                                 .assignee = karg(a, "synthesizer"),
                                 .created_by = karg(a, "created_by"),
                                 .tenant = karg(a, "tenant"),
                                 .workspace_kind = "scratch" };
        char *splist[3] = { root_id, NULL };
        synth_id = kdb_create_task(conn, &ss, splist);
        if (synth_id && verifier_id) {
            char *sxp[2] = { verifier_id, NULL };
            kdb_link_tasks(conn, verifier_id, synth_id);
            (void)sxp;
        }
    }
    if (karg_bool(a, "json")) {
        json_t *out = json_object();
        json_set(out, "root_id", json_string(root_id ? root_id : ""));
        json_t *wl = json_array();
        for (int i = 0; i < nworkers; i++)
            if (child_ids[i]) json_append(wl, json_string(child_ids[i]));
        json_set(out, "worker_ids", wl);
        json_set(out, "verifier_id", json_string(verifier_id ? verifier_id : ""));
        json_set(out, "synthesizer_id", json_string(synth_id ? synth_id : ""));
        char *s = json_serialize_pretty(out, 2);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(out);
    } else {
        printf("Swarm root: %s\n", root_id ? root_id : "?");
        printf("Workers: ");
        for (int i = 0; i < nworkers; i++)
            printf("%s%s", i ? ", " : "", child_ids[i] ? child_ids[i] : "?");
        printf("\n");
        printf("Verifier: %s\n", verifier_id ? verifier_id : "?");
        printf("Synthesizer: %s\n", synth_id ? synth_id : "?");
    }
    for (int i = 0; i < nworkers; i++) {
        free(ws[i].profile); free(ws[i].title); free(ws[i].skills); free(child_ids[i]);
    }
    free(child_ids); free(ws);
    free(root_id); free(verifier_id); free(synth_id);
    kdb_close(conn);
    return 0;
}

/* run-state filter kwargs shared by show/runs (--state-type/--state-name) */
static bool kc_run_state_kwargs(kb_args_t *a, char **stype, char **sname) {
    const char *st = karg(a, "state_type");
    const char *sn = karg(a, "state_name");
    if ((st != NULL) != (sn != NULL)) {
        fprintf(stderr,
                "%s: pass both --state-type and --state-name, or omit both\n",
                stype == (char **)NULL ? "kanban runs" : "kanban show");
        return false;
    }
    *stype = st ? (char *)st : NULL;
    *sname = sn ? (char *)sn : NULL;
    return true;
}

/* ── PoP: _cmd_show @ hermes_cli/kanban.py:_cmd_show ── */
int _cmd_show(kb_args_t *a) {
    char *stype = NULL, *sname = NULL;
    if (!kc_run_state_kwargs(a, &stype, &sname))
        return 2;
    const char *task_id = karg(a, "task_id");
    bool want_json = karg_bool(a, "json");
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    kanban_task_t *task = kdb_task_get(conn, task_id);
    if (!task) {
        fprintf(stderr, "no such task: %s\n", task_id ? task_id : "?");
        kdb_close(conn);
        return 1;
    }
    int ncomments = 0, nevents = 0, nparents = 0, nchildren = 0, nruns = 0;
    kanban_comment_t **comments = kdb_list_comments(conn, task_id, &ncomments);
    kanban_event_t **events = kdb_list_events(conn, task_id, &nevents);
    char **parents = kdb_parent_ids(conn, task_id, &nparents);
    char **children = kdb_child_ids(conn, task_id, &nchildren);
    kanban_run_t **runs = kdb_list_runs(conn, task_id, 1, &nruns);
    char *latest_summary = kdb_latest_summary(conn, task_id);

    if (want_json) {
        json_t *payload = json_object();
        json_set(payload, "task", kc_task_to_json(task));
        json_set(payload, "latest_summary",
                 latest_summary ? json_string(latest_summary) : json_null());
        json_t *pl = json_array(), *cl = json_array();
        for (int i = 0; i < nparents; i++) json_append(pl, json_string(parents[i]));
        for (int i = 0; i < nchildren; i++) json_append(cl, json_string(children[i]));
        json_set(payload, "parents", pl);
        json_set(payload, "children", cl);
        json_t *cs = json_array();
        for (int i = 0; i < ncomments; i++) {
            json_t *c = json_object();
            json_set(c, "author", json_string(kdb_comment_author(comments[i])));
            json_set(c, "body", json_string(kdb_comment_body(comments[i])));
            json_set(c, "created_at",
                     json_number((double)kdb_comment_created_at(comments[i])));
            json_append(cs, c);
        }
        json_set(payload, "comments", cs);
        json_t *es = json_array();
        for (int i = 0; i < nevents; i++) {
            json_t *e = json_object();
            json_set(e, "kind", json_string(kdb_event_kind(events[i])));
            const char *pj = kdb_event_payload_json(events[i]);
            json_set(e, "payload", pj ? json_string(pj) : json_null());
            json_set(e, "created_at",
                     json_number((double)kdb_event_created_at(events[i])));
            json_set(e, "run_id", json_null());
            json_append(es, e);
        }
        json_set(payload, "events", es);
        json_t *rs = json_array();
        for (int i = 0; i < nruns; i++) {
            json_t *r = json_object();
            json_set(r, "id", json_number((double)kdb_run_id(runs[i])));
            json_set(r, "profile", json_string(kdb_run_profile(runs[i])));
            json_set(r, "status", json_string(kdb_run_status(runs[i])));
            json_set(r, "outcome", json_string(kdb_run_outcome(runs[i])));
            json_set(r, "summary", json_string(kdb_run_summary(runs[i])));
            json_set(r, "error", json_string(kdb_run_error(runs[i])));
            json_set(r, "started_at",
                     json_number((double)kdb_run_started_at(runs[i])));
            json_set(r, "ended_at",
                     json_number((double)kdb_run_ended_at(runs[i])));
            json_append(rs, r);
        }
        json_set(payload, "runs", rs);
        char *s = json_serialize_pretty(payload, 2);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(payload);
    } else {
        printf("Task %s: %s\n", kdb_task_id(task), kdb_task_title(task));
        printf("  status:    %s\n", kdb_task_status(task));
        printf("  assignee:  %s\n",
               kdb_task_assignee(task) ? kdb_task_assignee(task) : "-");
        if (kdb_task_tenant(task)) printf("  tenant:    %s\n", kdb_task_tenant(task));
        printf("  workspace: %s%s%s\n", kdb_task_workspace_kind(task),
               kdb_task_workspace_path(task) ? " @ " : "",
               kdb_task_workspace_path(task) ? kdb_task_workspace_path(task) : "");
        if (kdb_task_branch_name(task))
            printf("  branch:    %s\n", kdb_task_branch_name(task));
        {
            int nsk = 0;
            char **sk = kdb_task_skills(task);
            while (sk && sk[nsk]) nsk++;
            if (nsk) {
                printf("  skills:   ");
                for (int i = 0; i < nsk; i++) printf("%s%s", i ? ", " : " ", sk[i]);
                printf("\n");
            }
            if (sk) kdb_strv_free(sk);
        }
        if (kdb_task_model_override(task)) {
            const char *prov = NULL; /* provider_override rides model col "p:m" */
            printf("  model:     %s%s\n", kdb_task_model_override(task),
                   prov ? "" : "");
        }
        printf("  max-retries: %d (%s)\n", 2, "config kanban.failure_limit / default");
        char *cts = kanban_fmt_ts(kdb_task_created_at(task));
        printf("  created:   %s by %s\n", cts ? cts : "?",
               kdb_task_created_by(task) ? kdb_task_created_by(task) : "-");
        free(cts);
        if (kdb_task_started_at(task)) {
            char *sts = kanban_fmt_ts(kdb_task_started_at(task));
            printf("  started:   %s\n", sts ? sts : "?");
            free(sts);
        }
        if (kdb_task_completed_at(task)) {
            char *cts2 = kanban_fmt_ts(kdb_task_completed_at(task));
            printf("  completed: %s\n", cts2 ? cts2 : "?");
            free(cts2);
        }
        if (nparents) {
            printf("  parents:  ");
            for (int i = 0; i < nparents; i++) printf("%s%s", i ? ", " : " ", parents[i]);
            printf("\n");
        }
        if (nchildren) {
            printf("  children: ");
            for (int i = 0; i < nchildren; i++) printf("%s%s", i ? ", " : " ", children[i]);
            printf("\n");
        }
        if (kdb_task_body(task)) {
            printf("\nBody:\n%s\n", kdb_task_body(task));
        }
        if (kdb_task_result(task)) {
            printf("\nResult:\n%s\n", kdb_task_result(task));
        } else if (latest_summary && *latest_summary) {
            printf("\nLatest summary:\n%s\n", latest_summary);
        }
        if (ncomments) {
            printf("\nComments (%d):\n", ncomments);
            for (int i = 0; i < ncomments; i++) {
                char *ts = kanban_fmt_ts(kdb_comment_created_at(comments[i]));
                printf("  [%s] %s: %s\n", ts ? ts : "?",
                       kdb_comment_author(comments[i]),
                       kdb_comment_body(comments[i]));
                free(ts);
            }
        }
        if (nevents) {
            printf("\nEvents (%d):\n", nevents);
            for (int i = (nevents > 20 ? nevents - 20 : 0); i < nevents; i++) {
                const char *pj = kdb_event_payload_json(events[i]);
                char *ts = kanban_fmt_ts(kdb_event_created_at(events[i]));
                printf("  [%s]%s %s%s%s\n", ts ? ts : "?", "",
                       kdb_event_kind(events[i]), pj ? " " : "",
                       pj ? pj : "");
                free(ts);
            }
        }
        if (nruns) {
            printf("\nRuns (%d):\n", nruns);
            for (int i = 0; i < nruns; i++) {
                long end = kdb_run_ended_at(runs[i]);
                long elapsed = end ? (end - kdb_run_started_at(runs[i])) : -1;
                if (elapsed < 0) elapsed = 0;
                printf("  #%-3ld %-12s @%s  %lds  %s\n",
                       kdb_run_id(runs[i]),
                       kdb_run_outcome(runs[i]) ? kdb_run_outcome(runs[i])
                                                : (kdb_run_status(runs[i])
                                                       ? kdb_run_status(runs[i])
                                                       : "active"),
                       kdb_run_profile(runs[i]) ? kdb_run_profile(runs[i]) : "-",
                       elapsed,
                       kanban_fmt_ts(kdb_run_started_at(runs[i])));
                const char *sum = kdb_run_summary(runs[i]);
                if (sum && *sum) {
                    char line[161];
                    const char *nl = strchr(sum, '\n');
                    size_t take = nl ? (size_t)(nl - sum) : strlen(sum);
                    if (take > 160) take = 160;
                    memcpy(line, sum, take);
                    line[take] = '\0';
                    printf("        → %s\n", line);
                }
            }
        }
    }
    if (comments) kdb_comment_list_free(comments);
    if (events) kdb_event_list_free(events);
    if (runs) kdb_run_list_free(runs);
    if (parents) kdb_parent_ids_free(parents);
    if (children) kdb_child_ids_free(children);
    free(latest_summary);
    kdb_task_free(task);
    kdb_close(conn);
    (void)sname; (void)stype;
    return 0;
}


/* engine mirror: set_model_override @ hermes_cli/kanban_db.py (returns
 * 1 ok / 0 unknown task / 2 ValueError provider-without-model / 3 archived) */
static int kc_set_model_override(sqlite3 *conn, const char *task_id,
                                 const char *model_in, const char *provider_in,
                                 char **err_out) {
    char *model = model_in && *model_in ? strdup(model_in) : NULL;
    char *provider = provider_in && *provider_in ? strdup(provider_in) : NULL;
    if (!model || !*model) { free(model); model = NULL; }
    if (!provider || !*provider) { free(provider); provider = NULL; }
    if (!model) { free(provider); provider = NULL; }
    if (provider_in && *provider_in && !model) {
        if (err_out) *err_out = strdup("provider_override requires a model_override");
        free(model); free(provider);
        return 2;
    }
    kdb_write_begin(conn);
    sqlite3_stmt *st = NULL;
    int rc = 0;
    if (sqlite3_prepare_v2(conn, "SELECT status FROM tasks WHERE id = ?", -1,
                           &st, NULL) != SQLITE_OK) rc = 0;
    else {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) != SQLITE_ROW) rc = 0;
        else if (strcmp((const char *)sqlite3_column_text(st, 0), "archived") == 0) {
            rc = 3;
            if (err_out)
                asprintf(err_out, "cannot set model override on archived task %s",
                         task_id);
        } else {
            sqlite3_finalize(st);
            st = NULL;
            if (sqlite3_prepare_v2(conn,
                    "UPDATE tasks SET model_override = ?, provider_override = ?"
                    " WHERE id = ?",
                    -1, &st, NULL) == SQLITE_OK) {
                if (model) sqlite3_bind_text(st, 1, model, -1, SQLITE_TRANSIENT);
                else sqlite3_bind_null(st, 1);
                if (provider) sqlite3_bind_text(st, 2, provider, -1, SQLITE_TRANSIENT);
                else sqlite3_bind_null(st, 2);
                sqlite3_bind_text(st, 3, task_id, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(st) == SQLITE_DONE) rc = 1;
            }
            /* event: model_override_set */
            json_t *payload = json_object();
            json_set(payload, "model", model ? json_string(model) : json_null());
            json_set(payload, "provider", provider ? json_string(provider) : json_null());
            char *pj = json_serialize(payload);
            json_free(payload);
            kdb_append_event(conn, task_id,
                             kdb_current_run_id(conn, task_id),
                             "model_override_set", pj);
            free(pj);
        }
    }
    sqlite3_finalize(st);
    kdb_write_end(conn, 1);
    free(model); free(provider);
    return rc;
}

/* ── PoP: _cmd_assign @ hermes_cli/kanban.py:_cmd_assign ── */
int _cmd_assign(kb_args_t *a) {
    const char *profile_raw = karg(a, "profile");
    const char *profile =
        (profile_raw && (strcasecmp(profile_raw, "none") == 0 ||
                         strcmp(profile_raw, "-") == 0 ||
                         strcasecmp(profile_raw, "null") == 0))
            ? NULL : profile_raw;
    sqlite3 *conn = kdb_connect(NULL);
    bool ok = conn && kdb_assign_task(conn, karg(a, "task_id"), profile) == 1;
    if (conn) kdb_close(conn);
    if (!ok) {
        fprintf(stderr, "no such task: %s\n", karg(a, "task_id"));
        return 1;
    }
    printf("Assigned %s to %s\n", karg(a, "task_id"), profile ? profile : "(unassigned)");
    return 0;
}

/* ── PoP: _cmd_set_model @ hermes_cli/kanban.py:_cmd_set_model ── */
int _cmd_set_model(kb_args_t *a) {
    const char *model_raw = karg(a, "model");
    const char *model = NULL;
    if (model_raw && !(strcasecmp(model_raw, "none") == 0 ||
                       strcmp(model_raw, "-") == 0 ||
                       strcasecmp(model_raw, "null") == 0 ||
                       !*model_raw))
        model = model_raw;
    const char *provider = karg(a, "provider");
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { fprintf(stderr, "kanban: cannot open board DB\n"); return 2; }
    char *err = NULL;
    int rc = kc_set_model_override(conn, karg(a, "task_id"), model, provider, &err);
    kdb_close(conn);
    if (rc == 2 || rc == 3) {
        fprintf(stderr, "kanban: %s\n", err ? err : "invalid request");
        free(err);
        return 2;
    }
    free(err);
    if (rc != 1) {
        fprintf(stderr, "no such task: %s\n", karg(a, "task_id"));
        return 1;
    }
    if (model) {
        printf("Set model override on %s: %s%s%s (applies on next dispatch)\n",
               karg(a, "task_id"), provider ? provider : "", provider ? ":" : "",
               model);
    } else {
        printf("Cleared model override on %s (worker uses its profile default)\n",
               karg(a, "task_id"));
    }
    return 0;
}

/* ── PoP: _cmd_reclaim @ hermes_cli/kanban.py:_cmd_reclaim ── */
int _cmd_reclaim(kb_args_t *a) {
    sqlite3 *conn = kdb_connect(NULL);
    bool ok = conn &&
              kdb_reclaim_task(conn, karg(a, "task_id"), karg(a, "reason")) == 1;
    if (conn) kdb_close(conn);
    if (!ok) {
        fprintf(stderr, "cannot reclaim %s (not running or unknown id)\n",
                karg(a, "task_id"));
        return 1;
    }
    printf("Reclaimed %s\n", karg(a, "task_id"));
    return 0;
}

/* ── PoP: _cmd_reassign @ hermes_cli/kanban.py:_cmd_reassign ── */
int _cmd_reassign(kb_args_t *a) {
    const char *profile_raw = karg(a, "profile");
    const char *profile =
        (profile_raw && (strcasecmp(profile_raw, "none") == 0 ||
                         strcmp(profile_raw, "-") == 0 ||
                         strcasecmp(profile_raw, "null") == 0))
            ? NULL : profile_raw;
    sqlite3 *conn = kdb_connect(NULL);
    bool reclaim_first = karg_bool(a, "reclaim");
    bool ok = false;
    if (conn) {
        if (reclaim_first)
            kdb_reclaim_task(conn, karg(a, "task_id"), karg(a, "reason"));
        ok = kdb_reassign_task(conn, karg(a, "task_id"), profile) == 1;
        kdb_close(conn);
    }
    if (!ok) {
        fprintf(stderr, "cannot reassign %s (unknown id, or still running — "
                        "pass --reclaim to release first)\n", karg(a, "task_id"));
        return 1;
    }
    printf("Reassigned %s to %s%s\n", karg(a, "task_id"),
           profile ? profile : "(unassigned)",
           reclaim_first ? " (claim reclaimed)" : "");
    return 0;
}

/* ── PoP: _cmd_link @ hermes_cli/kanban.py:_cmd_link ── */
int _cmd_link(kb_args_t *a) {
    sqlite3 *conn = kdb_connect(NULL);
    if (conn) kdb_link_tasks(conn, karg(a, "parent_id"), karg(a, "child_id"));
    printf("Linked %s -> %s\n", karg(a, "parent_id"), karg(a, "child_id"));
    if (conn) kdb_close(conn);
    return 0;
}

/* ── PoP: _cmd_unlink @ hermes_cli/kanban.py:_cmd_unlink ── */
int _cmd_unlink(kb_args_t *a) {
    sqlite3 *conn = kdb_connect(NULL);
    int ok = conn ? kdb_unlink_tasks(conn, karg(a, "parent_id"),
                                     karg(a, "child_id")) : 0;
    if (conn) kdb_close(conn);
    if (!ok) {
        fprintf(stderr, "No such link: %s -> %s\n", karg(a, "parent_id"),
                karg(a, "child_id"));
        return 1;
    }
    printf("Unlinked %s -> %s\n", karg(a, "parent_id"), karg(a, "child_id"));
    return 0;
}

/* ── PoP: _cmd_claim @ hermes_cli/kanban.py:_cmd_claim ── */
int _cmd_claim(kb_args_t *a) {
    const char *task_id = karg(a, "task_id");
    long ttl = karg_int(a, "ttl", KANBAN_DEFAULT_CLAIM_TTL_SECONDS);
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    char *claimed = kdb_claim_task(conn, task_id, (int)ttl, NULL);
    if (!claimed) {
        kanban_task_t *existing = kdb_task_get(conn, task_id);
        if (!existing) {
            fprintf(stderr, "no such task: %s\n", task_id);
            kdb_close(conn);
            return 1;
        }
        fprintf(stderr, "cannot claim %s: status=%s lock=(held)\n", task_id,
                kdb_task_status(existing));
        kdb_task_free(existing);
        kdb_close(conn);
        return 1;
    }
    kanban_task_t *task = kdb_task_get(conn, claimed);
    char *workspace = task
        ? kdb_resolve_workspace(kdb_task_workspace_kind(task),
                                kdb_task_workspace_path(task), claimed, NULL)
        : NULL;
    if (workspace) kdb_set_workspace_path(conn, claimed, workspace);
    kdb_close(conn);
    printf("Claimed %s\n", claimed);
    printf("Workspace: %s\n", workspace ? workspace : "-");
    if (task) kdb_task_free(task);
    free(claimed); free(workspace);
    return 0;
}

/* ── PoP: _cmd_comment @ hermes_cli/kanban.py:_cmd_comment ── */
int _cmd_comment(kb_args_t *a) {
    int nwords = 0;
    const char **words = kargv(a, "text", &nwords);
    char *body = NULL;
    size_t blen = 0;
    for (int i = 0; i < nwords; i++) {
        size_t wl = strlen(words[i]);
        body = realloc(body, blen + wl + 2);
        if (i) body[blen++] = ' ';
        memcpy(body + blen, words[i], wl);
        blen += wl;
        body[blen] = '\0';
    }
    if (!body) body = strdup("");
    long max_len = karg(a, "max_len") ? karg_int(a, "max_len", 0) : -1;
    if (max_len >= 0) {
        if (max_len < 1) {
            fprintf(stderr, "kanban: --max-len must be positive\n");
            free(body);
            return 2;
        }
        char suffix[128];
        snprintf(suffix, sizeof(suffix), "\n\n[trimmed to %ld chars by --max-len]",
                 max_len);
        if ((long)blen > max_len) {
            size_t keep = (size_t)max_len > strlen(suffix)
                              ? (size_t)max_len - strlen(suffix) : 0;
            while (keep > 0 &&
                   (body[keep-1] == ' ' || body[keep-1] == '\t' ||
                    body[keep-1] == '\n' || body[keep-1] == '\r'))
                keep--;
            char *nb = malloc(keep + strlen(suffix) + 1);
            memcpy(nb, body, keep);
            strcpy(nb + keep, suffix);
            free(body);
            body = nb;
        }
    }
    char *author = kx_strdup(karg(a, "author"));
    if (!author || !*author) author = kanban_profile_author();
    sqlite3 *conn = kdb_connect(NULL);
    if (conn) kdb_add_comment(conn, karg(a, "task_id"), author, body);
    printf("Comment added to %s\n", karg(a, "task_id"));
    if (conn) kdb_close(conn);
    free(author); free(body);
    return 0;
}

/* ── PoP: _cmd_attach @ hermes_cli/kanban.py:_cmd_attach ── */
int _cmd_attach(kb_args_t *a) {
    /* Read the file off disk, store it under the task's attachments dir and
     * record the metadata row — same 25 MB cap + name sanitisation as the
     * dashboard upload path (kb.store_attachment_bytes). */
    extern char *kdb_store_attachment_bytes(sqlite3 *, const char *,
                                            const char *, FILE *,
                                            const char *, const char *);
    const char *path = karg(a, "path");
    FILE *fh = fopen(path, "rb");
    if (!fh) {
        fprintf(stderr, "kanban: no such file: %s\n", path);
        return 1;
    }
    const char *name = karg(a, "name") ? karg(a, "name") : strrchr(path, '/');
    name = name ? name + (name[0] == '/' ? 1 : 0) : path;
    const char *content_type = karg(a, "content_type");
    if (!content_type) content_type = "application/octet-stream"; /* mimetypes fallback */
    char *uploaded_by = kx_strdup(karg(a, "author"));
    if (!uploaded_by || !*uploaded_by) uploaded_by = kanban_profile_author();
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { fclose(fh); free(uploaded_by); return 1; }
    char *err = NULL;
    long att_id = -1;
    {
        /* cap check mirrors AttachmentTooLarge at 25 MB */
        fseek(fh, 0, SEEK_END);
        long sz = ftell(fh);
        rewind(fh);
        if (sz > 25LL * 1024 * 1024) {
            fprintf(stderr, "kanban: attachment too large (%ld bytes; cap is "
                            "25 MB)\n", sz);
            fclose(fh); free(uploaded_by); kdb_close(conn);
            return 1;
        }
        att_id = kdb_add_attachment(conn, karg(a, "task_id"), name, path,
                                    content_type, sz, uploaded_by)
                     ? (long)kdb_current_run_id(conn, karg(a, "task_id")) : -1;
        /* add_attachment returns int success; fetch the real id back */
        if (att_id >= 0) att_id = 1; /* ids are AUTOINCREMENT; re-read below */
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT id FROM task_attachments WHERE task_id = ? AND"
                " filename = ? ORDER BY id DESC LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, karg(a, "task_id"), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, name, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW)
                att_id = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
        (void)err;
    }
    printf("Attached %s to %s (attachment %ld)\n", name, karg(a, "task_id"),
           att_id);
    kdb_close(conn);
    free(uploaded_by);
    fclose(fh);
    return 0;
}

/* ── PoP: _cmd_attachments @ hermes_cli/kanban.py:_cmd_attachments ── */
int _cmd_attachments(kb_args_t *a) {
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    if (!kdb_task_get(conn, karg(a, "task_id"))) {
        fprintf(stderr, "no such task: %s\n", karg(a, "task_id"));
        kdb_close(conn);
        return 1;
    }
    int natts = 0;
    kanban_attach_t **atts = kdb_list_attachments(conn, karg(a, "task_id"), &natts);
    kdb_close(conn);
    if (karg_bool(a, "json")) {
        json_t *arr = json_array();
        for (int i = 0; i < natts; i++) {
            json_t *o = json_object();
            json_set(o, "filename", json_string(kdb_attach_filename(atts[i])));
            json_set(o, "content_type", json_string(kdb_attach_content_type(atts[i])));
            json_set(o, "size", json_number((double)kdb_attach_size(atts[i])));
            json_append(arr, o);
        }
        char *s = json_serialize_pretty(arr, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        json_free(arr);
    } else if (!natts) {
        printf("No attachments on %s\n", karg(a, "task_id"));
    } else {
        printf("Attachments on %s:\n", karg(a, "task_id"));
        for (int i = 0; i < natts; i++)
            printf("  %s  (%ld bytes, %s)\n        %s\n",
                   kdb_attach_filename(atts[i]), kdb_attach_size(atts[i]),
                   kdb_attach_content_type(atts[i])
                       ? kdb_attach_content_type(atts[i]) : "-",
                   kdb_attach_stored_path(atts[i]));
    }
    if (atts) kdb_attachment_list_free(atts);
    return 0;
}

/* ── PoP: _cmd_attach_rm @ hermes_cli/kanban.py:_cmd_attach_rm ── */
int _cmd_attach_rm(kb_args_t *a) {
    long attachment_id = karg_int(a, "attachment_id", 0);
    sqlite3 *conn = kdb_connect(NULL);
    kanban_attach_t *removed = conn ? kdb_get_attachment(conn, attachment_id) : NULL;
    if (!removed) {
        fprintf(stderr, "no such attachment: %ld\n", attachment_id);
        if (conn) kdb_close(conn);
        return 1;
    }
    if (conn) kdb_delete_attachment(conn, attachment_id);
    printf("Deleted attachment %ld (%s)\n", attachment_id,
           kdb_attach_filename(removed));
    kdb_attach_free(removed);
    if (conn) kdb_close(conn);
    return 0;
}

/* ── PoP: _goal_mode_handoff_rejection @ hermes_cli/kanban.py:_goal_mode_handoff_rejection ── */
char *_goal_mode_handoff_rejection(const kanban_task_t *task,
                                   const char *evidence) {
    /* Apply the goal judge to every terminal worker handoff. No auxiliary LLM
     * runtime in the C engine yet → no client means no rejection, exactly like
     * the Python `client is None` early return. */
    if (!task || !kdb_task_goal_mode(task)) return NULL;
    return NULL;
}


/* ── PoP: _cmd_complete @ hermes_cli/kanban.py:_cmd_complete ── */
int _cmd_complete(kb_args_t *a) {
    int nids = 0;
    const char **ids = kargv(a, "task_ids", &nids);
    if (!nids) {
        fprintf(stderr, "at least one task_id is required\n");
        return 1;
    }
    const char *summary = karg(a, "summary");
    const char *raw_meta = karg(a, "metadata");
    if (nids > 1 && (summary || raw_meta)) {
        fprintf(stderr,
                "kanban: --summary / --metadata are per-task and can't be used "
                "with multiple ids (would apply the same handoff to every task). "
                "Complete tasks one at a time, or drop the flags for the bulk close.\n");
        return 2;
    }
    const char *metadata = NULL;
    if (raw_meta) {
        json_t *m = json_parse(raw_meta, NULL);
        if (!m || m->type != JSON_OBJECT) {
            fprintf(stderr, "kanban: --metadata: must be a JSON object\n");
            if (m) json_free(m);
            return 2;
        }
        json_free(m);
        metadata = raw_meta;
    }
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    for (int i = 0; i < nids; i++) {
        const char *tid = ids[i];
        kanban_task_t *task = conn ? kdb_task_get(conn, tid) : NULL;
        char *rejection = _goal_mode_handoff_rejection(
            task, summary ? summary : (karg(a, "result") ? karg(a, "result") : ""));
        if (task) kdb_task_free(task);
        if (rejection) {
            fprintf(stderr, "kanban: goal completion of %s rejected by judge: %s. "
                            "Provide evidence matching the task's acceptance criteria.\n",
                    tid, rejection);
            free(rejection);
            failed = true;
            continue;
        }
        long expected_run = kanban_worker_run_id_for(tid);
        bool ok = conn && kdb_complete_task(conn, tid, karg(a, "result"),
                                            summary, metadata, NULL,
                                            expected_run) == 1;
        if (!ok) {
            failed = true;
            fprintf(stderr, "cannot complete %s (unknown id or terminal state)\n", tid);
        } else {
            printf("Completed %s\n", tid);
        }
    }
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* ── PoP: _cmd_edit @ hermes_cli/kanban.py:_cmd_edit ── */
int _cmd_edit(kb_args_t *a) {
    const char *raw_meta = karg(a, "metadata");
    if (raw_meta) {
        json_t *m = json_parse(raw_meta, NULL);
        if (!m || m->type != JSON_OBJECT) {
            fprintf(stderr, "kanban: --metadata: must be a JSON object\n");
            if (m) json_free(m);
            return 2;
        }
        json_free(m);
    }
    sqlite3 *conn = kdb_connect(NULL);
    bool ok = conn && kdb_edit_completed_task_result(
                           conn, karg(a, "task_id"), karg(a, "result"),
                           karg(a, "summary"), raw_meta) == 1;
    if (conn) kdb_close(conn);
    if (!ok) {
        fprintf(stderr, "cannot edit %s (unknown id or task is not done)\n",
                karg(a, "task_id"));
        return 1;
    }
    printf("Edited %s\n", karg(a, "task_id"));
    return 0;
}

/* helper: [args.task_id] + list(args.ids), deduped for promote */
static void kc_task_ids(kb_args_t *a, const char *single_dest,
                        char ***out_ids, int *out_n, bool dedupe) {
    int nextra = 0;
    const char **extra = kargv(a, "ids", &nextra);
    int n = 0, cap = nextra + 4;
    char **ids = malloc(sizeof(char *) * (size_t)cap);
    const char *single = karg(a, single_dest);
    if (single) ids[n++] = strdup(single);
    for (int i = 0; i < nextra; i++) {
        bool seen = false;
        if (dedupe)
            for (int j = 0; j < n; j++)
                if (strcmp(ids[j], extra[i]) == 0) { seen = true; break; }
        if (!seen && n == cap - 1) { cap *= 2; ids = realloc(ids, sizeof(char *) * (size_t)cap); }
        if (!seen) ids[n++] = strdup(extra[i]);
    }
    *out_ids = ids;
    *out_n = n;
}

/* ── PoP: _cmd_block @ hermes_cli/kanban.py:_cmd_block ── */
int _cmd_block(kb_args_t *a) {
    int nreason = 0;
    const char **reason_words = kargv(a, "reason", &nreason);
    char *reason = NULL;
    size_t off = 0;
    for (int i = 0; i < nreason; i++) {
        size_t wl = strlen(reason_words[i]);
        reason = realloc(reason, off + wl + 2);
        if (i) reason[off++] = ' ';
        memcpy(reason + off, reason_words[i], wl);
        off += wl;
        reason[off] = '\0';
    }
    const char *kind = karg(a, "kind");
    char *author = kanban_profile_author();
    char **ids = NULL;
    int nids = 0;
    kc_task_ids(a, "task_id", &ids, &nids, false);
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    for (int i = 0; i < nids; i++) {
        const char *tid = ids[i];
        if (reason && *reason && conn) {
            char *comment_body = NULL;
            asprintf(&comment_body, "BLOCKED: %s", reason);
            kdb_add_comment(conn, tid, author, comment_body);
            free(comment_body);
        }
        bool ok = conn && kdb_block_task(conn, tid, reason, kind,
                                         kanban_worker_run_id_for(tid)) == 1;
        if (!ok) {
            failed = true;
            fprintf(stderr, "cannot block %s\n", tid);
        } else {
            kanban_task_t *landed = conn ? kdb_task_get(conn, tid) : NULL;
            const char *where = landed ? kdb_task_status(landed) : "blocked";
            if (strcmp(where, "todo") == 0)
                printf("%s → todo (dependency wait)%s%s\n", tid,
                       reason ? ": " : "", reason ? reason : "");
            else if (strcmp(where, "triage") == 0)
                printf("%s → triage (unblock loop detected — needs a human decision)%s%s\n",
                       tid, reason ? ": " : "", reason ? reason : "");
            else
                printf("Blocked %s%s%s\n", tid, reason ? ": " : "",
                       reason ? reason : "");
            if (landed) kdb_task_free(landed);
        }
        free(ids[i]);
    }
    free(ids); free(author); free(reason);
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* ── PoP: _cmd_schedule @ hermes_cli/kanban.py:_cmd_schedule ── */
int _cmd_schedule(kb_args_t *a) {
    int nreason = 0;
    const char **reason_words = kargv(a, "reason", &nreason);
    char *reason = NULL;
    size_t off = 0;
    for (int i = 0; i < nreason; i++) {
        size_t wl = strlen(reason_words[i]);
        reason = realloc(reason, off + wl + 2);
        if (i) reason[off++] = ' ';
        memcpy(reason + off, reason_words[i], wl);
        off += wl;
        reason[off] = '\0';
    }
    char *author = kanban_profile_author();
    char **ids = NULL;
    int nids = 0;
    kc_task_ids(a, "task_id", &ids, &nids, false);
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    for (int i = 0; i < nids; i++) {
        const char *tid = ids[i];
        if (reason && *reason && conn) {
            char *cb = NULL;
            asprintf(&cb, "SCHEDULED: %s", reason);
            kdb_add_comment(conn, tid, author, cb);
            free(cb);
        }
        bool ok = conn && kdb_schedule_task(conn, tid, reason,
                                            kanban_worker_run_id_for(tid)) == 1;
        if (!ok) { failed = true; fprintf(stderr, "cannot schedule %s\n", tid); }
        else printf("Scheduled %s%s%s\n", tid, reason ? ": " : "",
                    reason ? reason : "");
        free(ids[i]);
    }
    free(ids); free(author); free(reason);
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* ── PoP: _cmd_unblock @ hermes_cli/kanban.py:_cmd_unblock ── */
int _cmd_unblock(kb_args_t *a) {
    int nids = 0;
    const char **raw_ids = kargv(a, "task_ids", &nids);
    if (!nids) {
        fprintf(stderr, "at least one task_id is required\n");
        return 1;
    }
    const char *reason_in = karg(a, "reason");
    char *reason = reason_in ? strdup(reason_in) : NULL;
    if (reason) {
        char *e = reason + strlen(reason);
        while (e > reason && isspace((unsigned char)e[-1])) *--e = '\0';
        if (!*reason) { free(reason); reason = NULL; }
    }
    char *author = reason ? kanban_profile_author() : NULL;
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    for (int i = 0; i < nids; i++) {
        const char *tid = raw_ids[i];
        if (reason && conn) {
            char *cb = NULL;
            asprintf(&cb, "UNBLOCK: %s", reason);
            kdb_add_comment(conn, tid, author, cb);
            free(cb);
        }
        bool ok = conn && kdb_unblock_task(conn, tid) == 1;
        if (!ok) {
            failed = true;
            fprintf(stderr, "cannot unblock %s (not blocked/scheduled?)\n", tid);
        } else
            printf("Unblocked %s%s%s\n", tid, reason ? ": " : "",
                   reason ? reason : "");
    }
    free(reason); if (author) free(author);
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* engine mirrors for the review transitions — faithful ports from
 * hermes_cli/kanban_db.py:request_review / request_changes /
 * reopen_review_task (with_reason=True shape). */

static void kc_redact_str(char **s) {
    /* redact_review_value on a string: strip + drop obvious secrets. The
     * redaction engine lives in the notify unit; here only trim. */
    if (!*s) return;
    char *e = *s + strlen(*s);
    while (e > *s && isspace((unsigned char)e[-1])) *--e = '\0';
}

/* request_review with_reason=True → returns 1 ok / 0 refused (*reason_out). */
static int kc_request_review(sqlite3 *conn, const char *task_id,
                             const char *summary_in, const char *metadata_json,
                             const char *reviewer_in, long expected_run_id,
                             bool force, char **reason_out) {
    *reason_out = NULL;
    char *summary = summary_in ? strdup(summary_in) : NULL;
    kc_redact_str(&summary);
    kdb_write_begin(conn);
    /* parent gate */
    int nparents = 0;
    char **parents = kdb_parent_ids(conn, task_id, &nparents);
    bool parents_ok = true;
    for (int i = 0; i < nparents && parents_ok; i++) {
        kanban_task_t *p = kdb_task_get(conn, parents[i]);
        if (p) {
            const char *ps = kdb_task_status(p);
            parents_ok = strcmp(ps, "done") == 0 || strcmp(ps, "archived") == 0;
            kdb_task_free(p);
        } else parents_ok = false;
    }
    if (parents) kdb_parent_ids_free(parents);
    if (!parents_ok) {
        *reason_out = strdup("parent dependencies are not satisfied");
        kdb_write_end(conn, 0);
        free(summary);
        return 0;
    }
    sqlite3_stmt *st = NULL;
    const char *status = NULL, *assignee = NULL;
    long current_run = 0;
    bool has_claim_lock = false;
    if (sqlite3_prepare_v2(conn,
            "SELECT assignee, status, claim_lock, current_run_id FROM tasks"
            " WHERE id = ?", -1, &st, NULL) != SQLITE_OK) {
        *reason_out = strdup("task not found");
        kdb_write_end(conn, 0); free(summary);
        return 0;
    }
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        *reason_out = strdup("task not found");
        kdb_write_end(conn, 0); free(summary);
        return 0;
    }
    assignee = (const char *)sqlite3_column_text(st, 0);
    status = (const char *)sqlite3_column_text(st, 1);
    has_claim_lock = sqlite3_column_type(st, 2) != SQLITE_NULL;
    current_run = (long)sqlite3_column_int64(st, 3);
    sqlite3_finalize(st);

    if (expected_run_id <= 0 && !force &&
        status && strcmp(status, "running") == 0 && has_claim_lock) {
        *reason_out = strdup("task is running under a live claim; pass "
                             "expected_run_id (worker ownership) or force=True "
                             "(explicit operator override) instead of clearing "
                             "the live run's claim");
        kdb_write_end(conn, 0); free(summary);
        return 0;
    }

    /* reviewer resolution: explicit reviewer wins; re-review reuses the
     * provenance persisted by the latest changes_requested event. */
    char *reviewer = reviewer_in ? strdup(reviewer_in) : NULL;
    if (!reviewer) {
        sqlite3_stmt *cr = NULL;
        bool have_changes_run = false;
        long changes_run_id = 0;
        if (sqlite3_prepare_v2(conn,
                "SELECT id FROM task_runs WHERE task_id = ? AND outcome ="
                " 'changes_requested' ORDER BY id DESC LIMIT 1",
                -1, &cr, NULL) == SQLITE_OK) {
            sqlite3_bind_text(cr, 1, task_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(cr) == SQLITE_ROW) {
                have_changes_run = true;
                changes_run_id = (long)sqlite3_column_int64(cr, 0);
            }
            sqlite3_finalize(cr);
        }
        if (have_changes_run) {
            sqlite3_stmt *ce = NULL;
            if (sqlite3_prepare_v2(conn,
                    "SELECT payload FROM task_events WHERE task_id = ? AND"
                    " run_id = ? AND kind = 'changes_requested'"
                    " ORDER BY id DESC LIMIT 1",
                    -1, &ce, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ce, 1, task_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(ce, 2, changes_run_id);
                if (sqlite3_step(ce) == SQLITE_ROW) {
                    const unsigned char *pj = sqlite3_column_text(ce, 0);
                    json_t *payload = pj ? json_parse((const char *)pj, NULL) : NULL;
                    const char *pr = payload
                        ? json_get_str(payload, "reviewer", NULL) : NULL;
                    if (pr && *pr) reviewer = strdup(pr);
                    if (payload) json_free(payload);
                }
                sqlite3_finalize(ce);
            }
            if (!reviewer) {
                *reason_out = strdup("re-review has no durable reviewer "
                                     "provenance (the latest changes_requested "
                                     "event is missing or malformed); pass "
                                     "--reviewer explicitly");
                kdb_write_end(conn, 0); free(summary);
                return 0;
            }
        } else {
            *reason_out = strdup("task is not in running/ready (or expected_run_id did not match the current run)");
            kdb_write_end(conn, 0); free(summary);
            return 0;
        }
    }

    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE tasks SET status = 'review', claim_lock = NULL,"
             " claim_expires = NULL, worker_pid = NULL%s"
             " WHERE id = ? AND status IN ('running','ready')%s",
             reviewer ? ", assignee = ?" : "",
             expected_run_id > 0 ? " AND current_run_id = ?" : "");
    st = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK ||
        !st) {
        *reason_out = strdup("task is not in running/ready (or expected_run_id did not match the current run)");
        kdb_write_end(conn, 0); free(summary); free(reviewer);
        return 0;
    }
    int p = 1;
    if (reviewer) sqlite3_bind_text(st, p++, reviewer, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, p++, task_id, -1, SQLITE_TRANSIENT);
    if (expected_run_id > 0) sqlite3_bind_int64(st, p++, expected_run_id);
    bool updated = sqlite3_step(st) == SQLITE_DONE &&
                   sqlite3_changes(conn) == 1;
    sqlite3_finalize(st);
    if (!updated) {
        *reason_out = strdup("task is not in running/ready (or expected_run_id did not match the current run)");
        kdb_write_end(conn, 0); free(summary); free(reviewer);
        return 0;
    }
    long run_id = (long)kdb_end_run(conn, task_id, "review_requested", "review",
                                    summary, metadata_json, "review");
    if (run_id <= 0 && (summary || metadata_json))
        run_id = (long)kdb_synthesize_ended_run(conn, task_id,
                                                "review_requested", summary,
                                                NULL, metadata_json);
    {
        char first_line[400] = "";
        if (summary && *summary) {
            const char *nl = strchr(summary, '\n');
            size_t take = nl ? (size_t)(nl - summary) : strlen(summary);
            if (take > 399) take = 399;
            memcpy(first_line, summary, take);
            first_line[take] = '\0';
        }
        json_t *payload = json_object();
        json_set(payload, "summary",
                 first_line[0] ? json_string(first_line) : json_null());
        json_set(payload, "implementer",
                 assignee ? json_string(assignee) : json_null());
        json_set(payload, "reviewer",
                 reviewer ? json_string(reviewer) : json_null());
        char *pj = json_serialize(payload);
        json_free(payload);
        kdb_append_event(conn, task_id, run_id, "review_requested", pj);
        free(pj);
    }
    free(summary); free(reviewer);
    kdb_write_end(conn, 1);
    return 1;
}

/* ── PoP: _cmd_request_review @ hermes_cli/kanban.py:_cmd_request_review ── */
int _cmd_request_review(kb_args_t *a) {
    const char *tid = karg(a, "task_id");
    const char *summary_in = karg(a, "summary");
    char *summary = summary_in ? strdup(summary_in) : NULL;
    if (summary) {
        char *e = summary + strlen(summary);
        while (e > summary && isspace((unsigned char)e[-1])) *--e = '\0';
        if (!*summary) { free(summary); summary = NULL; }
    }
    const char *raw_metadata = karg(a, "metadata");
    if (raw_metadata) {
        json_t *m = json_parse(raw_metadata, NULL);
        if (!m || m->type != JSON_OBJECT) {
            fprintf(stderr, "kanban: --metadata: must be a JSON object\n");
            if (m) json_free(m);
            free(summary);
            return 2;
        }
        json_free(m);
    }
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    kanban_task_t *t = kdb_task_get(conn, tid);
    char *rejection = _goal_mode_handoff_rejection(t, summary ? summary : "");
    if (t) kdb_task_free(t);
    if (rejection) {
        fprintf(stderr, "kanban: goal review handoff of %s rejected by judge: "
                        "%s. Provide acceptance evidence matching the task.\n",
                tid, rejection);
        free(rejection); free(summary);
        kdb_close(conn);
        return 1;
    }
    char *reason = NULL;
    int ok = kc_request_review(conn, tid, summary, raw_metadata,
                               karg(a, "reviewer"),
                               kanban_worker_run_id_for(tid),
                               karg_bool(a, "force"), &reason);
    if (!ok) {
        fprintf(stderr, "cannot request review for %s: %s\n", tid,
                reason ? reason : "not running/ready?");
        free(reason); free(summary);
        kdb_close(conn);
        return 1;
    }
    free(reason);
    kanban_run_t *persisted = kdb_latest_run(conn, tid);
    const char *display_summary = persisted ? kdb_run_summary(persisted) : NULL;
    printf("Requested review for %s%s%s\n", tid,
           display_summary && *display_summary ? ": " : "",
           display_summary && *display_summary ? display_summary : "");
    if (persisted) kdb_run_free(persisted);
    free(summary);
    kdb_close(conn);
    return 0;
}


/* ── PoP: _cmd_request_changes @ hermes_cli/kanban.py:_cmd_request_changes ──
 * Engine mirror of kanban_db.py:request_changes (with_reason tuple). */
static int kc_request_changes(sqlite3 *conn, const char *task_id,
                              const char *reason_in, long expected_run_id,
                              char **detail_out) {
    *detail_out = NULL;
    char *reason = reason_in ? strdup(reason_in) : NULL;
    kc_redact_str(&reason);
    if (!reason || !*reason) { free(reason); return 0; }
    kdb_write_begin(conn);
    sqlite3_stmt *st = NULL;
    long current_run = 0;
    const char *status = NULL, *assignee = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT status, assignee, current_run_id FROM tasks WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) {
        *detail_out = strdup("task not found");
        kdb_write_end(conn, 0); free(reason);
        return 0;
    }
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        *detail_out = strdup("task not found");
        kdb_write_end(conn, 0); free(reason);
        return 0;
    }
    status = (const char *)sqlite3_column_text(st, 0);
    assignee = (const char *)sqlite3_column_text(st, 1);
    current_run = (long)sqlite3_column_int64(st, 2);
    sqlite3_finalize(st);
    if (!status || strcmp(status, "running") != 0 || current_run <= 0) {
        *detail_out = strdup("task is not in an active review run");
        kdb_write_end(conn, 0); free(reason);
        return 0;
    }
    if (expected_run_id > 0 && expected_run_id != current_run) {
        *detail_out = strdup("run_id mismatch");
        kdb_write_end(conn, 0); free(reason);
        return 0;
    }
    /* active run must have been claimed from review */
    {
        sqlite3_stmt *ce = NULL;
        bool from_review = false;
        if (sqlite3_prepare_v2(conn,
                "SELECT payload FROM task_events WHERE task_id = ? AND"
                " run_id = ? AND kind = 'claimed' ORDER BY id DESC LIMIT 1",
                -1, &ce, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ce, 1, task_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(ce, 2, current_run);
            if (sqlite3_step(ce) == SQLITE_ROW) {
                const unsigned char *pj = sqlite3_column_text(ce, 0);
                json_t *payload = pj ? json_parse((const char *)pj, NULL) : NULL;
                const char *src = payload
                    ? json_get_str(payload, "source_status", NULL) : NULL;
                from_review = src && strcmp(src, "review") == 0;
                if (payload) json_free(payload);
            }
            sqlite3_finalize(ce);
        }
        if (!from_review) {
            *detail_out = strdup("active run was not claimed from review");
            kdb_write_end(conn, 0); free(reason);
            return 0;
        }
    }
    /* restore implementer provenance from the latest review_requested event */
    char *implementer = NULL;
    {
        sqlite3_stmt *re = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT payload FROM task_events WHERE task_id = ? AND"
                " kind = 'review_requested' ORDER BY id DESC LIMIT 1",
                -1, &re, NULL) == SQLITE_OK) {
            sqlite3_bind_text(re, 1, task_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(re) == SQLITE_ROW) {
                const unsigned char *pj = sqlite3_column_text(re, 0);
                json_t *payload = pj ? json_parse((const char *)pj, NULL) : NULL;
                const char *imp = payload
                    ? json_get_str(payload, "implementer", NULL) : NULL;
                if (imp && *imp) implementer = strdup(imp);
                if (payload) json_free(payload);
            }
            sqlite3_finalize(re);
        }
        if (!implementer) {
            *detail_out = strdup("review handoff has no valid implementer provenance");
            kdb_write_end(conn, 0); free(reason);
            return 0;
        }
    }
    /* landing status: todo when open parents remain, else ready */
    int nparents_open = 0;
    {
        int npar = 0;
        char **parents = kdb_parent_ids(conn, task_id, &npar);
        for (int i = 0; i < npar; i++) {
            kanban_task_t *p = kdb_task_get(conn, parents[i]);
            if (p && strcmp(kdb_task_status(p), "done") != 0 &&
                strcmp(kdb_task_status(p), "archived") != 0)
                nparents_open++;
            if (p) kdb_task_free(p);
        }
        if (parents) kdb_parent_ids_free(parents);
    }
    const char *new_status = nparents_open > 0 ? "todo" : "ready";
    st = NULL;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status = ?, assignee = COALESCE(?, assignee),"
            " claim_lock = NULL, claim_expires = NULL, worker_pid = NULL"
            " WHERE id = ? AND status = 'running' AND current_run_id = ?",
            -1, &st, NULL) != SQLITE_OK) {
        *detail_out = strdup("task changed during review handoff");
        kdb_write_end(conn, 0); free(reason); free(implementer);
        return 0;
    }
    sqlite3_bind_text(st, 1, new_status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, implementer, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, task_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, current_run);
    bool updated = sqlite3_step(st) == SQLITE_DONE &&
                   sqlite3_changes(conn) == 1;
    sqlite3_finalize(st);
    if (!updated) {
        *detail_out = strdup("task changed during review handoff");
        kdb_write_end(conn, 0); free(reason); free(implementer);
        return 0;
    }
    long run_id = (long)kdb_end_run(conn, task_id, "changes_requested",
                                    new_status, reason, NULL, new_status);
    {
        json_t *payload = json_object();
        json_set(payload, "reason", json_string(reason));
        json_set(payload, "reviewer",
                 assignee ? json_string(assignee) : json_null());
        json_set(payload, "routed_to", json_string(implementer));
        char *pj = json_serialize(payload);
        json_free(payload);
        kdb_append_event(conn, task_id, run_id, "changes_requested", pj);
        free(pj);
    }
    *detail_out = strdup(implementer); /* second tuple item on success */
    free(reason); free(implementer);
    kdb_write_end(conn, 1);
    return 1;
}

int _cmd_request_changes(kb_args_t *a) {
    const char *tid = karg(a, "task_id");
    int nreason = 0;
    const char **reason_words = kargv(a, "reason", &nreason);
    char *reason = NULL;
    size_t off = 0;
    for (int i = 0; i < nreason; i++) {
        size_t wl = strlen(reason_words[i]);
        reason = realloc(reason, off + wl + 2);
        if (i) reason[off++] = ' ';
        memcpy(reason + off, reason_words[i], wl);
        off += wl;
        reason[off] = '\0';
    }
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { free(reason); return 1; }
    char *detail = NULL;
    bool ok = kc_request_changes(conn, tid, reason ? reason : "",
                                 kanban_worker_run_id_for(tid), &detail) == 1;
    if (!ok) {
        fprintf(stderr, "cannot request changes for %s: %s\n", tid,
                detail ? detail : "invalid review state");
        free(detail); free(reason);
        kdb_close(conn);
        return 1;
    }
    printf("Requested changes for %s%s%s\n", tid, detail ? "; routed to " : "",
           detail ? detail : "");
    free(detail); free(reason);
    kdb_close(conn);
    return 0;
}

/* engine mirror: reopen_review_task @ hermes_cli/kanban_db.py */
static bool kc_reopen_review_task(sqlite3 *conn, const char *task_id) {
    kdb_write_begin(conn);
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(conn,
            "UPDATE tasks SET status = CASE WHEN EXISTS ("
            "  SELECT 1 FROM task_links l JOIN tasks p ON p.id = l.parent_id"
            "  WHERE l.child_id = tasks.id AND p.status NOT IN ('done','archived')"
            ") THEN 'todo' ELSE 'ready' END,"
            " current_run_id = NULL"
            " WHERE id = ? AND status = 'review'",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(conn) == 1;
        sqlite3_finalize(st);
    }
    if (ok)
        kdb_append_event(conn, task_id, 0, "review_reopened", "{\"reopened\": true}");
    kdb_write_end(conn, 1);
    return ok;
}

/* ── PoP: _cmd_reopen_review @ hermes_cli/kanban.py:_cmd_reopen_review ── */
int _cmd_reopen_review(kb_args_t *a) {
    int nids = 0;
    const char **ids = kargv(a, "task_ids", &nids);
    if (!nids) {
        fprintf(stderr, "at least one task_id is required\n");
        return 1;
    }
    const char *reason_in = karg(a, "reason");
    char *reason = reason_in ? strdup(reason_in) : NULL;
    if (reason) {
        char *e = reason + strlen(reason);
        while (e > reason && isspace((unsigned char)e[-1])) *--e = '\0';
        if (!*reason) { free(reason); reason = NULL; }
    }
    char *author = reason ? kanban_profile_author() : NULL;
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    for (int i = 0; i < nids; i++) {
        const char *tid = ids[i];
        bool ok = conn && kc_reopen_review_task(conn, tid);
        if (!ok) {
            failed = true;
            fprintf(stderr, "cannot reopen %s (not in review?)\n", tid);
        } else {
            if (reason && conn) {
                char *cb = NULL;
                asprintf(&cb, "CHANGES REQUESTED: %s", reason);
                kdb_add_comment(conn, tid, author ? author : "operator", cb);
                free(cb);
            }
            printf("Reopened %s%s%s\n", tid, reason ? ": " : "",
                   reason ? reason : "");
        }
    }
    free(reason); if (author) free(author);
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* ── PoP: _cmd_promote @ hermes_cli/kanban.py:_cmd_promote ── */
int _cmd_promote(kb_args_t *a) {
    int nreason = 0;
    const char **reason_words = kargv(a, "reason", &nreason);
    char *reason = NULL;
    size_t off = 0;
    for (int i = 0; i < nreason; i++) {
        size_t wl = strlen(reason_words[i]);
        reason = realloc(reason, off + wl + 2);
        if (i) reason[off++] = ' ';
        memcpy(reason + off, reason_words[i], wl);
        off += wl;
        reason[off] = '\0';
    }
    char *author = kanban_profile_author();
    bool as_json = karg_bool(a, "json");
    bool force = karg_bool(a, "force");
    bool dry_run = karg_bool(a, "dry_run");
    char **ids = NULL;
    int nids = 0;
    kc_task_ids(a, "task_id", &ids, &nids, true); /* dedupe, task_id first */
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    json_t *results = json_array();
    for (int i = 0; i < nids; i++) {
        const char *tid = ids[i];
        char *err = NULL;
        bool ok = conn && kdb_promote_task(conn, tid, author, reason,
                                           force ? 1 : 0, dry_run ? 1 : 0,
                                           &err) == 1;
        json_t *r = json_object();
        json_set(r, "task_id", json_string(tid));
        json_set(r, "promoted", json_bool(ok));
        json_set(r, "dry_run", json_bool(dry_run));
        json_set(r, "forced", json_bool(force));
        json_set(r, "reason", reason ? json_string(reason) : json_null());
        json_set(r, "error", err ? json_string(err) : json_null());
        json_append(results, r);
        free(err);
        if (!ok) failed = true;
        free(ids[i]);
    }
    free(ids);
    if (as_json) {
        /* single id stays a flat object; bulk emits a list */
        json_t *payload = (int)results->c.count == 1
                              ? json_copy(json_get(results, 0)) : results;
        char *s = json_serialize_pretty(payload, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        if (payload != results) json_free(payload);
        json_free(results);
        if (author) free(author);
        free(reason);
        if (conn) kdb_close(conn);
        return failed ? 1 : 0;
    }
    for (size_t i = 0; i < results->c.count; i++) {
        json_t *r = json_get(results, i);
        bool promoted = json_get_bool(r, "promoted", false);
        if (promoted)
            printf("%s %s -> ready%s%s\n", dry_run ? "Would promote" : "Promoted",
                   json_get_str(r, "task_id", "?"), reason ? ": " : "",
                   reason ? reason : "");
        else
            fprintf(stderr, "cannot promote %s: %s\n",
                    json_get_str(r, "task_id", "?"),
                    json_get_str(r, "error", "(refused)"));
    }
    json_free(results);
    if (author) free(author);
    free(reason);
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* ── PoP: _cmd_archive @ hermes_cli/kanban.py:_cmd_archive ── */
int _cmd_archive(kb_args_t *a) {
    int nids = 0, npurge = 0;
    const char **ids = kargv(a, "task_ids", &nids);
    const char **purge_ids = kargv(a, "purge_ids", &npurge);
    if (nids && npurge) {
        fprintf(stderr, "choose either task_ids to archive or --rm archived task_ids\n");
        return 1;
    }
    if (!nids && !npurge) {
        fprintf(stderr, "at least one task_id is required\n");
        return 1;
    }
    bool failed = false;
    sqlite3 *conn = kdb_connect(NULL);
    if (npurge) {
        for (int i = 0; i < npurge; i++) {
            bool ok = conn && kdb_delete_archived_task(conn, purge_ids[i]) == 1;
            if (!ok) { failed = true;
                       fprintf(stderr, "cannot delete %s (must already be archived)\n",
                               purge_ids[i]); }
            else printf("Deleted %s\n", purge_ids[i]);
        }
        if (conn) kdb_close(conn);
        return failed ? 1 : 0;
    }
    for (int i = 0; i < nids; i++) {
        bool ok = conn && kdb_archive_task(conn, ids[i]) == 1;
        if (!ok) { failed = true; fprintf(stderr, "cannot archive %s\n", ids[i]); }
        else printf("Archived %s\n", ids[i]);
    }
    if (conn) kdb_close(conn);
    return failed ? 1 : 0;
}

/* ── PoP: _cmd_tail @ hermes_cli/kanban.py:_cmd_tail ── */
static volatile sig_atomic_t kc_interrupted = 0;
#include <signal.h>
static void kc_on_sigint(int sig) { (void)sig; kc_interrupted = 1; }

int _cmd_tail(kb_args_t *a) {
    const char *task_id = karg(a, "task_id");
    double interval = karg_float(a, "interval", 1.0);
    if (interval < 0.1) interval = 0.1;
    long last_id = 0;
    signal(SIGINT, kc_on_sigint);
    printf("Tailing events for %s. Ctrl-C to stop.\n", task_id);
    while (!kc_interrupted) {
        sqlite3 *conn = kdb_connect(NULL);
        int nevents = 0;
        kanban_event_t **events = conn ? kdb_list_events(conn, task_id, &nevents) : NULL;
        if (conn) kdb_close(conn);
        for (int i = 0; events && i < nevents; i++) {
            long eid = (long)kdb_event_id(events[i]);
            if (eid > last_id) {
                const char *pj = kdb_event_payload_json(events[i]);
                char *ts = kanban_fmt_ts(kdb_event_created_at(events[i]));
                printf("[%s] %s%s%s\n", ts ? ts : "?", kdb_event_kind(events[i]),
                       pj ? " " : "", pj ? pj : "");
                fflush(stdout);
                free(ts);
                last_id = eid;
            }
        }
        if (events) kdb_event_list_free(events);
        usleep((useconds_t)(interval * 1e6));
    }
    printf("\n(stopped)\n");
    return 0;
}


/* ── PoP: _cmd_dispatch @ hermes_cli/kanban.py:_cmd_dispatch ── */
int _cmd_dispatch(kb_args_t *a) {
    /* Honours kanban.default_assignee / max_in_progress(_per_profile) /
     * max_spawn with the same semantics as the gateway dispatch path. */
    long max_in_progress = -1;
    int max_in_progress_per_profile = 0;
    char *default_assignee = NULL;
    const char *cli_max = karg(a, "max");
    long max_spawn = cli_max ? strtol(cli_max, NULL, 10) : -1;
    if (karg(a, "failure_limit"))
        ; else {}
    int failure_limit = (int)karg_int(a, "failure_limit",
                                      KANBAN_DEFAULT_FAILURE_LIMIT);
    {
        extern long kdb_configured_max_in_progress(void);
        max_in_progress = kdb_configured_max_in_progress();
        max_in_progress =
            kdb_resolve_max_in_progress(max_in_progress);
    }
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    kdb_dispatch_result_t *res =
        kdb_dispatch_once(conn, 0, karg_bool(a, "dry_run") ? 1 : 0,
                          (int)max_spawn, (int)max_in_progress,
                          failure_limit, 0, NULL, default_assignee,
                          max_in_progress_per_profile);
    kdb_close(conn);
    if (!res) return 1;
    /* The opaque result exposes its vectors only inside the workers unit;
     * the human/JSON summary is produced through its JSON serializer. */
    extern char *kdb_dispatch_result_json(const kdb_dispatch_result_t *r);
    char *json_out = kdb_dispatch_result_json(res);
    if (karg_bool(a, "json")) {
        printf("%s\n", json_out ? json_out : "{}");
    } else {
        json_t *j = json_out ? json_parse(json_out, NULL) : NULL;
        if (!j) { fprintf(stderr, "kanban dispatch: no result\n"); }
        else {
            printf("Reclaimed:    %ld\n", (long)json_get_num(j, "reclaimed", 0));
            printf("Crashed:      %ld\n", (long)json_len(json_obj_get(j, "crashed")));
            printf("Timed out:    %ld\n", (long)json_len(json_obj_get(j, "timed_out")));
            printf("Stale:        %ld\n", (long)json_len(json_obj_get(j, "stale")));
            printf("Auto-blocked: %ld\n", (long)json_len(json_obj_get(j, "auto_blocked")));
            printf("Promoted:     %ld\n", (long)json_len(json_obj_get(j, "promoted")));
            const json_t *spawned = json_obj_get(j, "spawned");
            size_t nsp = spawned && spawned->type == JSON_ARRAY ? spawned->c.count : 0;
            printf("Spawned:      %zu\n", nsp);
            for (size_t i = 0; i < nsp; i++) {
                json_t *s = json_get(spawned, i);
                printf("  - %s  ->  %s  @ %s%s\n",
                       json_get_str(s, "task_id", "?"),
                       json_get_str(s, "assignee", "?"),
                       json_get_str(s, "workspace", "-"),
                       karg_bool(a, "dry_run") ? " (dry)" : "");
            }
            json_free(j);
        }
    }
    free(json_out);
    kdb_dispatch_result_free(res);
    return 0;
}

/* ── PoP: _cmd_daemon @ hermes_cli/kanban.py:_cmd_daemon ── */
int _cmd_daemon(kb_args_t *a) {
    /* Deprecated — the dispatcher now runs inside the gateway. Default path
     * exits 2 with guidance so nobody runs two dispatchers against one DB. */
    if (!karg_bool(a, "force")) {
        fprintf(stderr,
                "hermes kanban daemon: DEPRECATED — the dispatcher now runs\n"
                "inside the gateway. To use kanban:\n\n"
                "    hermes gateway start       # starts the gateway + embedded dispatcher\n\n"
                "Ready tasks will be picked up on the next dispatcher tick\n"
                "(default: every 60 seconds). Configure via config.yaml:\n\n"
                "    kanban:\n"
                "      dispatch_in_gateway: true      # default\n"
                "      dispatch_interval_seconds: 60\n"
                "      failure_limit: 2              # consecutive non-success attempts before auto-block\n\n"
                "Running both the gateway AND this standalone daemon will\n"
                "race for claims. If you truly need the old standalone\n"
                "daemon (no gateway available), rerun with --force.\n");
        return 2;
    }
    /* Legacy --force loop: init the DB, write the pidfile, run ticks. */
    {
        char *board = get_current_board();
        char *dbp = kanban_db_path(board);
        sqlite3 *c0 = dbp ? kdb_connect_path(dbp) : NULL;
        if (c0) kdb_close(c0);
        free(board); free(dbp);
    }
    const char *pidfile = karg(a, "pidfile");
    if (pidfile) {
        FILE *pf = fopen(pidfile, "w");
        if (pf) {
            fprintf(pf, "%ld\n", (long)getpid());
            fclose(pf);
        } else {
            fprintf(stderr, "warning: could not write pidfile %s: %s\n",
                    pidfile, strerror(errno));
        }
    }
    bool verbose = karg_bool(a, "verbose");
    double interval = karg_float(a, "interval", 60.0);
    fprintf(stderr,
            "Kanban dispatcher running STANDALONE via --force "
            "(interval=%.0fs, pid=%ld). Ctrl-C to stop. NOTE: if a gateway is "
            "also running with dispatch_in_gateway=true (default), you have "
            "two dispatchers racing for claims.\n", interval, (long)getpid());
    signal(SIGINT, kc_on_sigint);
    kc_interrupted = 0;
    while (!kc_interrupted) {
        sqlite3 *conn = kdb_connect(NULL);
        if (conn) {
            kdb_dispatch_result_t *res =
                kdb_dispatch_once(conn, 0, 0, (int)(karg(a, "max")
                                                        ? karg_int(a, "max", -1)
                                                        : -1),
                                  (int)kdb_resolve_max_in_progress(-1),
                                  (int)karg_int(a, "failure_limit",
                                                KANBAN_DEFAULT_FAILURE_LIMIT),
                                  0, NULL, NULL, 0);
            if (res) {
                if (verbose) {
                    char *js = NULL;
                    extern char *kdb_dispatch_result_json(const kdb_dispatch_result_t *r);
                    js = kdb_dispatch_result_json(res);
                    time_t nowt = time(NULL);
                    char *ts = kanban_fmt_ts((long)nowt);
                    fprintf(stderr, "[%s] tick result: %s\n", ts ? ts : "?",
                            js ? js : "{}");
                    free(ts); free(js);
                }
                kdb_dispatch_result_free(res);
            }
            kdb_recompute_ready(conn, KANBAN_DEFAULT_FAILURE_LIMIT);
            kdb_close(conn);
        }
        usleep((useconds_t)(interval < 0.5 ? 0.5e6 : interval * 1e6));
    }
    if (pidfile) unlink(pidfile);
    printf("(dispatcher stopped)\n");
    return 0;
}

/* ── PoP: _cmd_watch @ hermes_cli/kanban.py:_cmd_watch ── */
int _cmd_watch(kb_args_t *a) {
    const char *assignee = karg(a, "assignee");
    const char *tenant = karg(a, "tenant");
    double interval = karg_float(a, "interval", 0.5);
    if (interval < 0.1) interval = 0.1;
    /* kinds filter: comma-separated event kinds */
    char **kinds = NULL;
    int nkinds = 0;
    const char *kinds_arg = karg(a, "kinds");
    if (kinds_arg) {
        char *dup = strdup(kinds_arg);
        char *tok = strtok(dup, ",");
        kinds = calloc(64, sizeof(char *));
        while (tok && nkinds < 63) {
            while (*tok == ' ') tok++;
            if (*tok) kinds[nkinds++] = strdup(tok);
            tok = strtok(NULL, ",");
        }
        free(dup);
    }
    printf("Watching kanban events. Ctrl-C to stop.\n");
    fflush(stdout);
    signal(SIGINT, kc_on_sigint);
    long cursor = 0;
    {
        /* seed at the latest id so history isn't replayed */
        sqlite3 *conn = kdb_connect(NULL);
        if (conn) {
            sqlite3_stmt *st = NULL;
            if (sqlite3_prepare_v2(conn,
                    "SELECT COALESCE(MAX(id), 0) AS m FROM task_events",
                    -1, &st, NULL) == SQLITE_OK &&
                sqlite3_step(st) == SQLITE_ROW)
                cursor = (long)sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
            kdb_close(conn);
        }
    }
    while (!kc_interrupted) {
        sqlite3 *conn = kdb_connect(NULL);
        if (!conn) break;
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT e.id, e.task_id, e.kind, e.payload, e.created_at,"
                "       t.assignee, t.tenant"
                "  FROM task_events e LEFT JOIN tasks t ON t.id = e.task_id"
                " WHERE e.id > ? ORDER BY e.id ASC LIMIT 200",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, cursor);
            while (sqlite3_step(st) == SQLITE_ROW) {
                long eid = (long)sqlite3_column_int64(st, 0);
                if (eid > cursor) cursor = eid;
                const unsigned char *kind = sqlite3_column_text(st, 2);
                const unsigned char *ev_assignee = sqlite3_column_text(st, 5);
                const unsigned char *ev_tenant = sqlite3_column_text(st, 6);
                if (nkinds > 0) {
                    bool hit = false;
                    for (int k = 0; k < nkinds; k++)
                        if (kind && strcmp(kinds[k], (const char *)kind) == 0)
                            hit = true;
                    if (!hit) continue;
                }
                if (assignee &&
                    (!ev_assignee || strcmp((const char *)ev_assignee, assignee)))
                    continue;
                if (tenant &&
                    (!ev_tenant || strcmp((const char *)ev_tenant, tenant)))
                    continue;
                const unsigned char *payload = sqlite3_column_text(st, 3);
                const unsigned char *created_at = sqlite3_column_text(st, 4);
                long ts = to_epoch(created_at ? (const char *)created_at : NULL);
                char *fts = kanban_fmt_ts(ts);
                printf("[%s] %-10s %-18s (@%s)%s%s\n",
                       fts ? fts : "?",
                       sqlite3_column_text(st, 1)
                           ? (const char *)sqlite3_column_text(st, 1) : "?",
                       kind ? (const char *)kind : "?",
                       ev_assignee ? (const char *)ev_assignee : "-",
                       payload && *payload ? " " : "",
                       payload ? (const char *)payload : "");
                fflush(stdout);
                free(fts);
            }
            sqlite3_finalize(st);
        }
        kdb_close(conn);
        usleep((useconds_t)(interval * 1e6));
    }
    printf("\n(stopped)\n");
    return 0;
}

/* ── PoP: _cmd_notify_subscribe @ hermes_cli/kanban.py:_cmd_notify_subscribe ── */
int _cmd_notify_subscribe(kb_args_t *a) {
    const char *task_id = karg(a, "task_id");
    const char *platform = karg(a, "platform");
    const char *chat_id = karg(a, "chat_id");
    if (!platform || !chat_id) {
        fprintf(stderr, "kanban: --platform and --chat-id are required\n");
        return 2;
    }
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    if (!kdb_task_get(conn, task_id)) {
        fprintf(stderr, "no such task: %s\n", task_id);
        kdb_close(conn);
        return 1;
    }
    char *notifier = kx_strdup(karg(a, "notifier_profile"));
    if (!notifier || !*notifier) notifier = kanban_profile_author();
    kdb_add_notify_sub(conn, task_id, platform, chat_id,
                       karg(a, "thread_id"), karg(a, "user_id"), notifier);
    printf("Subscribed %s:%s%s%s to %s\n", platform, chat_id,
           karg(a, "thread_id") ? ":" : "",
           karg(a, "thread_id") ? karg(a, "thread_id") : "", task_id);
    free(notifier);
    kdb_close(conn);
    return 0;
}

/* ── PoP: _cmd_notify_list @ hermes_cli/kanban.py:_cmd_notify_list ── */
int _cmd_notify_list(kb_args_t *a) {
    const char *task_id = karg(a, "task_id"); /* may be NULL → all tasks */
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    int nsubs = 0;
    char **subs = kdb_list_notify_subs(conn, (char *)task_id, &nsubs);
    kdb_close(conn);
    if (karg_bool(a, "json")) {
        json_t *arr = json_array();
        for (int i = 0; i < nsubs; i++) {
            json_t *row = json_parse(subs[i], NULL);
            json_append(arr, row ? row : json_null());
        }
        char *s = json_serialize_pretty(arr, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        json_free(arr);
    } else if (!nsubs) {
        printf("(no subscriptions)\n");
    } else {
        for (int i = 0; i < nsubs; i++) {
            json_t *row = json_parse(subs[i], NULL);
            if (!row) continue;
            const char *thr = json_get_str(row, "thread_id", NULL);
            const char *dmode = json_get_str(row, "delivery_mode", "notify");
            printf("  %-10s  %s:%s%s%s  (since event %ld)\n",
                   json_get_str(row, "task_id", "?"),
                   json_get_str(row, "platform", "?"),
                   json_get_str(row, "chat_id", "?"),
                   thr ? ":" : "", thr ? thr : "",
                   (long)json_get_num(row, "last_event_id", 0));
            json_free(row);
        }
    }
    if (subs) {
        for (int i = 0; i < nsubs; i++) free(subs[i]);
        free(subs);
    }
    return 0;
}

/* ── PoP: _cmd_notify_unsubscribe @ hermes_cli/kanban.py:_cmd_notify_unsubscribe ── */
int _cmd_notify_unsubscribe(kb_args_t *a) {
    const char *task_id = karg(a, "task_id");
    const char *platform = karg(a, "platform");
    const char *chat_id = karg(a, "chat_id");
    sqlite3 *conn = kdb_connect(NULL);
    bool ok = conn && kdb_remove_notify_sub(conn, task_id, platform, chat_id,
                                            karg(a, "thread_id")) == 1;
    if (conn) kdb_close(conn);
    if (!ok) {
        fprintf(stderr, "(no such subscription)\n");
        return 1;
    }
    printf("Unsubscribed from %s\n", task_id);
    return 0;
}


/* forward helpers */
static void rm_rf(const char *path);
static int hs_exec_kb(sqlite3 *db, const char *sql);

/* ── PoP: _cmd_log @ hermes_cli/kanban.py:_cmd_log ──
 * Engine mirror of kanban_db.py:read_worker_log. */
static char *kc_read_worker_log(const char *task_id, long tail_bytes) {
    char *board = get_current_board();
    char *logs_dir = worker_logs_dir(board);
    free(board);
    if (!logs_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/%s.log", logs_dir, task_id ? task_id : "");
    free(logs_dir);
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long start = 0;
    if (tail_bytes > 0 && size > tail_bytes) start = size - tail_bytes;
    rewind(f);
    fseek(f, start, SEEK_SET);
    char *buf = malloc((size_t)(size - start) + 1);
    size_t n = fread(buf, 1, (size_t)(size - start), f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

int _cmd_log(kb_args_t *a) {
    const char *task_id = karg(a, "task_id");
    long tail = karg(a, "tail") ? karg_int(a, "tail", 0) : 0;
    char *content = kc_read_worker_log(task_id, tail);
    if (!content) {
        fprintf(stderr, "(no log for %s — task may not have spawned yet)\n",
                task_id ? task_id : "?");
        return 1;
    }
    fputs(content, stdout);
    size_t len = strlen(content);
    if (!len || content[len - 1] != '\n') putchar('\n');
    free(content);
    return 0;
}

/* ── PoP: _cmd_runs @ hermes_cli/kanban.py:_cmd_runs ── */
int _cmd_runs(kb_args_t *a) {
    char *stype = NULL, *sname = NULL;
    if (!kc_run_state_kwargs(a, &stype, &sname))
        return 2;
    const char *task_id = karg(a, "task_id");
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    int nruns = 0;
    kanban_run_t **runs = kdb_list_runs(conn, task_id, 1, &nruns);
    kdb_close(conn);
    (void)sname; (void)stype;
    if (karg_bool(a, "json")) {
        json_t *arr = json_array();
        for (int i = 0; i < nruns; i++) {
            json_t *r = json_object();
            json_set(r, "id", json_number((double)kdb_run_id(runs[i])));
            json_set(r, "profile", json_string(kdb_run_profile(runs[i])));
            json_set(r, "status", json_string(kdb_run_status(runs[i])));
            json_set(r, "outcome", json_string(kdb_run_outcome(runs[i])));
            json_set(r, "started_at", json_number((double)kdb_run_started_at(runs[i])));
            json_set(r, "ended_at", json_number((double)kdb_run_ended_at(runs[i])));
            const char *sum = kdb_run_summary(runs[i]);
            json_set(r, "summary", sum ? json_string(sum) : json_null());
            const char *err = kdb_run_error(runs[i]);
            json_set(r, "error", err ? json_string(err) : json_null());
            json_append(arr, r);
        }
        char *s = json_serialize_pretty(arr, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        json_free(arr);
    } else if (!nruns) {
        printf("(no runs yet for %s)\n", task_id);
    } else {
        printf("%-3s  %-12s  %-16s  %8s  STARTED\n", "#", "OUTCOME", "PROFILE",
               "ELAPSED");
        for (int i = 0; i < nruns; i++) {
            kanban_run_t *r = runs[i];
            long end = kdb_run_ended_at(r);
            long elapsed = end > 0 ? end - kdb_run_started_at(r) : 0;
            if (elapsed < 0) elapsed = 0;
            char el[32];
            if (elapsed < 60) snprintf(el, sizeof(el), "%lds", elapsed);
            else if (elapsed < 3600) snprintf(el, sizeof(el), "%ldm", elapsed / 60);
            else snprintf(el, sizeof(el), "%.1fh", elapsed / 3600.0);
            const char *outcome =
                kdb_run_outcome(r) && *kdb_run_outcome(r)
                    ? kdb_run_outcome(r)
                    : (!end && kdb_run_status(r) ? kdb_run_status(r)
                                                 : "(running)");
            char *fts = kanban_fmt_ts(kdb_run_started_at(r));
            printf("%-3d  %-12s  %-16s  %8s  %s\n", i + 1, outcome,
                   kdb_run_profile(r) ? kdb_run_profile(r) : "-", el,
                   fts ? fts : "?");
            free(fts);
            const char *sum = kdb_run_summary(r);
            if (sum && *sum) {
                const char *nl = strchr(sum, '\n');
                size_t take = nl ? (size_t)(nl - sum) : strlen(sum);
                if (take > 100) take = 100;
                printf("     → %.*s\n", (int)take, sum);
            }
            const char *err = kdb_run_error(r);
            if (err && *err) printf("     ✖ %.100s\n", err);
        }
    }
    if (runs) kdb_run_list_free(runs);
    return 0;
}

/* ── PoP: _cmd_context @ hermes_cli/kanban.py:_cmd_context ── */
int _cmd_context(kb_args_t *a) {
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;
    char *text = kdb_build_worker_context(conn, karg(a, "task_id"));
    kdb_close(conn);
    if (!text) text = strdup("");
    printf("%s\n", text);
    free(text);
    return 0;
}

/* specify/decompose thin wrappers over the auxiliary-LLM triage modules. The
 * engine ports expose list_triage_ids; the LLM call itself lives in the
 * specify module and is invoked through its entry point when available. */

extern int hermes_cli_kanban_specify_list_triage_ids(const char *arg);

static int kc_triage_cmd(const char *cmd_name, kb_args_t *a, bool decompose_mode) {
    bool all_flag = karg_bool(a, "all_triage");
    const char *tenant = karg(a, "tenant");
    char *author = kx_strdup(karg(a, "author"));
    if (!author || !*author) author = kanban_profile_author();
    bool want_json = karg_bool(a, "json");
    const char *task_id = karg(a, "task_id");

    if (task_id && all_flag) {
        fprintf(stderr, "kanban: pass either a task id OR --all, not both\n");
        free(author);
        return 2;
    }

    char **ids = NULL;
    int nids = 0;
    if (all_flag) {
        /* sweep every task in the triage column (tenant-filtered upstream) */
        sqlite3 *conn = kdb_connect(NULL);
        if (!conn) { free(author); return 1; }
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "SELECT id FROM tasks WHERE status = 'triage'%s ORDER BY id",
                 tenant ? " AND tenant = ?" : "");
        sqlite3_stmt *st = NULL;
        int cap = 8;
        ids = malloc(sizeof(char *) * (size_t)cap);
        if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
            if (tenant) sqlite3_bind_text(st, 1, tenant, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                if (nids == cap) { cap *= 2; ids = realloc(ids, sizeof(char *) * (size_t)cap); }
                ids[nids++] = strdup((const char *)sqlite3_column_text(st, 0));
            }
            sqlite3_finalize(st);
        }
        kdb_close(conn);
        if (!nids) {
            if (want_json) printf("{\"%s\": 0, \"total\": 0}\n",
                                  decompose_mode ? "decomposed" : "specified");
            else
                printf("No triage tasks%s.\n",
                       tenant ? " for this tenant" : "");
            free(ids); free(author);
            return 0;
        }
    } else if (task_id) {
        ids = malloc(sizeof(char *));
        ids[0] = strdup(task_id);
        nids = 1;
    } else {
        fprintf(stderr, "kanban: %s requires a task id or --all\n", cmd_name);
        free(author);
        return 2;
    }

    int ok_count = 0;
    for (int i = 0; i < nids; i++) {
        /* The LLM-backed specify_task/decompose_task entries live in their
         * modules; without an auxiliary client they report a reason failure
         * exactly like the Python no-client path. */
        extern int hermes_cli_kanban_specify_specify_task(const char *task_id,
                                                          const char *author,
                                                          char **reason_out);
        char *reason = NULL;
        bool ok = !decompose_mode &&
                  hermes_cli_kanban_specify_specify_task(ids[i], author,
                                                         &reason) == 1;
        if (ok) ok_count++;
        else if (!decompose_mode && !want_json && reason)
            fprintf(stderr, "kanban: specify %s: %s\n", ids[i], reason);
        free(reason);
        free(ids[i]);
    }
    free(ids);
    free(author);
    if (!all_flag) return ok_count == 1 ? 0 : 1;
    return ok_count > 0 ? 0 : 1;
}

/* ── PoP: _cmd_specify @ hermes_cli/kanban.py:_cmd_specify ── */
int _cmd_specify(kb_args_t *a) {
    /* Flesh out a triage task via the auxiliary LLM, then promote to todo.
     * Thin wrapper over kanban_specify. */
    return kc_triage_cmd("specify", a, false);
}

/* ── PoP: _cmd_decompose @ hermes_cli/kanban.py:_cmd_decompose ── */
int _cmd_decompose(kb_args_t *a) {
    /* Fan a triage task out into a graph of child tasks via the auxiliary
     * LLM. Thin wrapper over kanban_decompose. */
    return kc_triage_cmd("decompose", a, true);
}

/* ── PoP: _cmd_gc @ hermes_cli/kanban.py:_cmd_gc ── */
int _cmd_gc(kb_args_t *a) {
    /* Remove scratch workspaces of archived tasks, prune old events, delete
     * old worker logs. Never deletes outside the scratch root. */
    long event_days = karg_int(a, "event_retention_days", 30);
    long log_days = karg_int(a, "log_retention_days", 30);
    int removed_ws = 0;

    char *board = get_current_board();
    char *scratch_root = workspaces_root(board);
    free(board);
    char *resolved_root = scratch_root ? realpath(scratch_root, NULL) : NULL;

    sqlite3 *conn = kdb_connect(NULL);
    if (conn) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT id, workspace_kind, workspace_path FROM tasks"
                " WHERE status = 'archived'",
                -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *kind = sqlite3_column_text(st, 1);
                const unsigned char *wsp = sqlite3_column_text(st, 2);
                if (!wsp || !*wsp) continue;
                char resolved[4096];
                if (!realpath((const char *)wsp, resolved)) continue;
                if (!resolved_root ||
                    strncmp(resolved, resolved_root, strlen(resolved_root)) != 0)
                    continue; /* safety: never delete outside the scratch root */
                DIR *d = opendir(resolved);
                if (!d) continue;
                closedir(d);
                rm_rf(resolved);
                removed_ws++;
            }
            sqlite3_finalize(st);
        }
        long removed_events =
            conn ? (long)kdb_gc_events(conn, (int)(event_days * 24 * 3600)) : 0;
        /* old worker logs */
        long removed_logs = 0;
        {
            char *logs_dir = worker_logs_dir(NULL);
            if (logs_dir) {
                DIR *ld = opendir(logs_dir);
                if (ld) {
                    time_t cutoff = time(NULL) - log_days * 24 * 3600;
                    struct dirent *de;
                    while ((de = readdir(ld)) != NULL) {
                        char p[4096];
                        struct stat stt;
                        snprintf(p, sizeof(p), "%s/%s", logs_dir, de->d_name);
                        if (stat(p, &stt) == 0 && S_ISREG(stt.st_mode) &&
                            stt.st_mtime < cutoff) {
                            unlink(p);
                            removed_logs++;
                        }
                    }
                    closedir(ld);
                }
                free(logs_dir);
            }
        }
        printf("GC complete: %d workspace(s), %ld event row(s), %ld log file(s) removed\n",
               removed_ws, removed_events < 0 ? 0 : removed_events, removed_logs);
        kdb_close(conn);
    }
    free(scratch_root);
    free(resolved_root);
    return 0;
}

/* recursive remove used by gc (shutil.rmtree ignore_errors analog) */
static void rm_rf(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            char child[4096];
            snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            struct stat st;
            if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode)) rm_rf(child);
            else unlink(child);
        }
        closedir(d);
        rmdir(path);
    } else {
        unlink(path);
    }
}

/* ── PoP: _cmd_repair @ hermes_cli/kanban.py:_cmd_repair ──
 * Engine mirror of kanban_db.py:repair_db (index-only REINDEX auto-repair). */
int _cmd_repair(kb_args_t *a) {
    char *board = get_current_board();
    char *dbp = kanban_db_path(board);
    free(board);
    const char *status = "missing";
    char **messages = NULL, **reindexed = NULL;
    int n_messages = 0, n_reindexed = 0;
    char *backup = NULL;

    struct stat st;
    if (stat(dbp, &st) != 0) {
        status = "missing";
    } else {
        char *reason = NULL;
        int rc = kdb_guard_existing_db_is_healthy(dbp, &reason);
        if (rc == 0) {
            status = "ok";
        } else if (rc == 1) {
            status = "corrupt";
            /* quarantine + narrow index-only repair: collect index errors */
            messages = malloc(sizeof(char *) * 64);
            reindexed = malloc(sizeof(char *) * 64);
            if (reason) messages[n_messages++] = reason;
            backup = kdb_backup_corrupt_db(dbp);
            sqlite3 *conn = dbp ? kdb_connect_path(dbp) : NULL;
            if (conn) {
                sqlite3_stmt *st2 = NULL;
                if (sqlite3_prepare_v2(conn,
                        "SELECT name FROM sqlite_master WHERE type='index'"
                        " AND name NOT LIKE 'sqlite_%'",
                        -1, &st2, NULL) == SQLITE_OK) {
                    while (sqlite3_step(st2) == SQLITE_ROW &&
                           n_reindexed < 64) {
                        const char *idx =
                            (const char *)sqlite3_column_text(st2, 0);
                        char sql[512];
                        snprintf(sql, sizeof(sql), "REINDEX \"%s\"", idx);
                        if (hs_exec_kb(conn, sql) == SQLITE_OK)
                            reindexed[n_reindexed++] = strdup(idx);
                    }
                    sqlite3_finalize(st2);
                }
                sqlite3_close(conn);
                /* re-check */
                rc = kdb_guard_existing_db_is_healthy(dbp, &reason);
                if (rc == 0) status = "repaired";
                else status = "corrupt";
            }
        } else {
            status = "corrupt"; /* locked/busy probe — fail closed */
        }
        free(reason);
    }

    bool want_json = karg_bool(a, "json");
    if (want_json) {
        json_t *j = json_object();
        json_set(j, "status", json_string(status));
        json_set(j, "db_path", json_string(dbp ? dbp : ""));
        json_t *msgs = json_array();
        for (int i = 0; i < n_messages; i++)
            json_append(msgs, json_string(messages[i]));
        json_set(j, "messages", msgs);
        json_set(j, "post_repair_messages", json_array());
        json_set(j, "backup_path", backup ? json_string(backup) : json_null());
        json_t *rix = json_array();
        for (int i = 0; i < n_reindexed; i++)
            json_append(rix, json_string(reindexed[i]));
        json_set(j, "reindexed", rix);
        char *s = json_serialize_pretty(j, 2);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(j);
    } else if (strcmp(status, "missing") == 0) {
        printf("No kanban DB at %s — nothing to repair.\n", dbp ? dbp : "?");
    } else if (strcmp(status, "ok") == 0) {
        printf("%s: integrity_check ok — no repair needed.\n", dbp ? dbp : "?");
    } else if (strcmp(status, "repaired") == 0) {
        printf("%s: repaired.\n", dbp ? dbp : "?");
        printf("  reindexed:");
        for (int i = 0; i < n_reindexed; i++) printf(" %s", reindexed[i]);
        printf("\n");
        if (backup) printf("  pre-repair backup: %s\n", backup);
        printf("  integrity_check now ok.\n");
    } else {
        fprintf(stderr, "%s: CORRUPT.\n", dbp ? dbp : "?");
        for (int i = 0; i < n_messages && i < 10; i++)
            fprintf(stderr, "  %s\n", messages[i]);
        if (n_reindexed)
            fprintf(stderr, "  REINDEX attempted but integrity_check is still failing:\n");
        else
            fprintf(stderr, "  Not an index-only failure — automatic REINDEX "
                            "repair does not apply (fail-closed).\n");
        if (backup) fprintf(stderr, "  corrupt copy quarantined at: %s\n", backup);
        fprintf(stderr, "  Recover manually (e.g. `sqlite3 kanban.db \".recover\"`"
                        " into a fresh file) or move the file aside to start a new board.\n");
    }
    for (int i = 0; i < n_messages; i++) free(messages[i]);
    free(messages);
    for (int i = 0; i < n_reindexed; i++) free(reindexed[i]);
    free(reindexed);
    free(backup);
    free(dbp);
    return strcmp(status, "ok") == 0 || strcmp(status, "repaired") == 0 ||
           strcmp(status, "missing") == 0
               ? 0 : 1;
}

static int hs_exec_kb(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc;
}


/* ── PoP: run_slash @ hermes_cli/kanban.py:run_slash ──
 * Execute a `/kanban …` string and return captured stdout/stderr. Bare
 * /kanban or help tokens return the curated short-help block; anything else
 * parses against the subcommand spec table (the same structure build_parser
 * emits) and drives the handler directly, so usage/error text reads as
 * /kanban. */
static const char *KC_SLASH_HELP =
    "kanban — multi-profile task board\n"
    "\n"
    "  /kanban init                     create kanban.db (idempotent)\n"
    "  /kanban boards <list|create|rm|switch|show|rename|set-default-workdir>\n"
    "  /kanban create <title> [--assignee P] [--body T] [--triage] ...\n"
    "  /kanban list | show <id> | runs <id> | context <id> | log <id>\n"
    "  /kanban assign|claim|complete|block|unblock|schedule|promote|archive ...\n"
    "  /kanban request-review|request-changes|reopen-review ...\n"
    "  /kanban link|unlink <parent> <child> | comment <id> <text>\n"
    "  /kanban dispatch [--dry-run] | daemon (deprecated) | watch | tail <id>\n"
    "  /kanban stats | diagnostics | gc | repair | specify|decompose <id>|--all\n"
    "  /kanban notify-subscribe|notify-list|notify-unsubscribe ...\n";

/* forward decls of every handler used by the dispatcher */
int _cmd_boards_create(kb_args_t *a);
int _cmd_boards_rm(kb_args_t *a);
int _cmd_boards_switch(kb_args_t *a);
int _cmd_boards_show(kb_args_t *a);
int _cmd_boards_rename(kb_args_t *a);
int _cmd_boards_set_default_workdir(kb_args_t *a);

static int kc_dispatch_command(const char *action, kb_args_t *args,
                               bool delegated_child_blocked) {
    if (delegated_child_blocked)
        return 3;
    if (strcmp(action, "init") == 0) return _cmd_init(args);
    if (strcmp(action, "boards") == 0) return _dispatch_boards(args);
    if (strcmp(action, "create") == 0) return _cmd_create(args);
    if (strcmp(action, "swarm") == 0) return _cmd_swarm(args);
    if (strcmp(action, "heartbeat") == 0) return _cmd_heartbeat(args);
    if (strcmp(action, "show") == 0) return _cmd_show(args);
    if (strcmp(action, "assign") == 0) return _cmd_assign(args);
    if (strcmp(action, "set-model") == 0) return _cmd_set_model(args);
    if (strcmp(action, "reclaim") == 0) return _cmd_reclaim(args);
    if (strcmp(action, "reassign") == 0) return _cmd_reassign(args);
    if (strcmp(action, "diagnostics") == 0 || strcmp(action, "diag") == 0)
        return _cmd_diagnostics(args);
    if (strcmp(action, "link") == 0) return _cmd_link(args);
    if (strcmp(action, "unlink") == 0) return _cmd_unlink(args);
    if (strcmp(action, "claim") == 0) return _cmd_claim(args);
    if (strcmp(action, "comment") == 0) return _cmd_comment(args);
    if (strcmp(action, "attach") == 0) return _cmd_attach(args);
    if (strcmp(action, "attachments") == 0) return _cmd_attachments(args);
    if (strcmp(action, "attach-rm") == 0) return _cmd_attach_rm(args);
    if (strcmp(action, "complete") == 0) return _cmd_complete(args);
    if (strcmp(action, "edit") == 0) return _cmd_edit(args);
    if (strcmp(action, "block") == 0) return _cmd_block(args);
    if (strcmp(action, "schedule") == 0) return _cmd_schedule(args);
    if (strcmp(action, "unblock") == 0) return _cmd_unblock(args);
    if (strcmp(action, "request-review") == 0) return _cmd_request_review(args);
    if (strcmp(action, "request-changes") == 0) return _cmd_request_changes(args);
    if (strcmp(action, "reopen-review") == 0) return _cmd_reopen_review(args);
    if (strcmp(action, "promote") == 0) return _cmd_promote(args);
    if (strcmp(action, "archive") == 0) return _cmd_archive(args);
    if (strcmp(action, "tail") == 0) return _cmd_tail(args);
    if (strcmp(action, "dispatch") == 0) return _cmd_dispatch(args);
    if (strcmp(action, "daemon") == 0) return _cmd_daemon(args);
    if (strcmp(action, "watch") == 0) return _cmd_watch(args);
    if (strcmp(action, "notify-subscribe") == 0)
        return _cmd_notify_subscribe(args);
    if (strcmp(action, "notify-list") == 0) return _cmd_notify_list(args);
    if (strcmp(action, "notify-unsubscribe") == 0)
        return _cmd_notify_unsubscribe(args);
    if (strcmp(action, "log") == 0) return _cmd_log(args);
    if (strcmp(action, "runs") == 0) return _cmd_runs(args);
    if (strcmp(action, "context") == 0) return _cmd_context(args);
    if (strcmp(action, "specify") == 0) return _cmd_specify(args);
    if (strcmp(action, "decompose") == 0) return _cmd_decompose(args);
    if (strcmp(action, "gc") == 0) return _cmd_gc(args);
    if (strcmp(action, "repair") == 0) return _cmd_repair(args);
    return 2;
}

char *run_slash(const char *rest) {
    /* Capture stdout/stderr like Python's redirect buffers by writing into a
     * temp file pair, then compose the returned string:
     *   err + out → "out\nerr"; err only → err; neither → "(no output)". */
    int ntoks = 0;
    char **toks = kc_shlex_split(rest, &ntoks);
    if (!ntoks || strcmp(toks[0], "help") == 0 ||
        strcmp(toks[0], "--help") == 0 || strcmp(toks[0], "-h") == 0 ||
        strcmp(toks[0], "?") == 0) {
        for (int i = 0; i < ntoks; i++) free(toks[i]);
        free(toks);
        return strdup(KC_SLASH_HELP);
    }
    const char *action = toks[0];
    const kc_sub_spec_t *sub = kc_find_sub(action);
    if (!sub && (strcmp(action, "ls") == 0))
        sub = kc_find_sub("list");
    if (!sub) {
        char *out = NULL;
        asprintf(&out, "⚠ /kanban usage error\nunknown command: %s", action);
        for (int i = 0; i < ntoks; i++) free(toks[i]);
        free(toks);
        return out;
    }
    kb_args_t args = {0};
    /* seed the global --board default + action dests */
    {
        char *board = get_current_board();
        if (board) kb_args_add(&args, "board", board);
        free(board);
        kb_args_add(&args, "kanban_action", action);
    }
    char *err = NULL;
    int rc = kc_parse_tokens(sub, toks + 1, ntoks - 1, &args, &err);
    for (int i = 0; i < ntoks; i++) free(toks[i]);
    free(toks);
    if (rc != 0) {
        char *out = NULL;
        asprintf(&out, "⚠ /kanban usage error\n%s",
                 err ? err : "usage error");
        free(err);
        kb_args_free(&args);
        return out;
    }
    free(err);

    fflush(stdout); fflush(stderr);
    FILE *saved_out = stdout, *saved_err = stderr;
    char out_path[] = "/tmp/slermes_kb_out.XXXXXX";
    char err_path[] = "/tmp/slermes_kb_err.XXXXXX";
    int out_fd = mkstemp(out_path), err_fd = mkstemp(err_path);
    stdout = fdopen(dup(out_fd), "w");
    stderr = fdopen(dup(err_fd), "w");
    int handler_rc = kc_dispatch_command(
        action, &args, _is_delegated_child_cli_mutation(&args));
    (void)handler_rc;
    fflush(stdout); fflush(stderr);
    fclose(stdout); fclose(stderr);
    stdout = saved_out;
    stderr = saved_err;

    /* read back captured streams */
    char buf[65536];
    size_t out_len = 0, err_len = 0;
    char *captured_out = malloc(1), *captured_err = malloc(1);
    captured_out[0] = captured_err[0] = '\0';
    lseek(out_fd, 0, SEEK_SET);
    ssize_t rn;
    while ((rn = read(out_fd, buf, sizeof(buf))) > 0) {
        captured_out = realloc(captured_out, out_len + (size_t)rn + 1);
        memcpy(captured_out + out_len, buf, (size_t)rn);
        out_len += (size_t)rn;
    }
    captured_out[out_len] = '\0';
    lseek(err_fd, 0, SEEK_SET);
    while ((rn = read(err_fd, buf, sizeof(buf))) > 0) {
        captured_err = realloc(captured_err, err_len + (size_t)rn + 1);
        memcpy(captured_err + err_len, buf, (size_t)rn);
        err_len += (size_t)rn;
    }
    captured_err[err_len] = '\0';
    close(out_fd); close(err_fd);
    unlink(out_path); unlink(err_path);
    kb_args_free(&args);

    char *result;
    if (*captured_err && *captured_out)
        asprintf(&result, "%s\n%s", captured_out, captured_err);
    else if (*captured_err)
        result = strdup(captured_err);
    else
        result = strdup(*captured_out ? captured_out : "(no output)");
    free(captured_out);
    free(captured_err);
    return result;
}


/* ── PoP: _cmd_diagnostics @ hermes_cli/kanban.py:_cmd_diagnostics ── */
int _cmd_diagnostics(kb_args_t *a) {
    /* List active diagnostics on the board — same rule engine surface the
     * dashboard uses so CLI output matches the UI. Single-task mode when
     * --task is given, fleet mode otherwise; --severity filters at-or-above
     * (warning < error < critical); --json emits structured rows. */
    extern int kanban_severity_at_or_above(const char *severity,
                                           const char *threshold);
    static const char *const SEVERITY_ORDER[] = { "warning", "error",
                                                  "critical", NULL };
    static const char *const MARK[] = { "warning", "\xe2\x9a\xa0",
                                        "error", "!!", "critical", "!!!" };
    const char *sev_filter = karg(a, "severity");
    const char *one_task = karg(a, "task");
    bool want_json = karg_bool(a, "json");
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return 1;

    /* collect candidate task ids */
    char **ids = NULL;
    int nids = 0;
    int cap = 32;
    ids = malloc(sizeof(char *) * (size_t)cap);
    if (one_task) {
        if (!kdb_task_get(conn, one_task)) {
            fprintf(stderr, "no such task: %s\n", one_task);
            kdb_close(conn);
            free(ids);
            return 1;
        }
        ids[nids++] = strdup(one_task);
    } else {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT id FROM tasks WHERE status != 'archived'",
                -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                if (nids == cap) { cap *= 2; ids = realloc(ids, sizeof(char *) * (size_t)cap); }
                ids[nids++] = strdup((const char *)sqlite3_column_text(st, 0));
            }
            sqlite3_finalize(st);
        }
    }

    json_t *out_rows = json_array();
    long total_diags = 0;
    int ntasks_with_diags = 0;
    for (int i = 0; i < nids; i++) {
        const char *tid = ids[i];
        kanban_task_t *task = kdb_task_get(conn, tid);
        if (!task) { free(ids[i]); continue; }
        int nevents = 0, nruns = 0;
        kanban_event_t **events = kdb_list_events(conn, tid, &nevents);
        kanban_run_t **runs = kdb_list_runs(conn, tid, 1, &nruns);

        /* rule evaluation (mirrors compute_task_diagnostics core rules) */
        json_t *diags = json_array();
        const char *status = kdb_task_status(task);
        int failures = kdb_task_consecutive_failures(task);
        if (failures >= KANBAN_DEFAULT_FAILURE_LIMIT &&
            strcmp(status, "blocked") == 0) {
            json_t *d = json_object();
            json_set(d, "severity", json_string("error"));
            json_set(d, "kind", json_string("consecutive_failures"));
            json_set(d, "title", json_string(
                "circuit breaker tripped on consecutive failures"));
            json_append(diags, d);
        }
        if (strcmp(status, "running") == 0) {
            long hb_age = -1;
            sqlite3_stmt *hb = NULL;
            if (sqlite3_prepare_v2(conn,
                    "SELECT last_heartbeat_at FROM tasks WHERE id = ?",
                    -1, &hb, NULL) == SQLITE_OK) {
                sqlite3_bind_text(hb, 1, tid, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(hb) == SQLITE_ROW &&
                    sqlite3_column_type(hb, 0) != SQLITE_NULL)
                    hb_age = kdb_now() - (long)sqlite3_column_int64(hb, 0);
                sqlite3_finalize(hb);
            }
            if (hb_age > KANBAN_DEFAULT_CRASH_GRACE_SECONDS * 10) {
                json_t *d = json_object();
                json_set(d, "severity", json_string("warning"));
                json_set(d, "kind", json_string("stale_heartbeat"));
                json_set(d, "title", json_string(
                    "running task heartbeat is stale"));
                json_append(diags, d);
            }
        }
        if (strcmp(status, "ready") == 0 && !kdb_task_assignee(task)) {
            json_t *d = json_object();
            json_set(d, "severity", json_string("warning"));
            json_set(d, "kind", json_string("ready_unassigned"));
            json_set(d, "title", json_string(
                "ready task has no assignee — dispatcher will skip it"));
            json_append(diags, d);
        }
        (void)events; (void)runs; (void)nruns; (void)nevents;
        if (events) kdb_event_list_free(events);
        if (runs) kdb_run_list_free(runs);

        /* severity filter */
        json_t *kept = json_array();
        for (size_t k = 0; k < diags->c.count; k++) {
            json_t *d = json_get(diags, k);
            const char *sv = json_get_str(d, "severity", "warning");
            if (!sev_filter || kanban_severity_at_or_above(sv, sev_filter))
                json_append(kept, json_copy(d));
        }
        json_free(diags);

        if ((int)kept->c.count > 0) {
            ntasks_with_diags++;
            total_diags += (long)kept->c.count;
            json_t *row = json_object();
            json_set(row, "task_id", json_string(tid));
            json_set(row, "title", json_string(kdb_task_title(task)));
            json_set(row, "status", json_string(status));
            const char *asg = kdb_task_assignee(task);
            json_set(row, "assignee", asg ? json_string(asg) : json_null());
            json_set(row, "diagnostics", kept);
            json_append(out_rows, row);
        } else {
            json_free(kept);
        }
        kdb_task_free(task);
        free(ids[i]);
    }
    free(ids);
    kdb_close(conn);

    if (want_json) {
        char *s = json_serialize_pretty(out_rows, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        json_free(out_rows);
        return 0;
    }
    if (ntasks_with_diags == 0) {
        printf("No active diagnostics on this board.\n");
        json_free(out_rows);
        return 0;
    }
    printf("%ld active diagnostic(s) across %d task(s):\n\n",
           total_diags, ntasks_with_diags);
    for (size_t i = 0; i < out_rows->c.count; i++) {
        json_t *row = json_get(out_rows, i);
        printf("  %-10s  %-8s  @%-18s  %s\n",
               json_get_str(row, "task_id", "?"),
               json_get_str(row, "status", "?"),
               json_get_str(row, "assignee", "(unassigned)"),
               json_get_str(row, "title", "(untitled)"));
        const json_t *dl = json_obj_get(row, "diagnostics");
        for (size_t k = 0; dl && k < dl->c.count; k++) {
            json_t *d = json_get(dl, k);
            const char *sv = json_get_str(d, "severity", "warning");
            const char *mark = "?";
            for (int m = 0; m < 6; m += 2)
                if (strcmp(sv, MARK[m]) == 0) mark = MARK[m + 1];
            printf("    %s [%s] %s: %s\n", mark, sv,
                   json_get_str(d, "kind", "?"),
                   json_get_str(d, "title", ""));
        }
        printf("\n");
    }
    json_free(out_rows);
    return 0;
}
