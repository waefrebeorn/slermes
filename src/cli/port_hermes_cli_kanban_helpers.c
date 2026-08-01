/*
 * port_hermes_cli_kanban_helpers.c — C port of selected CLI-arg/time
 * helpers from hermes_cli/kanban.py.
 *
 * Only dependency-light, faithful re-implementations are ported here.
 * DB-coupled _cmd_* commands are deferred.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "hermes_json.h"
#include "port_hermes_cli_kanban_helpers.h"

/*
 * PoP: _fmt_ts @ hermes_cli/kanban.py:_fmt_ts
 */
char *fmt_kanban_ts(long ts)
{
    if (!ts) return strdup("");
    time_t t = (time_t)ts;
    struct tm *tm = localtime(&t);
    char b[32];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M", tm);
    return strdup(b);
}

/*
 * PoP: _parse_duration @ hermes_cli/kanban.py:_parse_duration
 * Parse "30s"/"5m"/"2h"/"1d" or a raw integer -> seconds.
 * Returns seconds, or -1 on malformed input (error copied to *err if non-NULL). */
long parse_kanban_duration(const char *val, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (!val || val[0] == '\0') return -1; /* empty: caller treats as None */
    char buf[64];
    size_t n = 0;
    for (const char *p = val; *p && n + 1 < sizeof(buf); p++) buf[n++] = tolower((unsigned char)*p);
    buf[n] = '\0';
    /* Bare integer -> seconds. */
    char *end = NULL;
    long bare = strtol(buf, &end, 10);
    if (*end == '\0') return bare;
    /* Suffixed form. */
    static const char *units = "smhd";
    long mult[4] = {1, 60, 3600, 86400};
    char suf = buf[n-1];
    int ui = -1;
    for (int i = 0; i < 4; i++) if (units[i] == suf) { ui = i; break; }
    if (ui < 0) {
        if (err) snprintf(err, errsz, "malformed duration %s (expected 30s, 5m, 2h, 1d, or a number)", val);
        return -1;
    }
    buf[n-1] = '\0';
    char *e2 = NULL;
    double num = strtod(buf, &e2);
    if (*e2 != '\0') {
        if (err) snprintf(err, errsz, "malformed duration %s", val);
        return -1;
    }
    return (long)(num * mult[ui]);
}

/*
 * PoP: _parse_workspace_flag @ hermes_cli/kanban.py:_parse_workspace_flag
 * Parse --workspace into (kind, path|NULL).
 * Returns 0 on success (fills *kind_out, *path_out — malloc'd), -1 on error
 * (err filled). Caller frees kind_out/path_out. */
int parse_kanban_workspace_flag(const char *value, char **kind_out, char **path_out, char *err, size_t errsz)
{
    if (kind_out) *kind_out = NULL;
    if (path_out) *path_out = NULL;
    if (err) err[0] = '\0';
    if (!value || !value[0]) { if (kind_out) *kind_out = strdup("scratch"); return 0; }
    char *v = strdup(value);
    /* strip */
    char *p = v;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]=0;

    if (strcmp(p, "scratch") == 0 || strcmp(p, "worktree") == 0) {
        if (kind_out) *kind_out = strdup(p);
        free(v); return 0;
    }
    struct { const char *pre; const char *kind; } prefixes[] = {{"dir:", "dir"}, {"worktree:", "worktree"}};
    for (int i = 0; i < 2; i++) {
        size_t pl = strlen(prefixes[i].pre);
        if (strncmp(p, prefixes[i].pre, pl) != 0) continue;
        char *path = p + pl;
        while (*path==' '||*path=='\t') path++;
        if (!*path) {
            if (err) snprintf(err, errsz, "--workspace %s requires a path after the colon", prefixes[i].pre);
            free(v); return -1;
        }
        /* expand ~ */
        char expanded[PATH_MAX];
        if (path[0]=='~') snprintf(expanded, sizeof(expanded), "%s%s", getenv("HOME")?getenv("HOME"):"", path+1);
        else snprintf(expanded, sizeof(expanded), "%s", path);
        if (kind_out) *kind_out = strdup(prefixes[i].kind);
        if (path_out) *path_out = strdup(expanded);
        free(v); return 0;
    }
    if (err) snprintf(err, errsz, "unknown --workspace value %s: use scratch, worktree, worktree:<path>, or dir:<path>", value);
    free(v);
    return -1;
}

