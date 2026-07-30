/*
 * kanban_model.c — opaque model structs for hermes_cli/kanban_db.py
 *
 * Concern: the in-memory view objects (Task / Run / Comment / Attachment /
 * Event). These are OPAQUE — the struct bodies live ONLY here, and every
 * other concern reaches fields through the *_get accessors in kanban_db.h.
 * That keeps the data layout private and the modules decoupled (no god
 * header, no shared struct header).
 *
 * from_row binds columns by name (mirrors Task.from_row / Run.from_row /
 * Comment / Attachment / Event), tolerating legacy/renamed columns exactly
 * like the Python code.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py.
 * PoP: kdb_task_from_row @ hermes_cli/kanban_db.py:Task
 * PoP: kdb_canon_assignee @ hermes_cli/kanban_db.py:_canonical_assignee
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- private struct bodies (opaque outside this TU) ---- */

struct kanban_task {
    char *id, *title, *body, *assignee, *status;
    char *workspace_kind, *workspace_path, *branch_name, *tenant;
    char *result, *idempotency_key, *project_id, *session_id;
    char *block_kind, *model_override, *skills_json, *created_by;
    int   priority;
    long  created_at, started_at, completed_at;
    long  claim_expires, current_run_id;
    int   consecutive_failures, block_recurrences;
    int   goal_mode;
    long  goal_max_turns;
    long  max_runtime_seconds, last_heartbeat_at;
    long  worker_pid;
};

struct kanban_run {
    long  id;
    char *task_id, *profile, *step_key, *status;
    char *claim_lock, *outcome, *summary, *metadata_json, *error;
    long  claim_expires, worker_pid, max_runtime_seconds;
    long  started_at, ended_at, last_heartbeat_at;
};

struct kanban_comment {
    long  id;
    char *task_id, *author, *body;
    long  created_at;
};

struct kanban_attach {
    long  id;
    char *task_id, *filename, *stored_path, *content_type, *uploaded_by;
    long  size, created_at;
};

struct kanban_event {
    long  id;
    char *task_id, *kind, *payload_json;
    long  created_at;
    long  run_id;
};

/* ---- helpers ---- */

static char *dup_or_null(const char *s) { return s ? strdup(s) : NULL; }
/* Locate a column by name (manual scan — portable across SQLite versions).
 * Returns -1 when absent. Mirrors the Python row.keys() tolerance for
 * legacy/renamed columns. */
static int col_index(sqlite3_stmt *st, const char *name)
{
    int n = sqlite3_column_count(st);
    for (int i = 0; i < n; i++) {
        const char *nm = sqlite3_column_name(st, i);
        if (nm && strcmp(nm, name) == 0) return i;
    }
    return -1;
}

static char *col_text(sqlite3_stmt *st, const char *name)
{
    int idx = col_index(st, name);
    if (idx < 0) return NULL;
    const char *t = (const char *)sqlite3_column_text(st, idx);
    return t ? strdup(t) : NULL;
}
static long col_long(sqlite3_stmt *st, const char *name, long dflt)
{
    int idx = col_index(st, name);
    if (idx < 0) return dflt;
    if (sqlite3_column_type(st, idx) == SQLITE_NULL) return dflt;
    return (long)sqlite3_column_int64(st, idx);
}
static int col_int(sqlite3_stmt *st, const char *name, int dflt)
{
    int idx = col_index(st, name);
    if (idx < 0) return dflt;
    if (sqlite3_column_type(st, idx) == SQLITE_NULL) return dflt;
    return (int)sqlite3_column_int(st, idx);
}

/* ---- Task ---- */