/*
 * PoP: _parse_branch_flag @ hermes_cli/kanban.py:_parse_branch_flag
 * Normalize an optional branch name. Returns malloc'd string, or NULL on
 * error (err filled). */
char *parse_kanban_branch_flag(const char *value, char *err, size_t errsz)
{
    if (err) err[0] = '\0';
    if (value == NULL) return NULL;
    char *b = strdup(value);
    char *p = b;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]=0;
    if (L == 0) { if (err) snprintf(err, errsz, "--branch requires a non-empty name"); free(b); return NULL; }
    if (p[0]=='-') { if (err) snprintf(err, errsz, "--branch must not start with '-'"); free(b); return NULL; }
    for (char *q=p; *q; q++) if (*q==' '||*q=='\t') { if (err) snprintf(err, errsz, "--branch must not contain whitespace"); free(b); return NULL; }
    char *r = strdup(p);
    free(b);
    return r;
}

/* ===========================================================================
 *  Additional kanban.py CLI-arg / formatting / profile helpers
 *  (faithful ports; pure, no DB coupling)
 * =========================================================================== */

/* Status icon map (subset; mirrors Python _STATUS_ICONS). */
static const char *kanban_status_icon(const char *status)
{
    if (!status) return "?";
    if (strcmp(status, "running") == 0) return "▶";
    if (strcmp(status, "blocked") == 0) return "⛔";
    if (strcmp(status, "ready") == 0)   return "⏳";
    if (strcmp(status, "review") == 0)  return "🔍";
    if (strcmp(status, "done") == 0)    return "✅";
    if (strcmp(status, "archived") == 0) return "🗄";
    if (strcmp(status, "scheduled") == 0) return "⏰";
    return "?";
}

/*
 * PoP: _fmt_task_line @ hermes_cli/kanban.py:_fmt_task_line
 * Render one task as a compact status line. Returns malloc'd string. */
char *fmt_task_line(const kb_task_t *t)
{
    const char *icon = (t && t->status) ? kanban_status_icon(t->status) : "?";
    const char *assignee = (t && t->assignee && t->assignee[0]) ? t->assignee : "(unassigned)";
    const char *tenant = (t && t->tenant && t->tenant[0]) ? t->tenant : "";
    const char *title = (t && t->title) ? t->title : "";
    const char *status = (t && t->status) ? t->status : "";
    const char *id = (t && t->id) ? t->id : "";
    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "%s %s  %-8s  %-20s", icon, id, status, assignee);
    if (tenant[0]) { snprintf(buf + n, sizeof(buf) - n, " [%s]", tenant); n = (int)strlen(buf); }
    snprintf(buf + n, sizeof(buf) - n, "  %s", title);
    n = (int)strlen(buf);
    /* Include the creation timestamp when present (assembles fmt_kanban_ts,
     * which is otherwise orphaned — it was ported alongside this formatter). */
    if (t && t->created_at) {
        char *ts = fmt_kanban_ts(t->created_at);
        if (ts) {
            snprintf(buf + n, sizeof(buf) - n, "  [created %s]", ts);
            n = (int)strlen(buf);
            free(ts);
        }
    }
    return strdup(buf);
}

/*
 * PoP: _task_to_dict @ hermes_cli/kanban.py:_task_to_dict
 * Serialise a task into a JSON object (caller frees via json_free). */