/* PoP: kdb_task_from_row @ hermes_cli/kanban_db.py:Task */
kanban_task_t *kdb_task_from_row(sqlite3_stmt *st)
{
    if (!st) return NULL;
    kanban_task_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->id               = col_text(st, "id");
    t->title            = col_text(st, "title");
    t->body             = col_text(st, "body");
    t->assignee         = col_text(st, "assignee");
    t->status           = col_text(st, "status");
    t->workspace_kind   = col_text(st, "workspace_kind");
    t->workspace_path   = col_text(st, "workspace_path");
    t->branch_name      = col_text(st, "branch_name");
    t->project_id       = col_text(st, "project_id");
    t->tenant           = col_text(st, "tenant");
    t->result           = col_text(st, "result");
    t->idempotency_key  = col_text(st, "idempotency_key");
    t->session_id       = col_text(st, "session_id");
    t->block_kind       = col_text(st, "block_kind");
    t->model_override   = col_text(st, "model_override");
    t->skills_json      = col_text(st, "skills");
    t->created_by       = col_text(st, "created_by");
    t->priority         = col_int(st, "priority", 0);
    t->created_at       = col_long(st, "created_at", 0);
    t->started_at       = col_long(st, "started_at", 0);
    t->completed_at     = col_long(st, "completed_at", 0);
    t->claim_expires    = col_long(st, "claim_expires", 0);
    t->current_run_id   = col_long(st, "current_run_id", 0);
    t->consecutive_failures = col_int(st, "consecutive_failures", 0);
    t->block_recurrences     = col_int(st, "block_recurrences", 0);
    t->goal_mode        = col_int(st, "goal_mode", 0);
    t->goal_max_turns   = col_long(st, "goal_max_turns", 0);
    t->max_runtime_seconds = col_long(st, "max_runtime_seconds", 0);
    t->last_heartbeat_at = col_long(st, "last_heartbeat_at", 0);
    t->worker_pid       = col_long(st, "worker_pid", 0);
    if (!t->workspace_kind) t->workspace_kind = strdup("scratch");
    return t;
}

/* PoP: kdb_task_get @ hermes_cli/kanban_db.py:get_task */
kanban_task_t *kdb_task_get(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT * FROM tasks WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
    kanban_task_t *t = NULL;
    if (sqlite3_step(st) == SQLITE_ROW)
        t = kdb_task_from_row(st);
    sqlite3_finalize(st);
    return t;
}

void kdb_task_free(kanban_task_t *t)
{
    if (!t) return;
    free(t->id); free(t->title); free(t->body); free(t->assignee);
    free(t->status); free(t->workspace_kind); free(t->workspace_path);
    free(t->branch_name); free(t->project_id); free(t->tenant);
    free(t->result); free(t->idempotency_key); free(t->session_id);
    free(t->block_kind); free(t->model_override); free(t->skills_json); free(t->created_by);
    free(t);
}

/* PoP: kdb_task_id @ hermes_cli/kanban_db.py:id */
const char *kdb_task_id(const kanban_task_t *t)            { return t ? t->id : NULL; }
/* PoP: kdb_task_title @ hermes_cli/kanban_db.py:title */
const char *kdb_task_title(const kanban_task_t *t)         { return t ? t->title : NULL; }
/* PoP: kdb_task_status @ hermes_cli/kanban_db.py:status */
const char *kdb_task_status(const kanban_task_t *t)        { return t ? t->status : NULL; }
/* PoP: kdb_task_priority @ hermes_cli/kanban_db.py:priority */
int         kdb_task_priority(const kanban_task_t *t)      { return t ? t->priority : 0; }
/* PoP: kdb_task_assignee @ hermes_cli/kanban_db.py:assignee */
const char *kdb_task_assignee(const kanban_task_t *t)      { return t ? t->assignee : NULL; }
/* PoP: kdb_task_body @ hermes_cli/kanban_db.py:body */
const char *kdb_task_body(const kanban_task_t *t)          { return t ? t->body : NULL; }
/* PoP: kdb_task_workspace_kind @ hermes_cli/kanban_db.py:workspace_kind */
const char *kdb_task_workspace_kind(const kanban_task_t *t){ return t ? t->workspace_kind : NULL; }
/* PoP: kdb_task_workspace_path @ hermes_cli/kanban_db.py:workspace_path */
const char *kdb_task_workspace_path(const kanban_task_t *t){ return t ? t->workspace_path : NULL; }
/* PoP: kdb_task_tenant @ hermes_cli/kanban_db.py:tenant */
const char *kdb_task_tenant(const kanban_task_t *t)        { return t ? t->tenant : NULL; }
/* PoP: kdb_task_created_by @ hermes_cli/kanban_db.py:created_by */
const char *kdb_task_created_by(const kanban_task_t *t)    { return t ? t->created_by : NULL; }
/* PoP: kdb_task_created_at @ hermes_cli/kanban_db.py:created_at */
long        kdb_task_created_at(const kanban_task_t *t)    { return t ? t->created_at : 0; }
/* PoP: kdb_task_started_at @ hermes_cli/kanban_db.py:started_at */
long        kdb_task_started_at(const kanban_task_t *t)    { return t ? t->started_at : 0; }
/* PoP: kdb_task_completed_at @ hermes_cli/kanban_db.py:completed_at */
long        kdb_task_completed_at(const kanban_task_t *t)  { return t ? t->completed_at : 0; }
/* PoP: kdb_task_claim_expires @ hermes_cli/kanban_db.py:claim_expires */
long        kdb_task_claim_expires(const kanban_task_t *t) { return t ? t->claim_expires : 0; }
/* PoP: kdb_task_branch_name @ hermes_cli/kanban_db.py:branch_name */
const char *kdb_task_branch_name(const kanban_task_t *t)   { return t ? t->branch_name : NULL; }
/* PoP: kdb_task_result @ hermes_cli/kanban_db.py:result */
const char *kdb_task_result(const kanban_task_t *t)        { return t ? t->result : NULL; }
/* PoP: kdb_task_consecutive_failures @ hermes_cli/kanban_db.py:consecutive_failures */
int         kdb_task_consecutive_failures(const kanban_task_t *t){ return t ? t->consecutive_failures : 0; }
/* PoP: kdb_task_last_failure_error @ hermes_cli/kanban_db.py:last_failure_error */
const char *kdb_task_last_failure_error(const kanban_task_t *t){ return NULL; /* stored separately */ }
/* PoP: kdb_task_current_run_id @ hermes_cli/kanban_db.py:current_run_id */
long        kdb_task_current_run_id(const kanban_task_t *t){ return t ? t->current_run_id : 0; }
/* PoP: kdb_task_block_kind @ hermes_cli/kanban_db.py:block_kind */
const char *kdb_task_block_kind(const kanban_task_t *t)    { return t ? t->block_kind : NULL; }
/* PoP: kdb_task_block_recurrences @ hermes_cli/kanban_db.py:block_recurrences */
int         kdb_task_block_recurrences(const kanban_task_t *t){ return t ? t->block_recurrences : 0; }
/* PoP: kdb_task_model_override @ hermes_cli/kanban_db.py:model_override */
const char *kdb_task_model_override(const kanban_task_t *t){ return t ? t->model_override : NULL; }
/* PoP: kdb_task_goal_mode @ hermes_cli/kanban_db.py:goal_mode */
int         kdb_task_goal_mode(const kanban_task_t *t){ return t ? t->goal_mode : 0; }
/* PoP: kdb_task_goal_max_turns @ hermes_cli/kanban_db.py:goal_max_turns */
long        kdb_task_goal_max_turns(const kanban_task_t *t){ return t ? t->goal_max_turns : 0; }
/* PoP: kdb_task_max_runtime_seconds @ hermes_cli/kanban_db.py:max_runtime_seconds */
long        kdb_task_max_runtime_seconds(const kanban_task_t *t){ return t ? t->max_runtime_seconds : 0; }