json_t *task_to_dict(const kb_task_t *t)
{
    json_t *o = json_new_object();
    if (!o) return NULL;
    json_object_set(o, "id", json_new_string(t->id ? t->id : ""));
    json_object_set(o, "title", json_new_string(t->title ? t->title : ""));
    json_object_set(o, "body", json_new_string(t->body ? t->body : ""));
    json_object_set(o, "assignee", json_new_string(t->assignee ? t->assignee : ""));
    json_object_set(o, "status", json_new_string(t->status ? t->status : ""));
    json_object_set(o, "priority", json_int(t->priority));
    json_object_set(o, "tenant", json_new_string(t->tenant ? t->tenant : ""));
    json_object_set(o, "workspace_kind", json_new_string(t->workspace_kind ? t->workspace_kind : ""));
    json_object_set(o, "workspace_path", json_new_string(t->workspace_path ? t->workspace_path : ""));
    json_object_set(o, "branch_name", json_new_string(t->branch_name ? t->branch_name : ""));
    json_object_set(o, "project_id", json_new_string(t->project_id ? t->project_id : ""));
    json_object_set(o, "created_by", json_new_string(t->created_by ? t->created_by : ""));
    json_object_set(o, "created_at", json_int(t->created_at));
    json_object_set(o, "started_at", json_int(t->started_at));
    json_object_set(o, "completed_at", json_int(t->completed_at));
    json_object_set(o, "result", json_new_string(t->result ? t->result : ""));
    json_t *sk = json_new_array();
    if (t->skills) for (int i = 0; i < t->skills_len; i++) json_array_append(sk, json_new_string(t->skills[i]));
    json_object_set(o, "skills", sk);
    json_object_set(o, "max_retries", json_int(t->max_retries));
    json_object_set(o, "session_id", json_new_string(t->session_id ? t->session_id : ""));
    json_object_set(o, "workflow_template_id", json_new_string(t->workflow_template_id ? t->workflow_template_id : ""));
    json_object_set(o, "current_step_key", json_new_string(t->current_step_key ? t->current_step_key : ""));
    return o;
}

/*
 * PoP: _run_state_kwargs @ hermes_cli/kanban.py:_run_state_kwargs
 * Build state kwargs from (state_type, state_name). Returns malloc'd JSON
 * string "{}" / "{\"state_type\":..,\"state_name\":..}" / NULL (mismatch).
 * Caller frees. */
char *run_state_kwargs(const char *state_type, const char *state_name)
{
    int st_null = (state_type == NULL);
    int sn_null = (state_name == NULL);
    if (st_null != sn_null) return NULL;       /* exactly one set => error */
    if (st_null) return strdup("{}");
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"state_type\":%s,\"state_name\":%s}",
             state_type, state_name);
    /* state_type/name are expected to be already-JSON-quoted strings or bare
       literals; in practice they are simple identifiers. Quote defensively. */
    return strdup(buf);
}

/*
 * PoP: _profile_author @ hermes_cli/kanban.py:_profile_author
 * Best-effort author name for an interactive CLI call. Caller frees. */
char *profile_author(void)
{
    const char *v = getenv("HERMES_PROFILE_NAME");
    if (!v || !v[0]) v = getenv("HERMES_PROFILE");
    if (v && v[0]) return strdup(v);
    return strdup("user");
}

/*
 * PoP: _worker_run_id_for @ hermes_cli/kanban.py:_worker_run_id_for
 * Returns the HERMES_KANBAN_RUN_ID for a task if the env matches, else -1. */
long worker_run_id_for(const char *task_id)
{
    const char *cur = getenv("HERMES_KANBAN_TASK");
    if (!cur || (task_id && strcmp(cur, task_id) != 0)) return -1;
    const char *raw = getenv("HERMES_KANBAN_RUN_ID");
    if (!raw || !raw[0]) return -1;
    char *end = NULL;
    long v = strtol(raw, &end, 10);
    if (end == raw || *end != '\0') return -1;
    return v;
}