/* PoP: kdb_task_skills @ hermes_cli/kanban_db.py (task.skills JSON array).
 * Parses the `skills_json` field (a JSON array of strings) into a malloc'd
 * NULL-terminated array of skill names. Caller frees with kdb_strv_free.
 * Returns NULL when there are no skills / parse fails. */
char **kdb_task_skills(const kanban_task_t *t)
{
    if (!t || !t->skills_json || !t->skills_json[0]) return NULL;
    const char *s = t->skills_json;
    /* require a JSON array */
    while (*s && (*s==' '||*s=='\t'||*s=='\n'||*s=='\r')) s++;
    if (*s != '[') return NULL;
    s++;
    char **out = calloc(16, sizeof(char *));
    int n = 0;
    while (*s) {
        while (*s && *s != '"' && *s != ']' && *s != ',') s++;
        if (*s == ']') break;
        if (*s == '"') {
            s++;
            size_t start = 0; char buf[256];
            while (*s && *s != '"' && start < sizeof(buf)-1) buf[start++] = *s++;
            buf[start] = 0;
            if (*s == '"') s++;           /* consume closing quote */
            if (start && n < 15) out[n++] = strdup(buf);
        } else if (*s == ',') {
            s++;
        }
    }
    out[n] = NULL;
    if (n == 0) { free(out); return NULL; }
    return out;
}

/* ---- Run ---- */

kanban_run_t *kdb_run_from_row(sqlite3_stmt *st)
{
    kanban_run_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->id                = col_long(st, "id", 0);
    r->task_id           = col_text(st, "task_id");
    r->profile           = col_text(st, "profile");
    r->step_key          = col_text(st, "step_key");
    r->status            = col_text(st, "status");
    r->claim_lock        = col_text(st, "claim_lock");
    r->outcome           = col_text(st, "outcome");
    r->summary           = col_text(st, "summary");
    r->metadata_json     = col_text(st, "metadata");
    r->error             = col_text(st, "error");
    r->claim_expires     = col_long(st, "claim_expires", 0);
    r->worker_pid        = col_long(st, "worker_pid", 0);
    r->max_runtime_seconds = col_long(st, "max_runtime_seconds", 0);
    r->started_at        = col_long(st, "started_at", 0);
    r->ended_at          = col_long(st, "ended_at", 0);
    r->last_heartbeat_at = col_long(st, "last_heartbeat_at", 0);
    return r;
}

void kdb_run_free(kanban_run_t *r)
{
    if (!r) return;
    free(r->task_id); free(r->profile); free(r->step_key); free(r->status);
    free(r->claim_lock); free(r->outcome); free(r->summary);
    free(r->metadata_json); free(r->error);
    free(r);
}

/* PoP: kdb_run_outcome @ hermes_cli/kanban_db.py:outcome */
const char *kdb_run_outcome(const kanban_run_t *r)   { return r ? r->outcome : NULL; }
/* PoP: kdb_run_summary @ hermes_cli/kanban_db.py:summary */
const char *kdb_run_summary(const kanban_run_t *r)   { return r ? r->summary : NULL; }
/* PoP: kdb_run_error @ hermes_cli/kanban_db.py:error */
const char *kdb_run_error(const kanban_run_t *r)     { return r ? r->error : NULL; }
/* PoP: kdb_run_id @ hermes_cli/kanban_db.py:id */
long        kdb_run_id(const kanban_run_t *r)        { return r ? r->id : 0; }
/* PoP: kdb_run_status @ hermes_cli/kanban_db.py:status */
const char *kdb_run_status(const kanban_run_t *r)   { return r ? r->status : NULL; }
/* PoP: kdb_run_profile @ hermes_cli/kanban_db.py:profile */
const char *kdb_run_profile(const kanban_run_t *r)  { return r ? r->profile : NULL; }
/* PoP: kdb_run_started_at @ hermes_cli/kanban_db.py:started_at */
long        kdb_run_started_at(const kanban_run_t *r){ return r ? r->started_at : 0; }
/* PoP: kdb_run_ended_at @ hermes_cli/kanban_db.py:ended_at */
long        kdb_run_ended_at(const kanban_run_t *r)  { return r ? r->ended_at : 0; }

/* ---- Comment ---- */

kanban_comment_t *kdb_comment_from_row(sqlite3_stmt *st)
{
    kanban_comment_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->id         = col_long(st, "id", 0);
    c->task_id    = col_text(st, "task_id");
    c->author     = col_text(st, "author");
    c->body       = col_text(st, "body");
    c->created_at = col_long(st, "created_at", 0);
    return c;
}

void kdb_comment_free(kanban_comment_t *c)
{
    if (!c) return;
    free(c->task_id); free(c->author); free(c->body); free(c);
}

/* PoP: kdb_comment_author @ hermes_cli/kanban_db.py:author */
const char *kdb_comment_author(const kanban_comment_t *c){ return c ? c->author : NULL; }
/* PoP: kdb_comment_body @ hermes_cli/kanban_db.py:body */
const char *kdb_comment_body(const kanban_comment_t *c){ return c ? c->body : NULL; }
/* PoP: kdb_comment_created_at @ hermes_cli/kanban_db.py:created_at */
long        kdb_comment_created_at(const kanban_comment_t *c){ return c ? c->created_at : 0; }

/* ---- Attachment ---- */

kanban_attach_t *kdb_attach_from_row(sqlite3_stmt *st)
{
    kanban_attach_t *a = calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->id            = col_long(st, "id", 0);
    a->task_id       = col_text(st, "task_id");
    a->filename      = col_text(st, "filename");
    a->stored_path   = col_text(st, "stored_path");
    a->content_type  = col_text(st, "content_type");
    a->uploaded_by   = col_text(st, "uploaded_by");
    a->size          = col_long(st, "size", 0);
    a->created_at    = col_long(st, "created_at", 0);
    return a;
}

void kdb_attach_free(kanban_attach_t *a)
{
    if (!a) return;
    free(a->task_id); free(a->filename); free(a->stored_path);
    free(a->content_type); free(a->uploaded_by); free(a);
}

const char *kdb_attach_filename(const kanban_attach_t *a){ return a ? a->filename : NULL; }
const char *kdb_attach_stored_path(const kanban_attach_t *a){ return a ? a->stored_path : NULL; }
const char *kdb_attach_content_type(const kanban_attach_t *a){ return a ? a->content_type : NULL; }
long        kdb_attach_size(const kanban_attach_t *a){ return a ? a->size : 0; }

/* ---- Event ---- */

kanban_event_t *kdb_event_from_row(sqlite3_stmt *st)
{
    kanban_event_t *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->id            = col_long(st, "id", 0);
    e->task_id       = col_text(st, "task_id");
    e->kind          = col_text(st, "kind");
    e->payload_json  = col_text(st, "payload");
    e->created_at    = col_long(st, "created_at", 0);
    e->run_id        = col_long(st, "run_id", -1);
    return e;
}

void kdb_event_free(kanban_event_t *e)
{
    if (!e) return;
    free(e->task_id); free(e->kind); free(e->payload_json); free(e);
}

/* PoP: kdb_event_kind @ hermes_cli/kanban_db.py:kind */
const char *kdb_event_kind(const kanban_event_t *e){ return e ? e->kind : NULL; }
/* PoP: kdb_event_payload_json @ hermes_cli/kanban_db.py:payload */
const char *kdb_event_payload_json(const kanban_event_t *e){ return e ? e->payload_json : NULL; }
/* PoP: kdb_event_id @ hermes_cli/kanban_db.py:id */
long        kdb_event_id(const kanban_event_t *e){ return e ? e->id : 0; }
/* PoP: kdb_event_created_at @ hermes_cli/kanban_db.py:created_at */
long        kdb_event_created_at(const kanban_event_t *e){ return e ? e->created_at : 0; }
