/*
 * kanban_format.c — Pure kanban CLI formatting/parsing helpers.
 * See kanban_format.h.
 */

#include "kanban_format.h"

#include <sqlite3.h>
/* kanban_db engine API (avoid pulling the full header: its kanban_task_t
 * conflicts with this unit's local format types) */
struct sqlite3;
extern struct sqlite3 *kdb_connect_path(const char *db_path);
extern char *kanban_db_path(const char *board);
extern char *get_current_board(void);
extern char *kdb_list_boards(int include_archived);
#include "hermes_json.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

/* Declared in src/cli/port_cli_profiles.c (profile_get_active_name @
 * hermes_cli/profiles.py:get_active_profile_name). Used by
 * _profile_author as the fallback after the HERMES_PROFILE* env vars. */
extern char *profile_get_active_name(void);

static char *xstrdup(const char *s){ return s?strdup(s):NULL; }

/* status -> display icon (mirrors _STATUS_ICONS) */
const char *kanban_status_icon(const char *status) {
    if (!status) return "?";
    if (strcmp(status,"todo")==0)     return "\xe2\x97\xbb";   /* ◻ */
    if (strcmp(status,"ready")==0)    return "\xe2\x96\xb6";   /* ▶ */
    if (strcmp(status,"running")==0)  return "\xe2\x97\x8f";   /* ● */
    if (strcmp(status,"scheduled")==0)return "\xe2\x8f\xb1";   /* ⏱ */
    if (strcmp(status,"blocked")==0)  return "\xe2\x8a\x98";   /* ⊘ */
    if (strcmp(status,"done")==0)     return "\xe2\x9c\x93";   /* ✓ */
    if (strcmp(status,"archived")==0) return "\xe2\x80\x94";   /* — */
    return "?";
}

/* PoP: _fmt_ts @ hermes_cli/kanban.py:_fmt_ts */
char *kanban_fmt_ts (long ts) {
    if (!ts) return xstrdup("");
    struct tm tm;
    if (localtime_r(&ts, &tm) == NULL) return xstrdup("");
    char buf[32];
    if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm) == 0) return xstrdup("");
    return xstrdup(buf);
}

/* PoP: _fmt_task_line @ hermes_cli/kanban.py:_fmt_task_line */
char *kanban_fmt_task_line (const kanban_task_t *t) {
    const char *icon = kanban_status_icon(t->status);
    const char *assignee = (t->assignee && *t->assignee) ? t->assignee : "(unassigned)";
    /* tenant display: " [tenant]" when present (mirrors Python's
     * f" [{t.tenant}]"), else "" — note the leading space. */
    char tenant_disp[256] = "";
    if (t->tenant && *t->tenant) snprintf(tenant_disp, sizeof(tenant_disp), " [%s]", t->tenant);
    /* icon + " " + id + "  " + status(left 8) + "  " + assignee(left 20) + tenant + "  " + title */
    size_t cap = 1024;
    char *out = (char*)malloc(cap);
    int n = snprintf(out, cap, "%s %s  %-8s  %-20s%s  %s",
                     icon,
                     t->id ? t->id : "",
                     t->status ? t->status : "",
                     assignee,
                     tenant_disp,
                     t->title ? t->title : "");
    if (n >= (int)cap){ cap = n+1; out = realloc(out, cap); snprintf(out, cap, "%s %s  %-8s  %-20s%s  %s",
                     icon, t->id?t->id:"", t->status?t->status:"", assignee, tenant_disp, t->title?t->title:""); }
    return out;
}


char *kanban_task_to_json(const kanban_task_t *t) {
    /* growable json builder */
    size_t cap = 1024, pos = 0;
    char *out = (char*)malloc(cap);
    out[pos++]='{';
    #define JSB_ENSURE(extra) do { \
        while (pos + (extra) + 1 >= cap) { cap *= 2; out = realloc(out, cap); } \
    } while(0)
    #define JSB_STR(k, v) do { \
        const char *vv=(v)?(v):""; \
        size_t kl=strlen(k), vl=strlen(vv); \
        JSB_ENSURE(kl + vl*2 + 8); \
        memcpy(out+pos, "\"", 1); memcpy(out+pos+1, k, kl); out[pos+1+kl]='\0'; \
        pos += 1 + kl; \
        out[pos++]='"'; out[pos++]=':'; out[pos++]=' '; out[pos++]='"'; \
        for (const char *pp=vv; *pp; pp++){ \
            if (*pp=='"'||*pp=='\\'){ JSB_ENSURE(2); out[pos++]='\\'; out[pos++]=*pp; } \
            else if (*pp=='\n'){ JSB_ENSURE(2); out[pos++]='\\'; out[pos++]='n'; } \
            else { JSB_ENSURE(1); out[pos++]=*pp; } \
        } \
        out[pos++]='"'; out[pos++]=','; \
    } while(0)
    #define JSB_INT(k, v) do { \
        char b[32]; snprintf(b,sizeof(b),"%ld",(long)(v)); \
        size_t kl=strlen(k), bl=strlen(b); \
        JSB_ENSURE(kl + bl + 8); \
        out[pos++]='"'; memcpy(out+pos,k,kl); pos+=kl; \
        out[pos++]='"'; out[pos++]=':'; out[pos++]=' '; \
        memcpy(out+pos,b,bl); pos+=bl; out[pos++]=','; \
    } while(0)

    JSB_STR("id", t->id);
    JSB_STR("title", t->title);
    JSB_STR("body", t->body);
    JSB_STR("assignee", t->assignee);
    JSB_STR("status", t->status);
    JSB_STR("priority", t->priority);
    JSB_STR("tenant", t->tenant);
    JSB_STR("workspace_kind", t->workspace_kind);
    JSB_STR("workspace_path", t->workspace_path);
    JSB_STR("branch_name", t->branch_name);
    JSB_STR("project_id", t->project_id);
    JSB_STR("created_by", t->created_by);
    JSB_INT("created_at", t->created_at);
    JSB_INT("started_at", t->started_at);
    JSB_INT("completed_at", t->completed_at);
    JSB_STR("result", t->result);
    JSB_INT("max_retries", t->max_retries);
    JSB_STR("session_id", t->session_id);
    JSB_STR("workflow_template_id", t->workflow_template_id);
    JSB_STR("current_step_key", t->current_step_key);
    /* skills array */
    { JSB_ENSURE(16); const char *sk_pre="\"skills\": ["; size_t sk_pre_len=strlen(sk_pre); JSB_ENSURE(sk_pre_len); memcpy(out+pos,sk_pre,sk_pre_len); pos+=sk_pre_len;
      for (int i=0;i<t->skills_n;i++){
          JSB_ENSURE(strlen(t->skills[i])*2 + 4);
          out[pos++]='"';
          for (const char *pp=t->skills[i]; *pp; pp++){ if(*pp=='"'||*pp=='\\'){out[pos++]='\\';} out[pos++]=*pp; }
          out[pos++]='"';
          if (i+1 < t->skills_n) out[pos++]=',';
      }
      out[pos++]=' '; out[pos++]=']'; out[pos++]=',';
    }

    #undef JSB_STR
    #undef JSB_INT
    #undef JSB_ENSURE
    if (pos>0 && out[pos-1]==',') pos--;
    while (pos + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
    out[pos++]='}'; out[pos]='\0';
    return out;
}

/* PoP: _run_state_kwargs @ hermes_cli/kanban.py:_run_state_kwargs */
char *kanban_run_state_kwargs (const char *state_type, const char *state_name) {
    bool st = state_type && *state_type;
    bool sn = state_name && *state_name;
    if (st != sn) return NULL;            /* mismatched -> invalid */
    if (!st) return xstrdup("{}");
    size_t cap = 64 + strlen(state_type) + strlen(state_name);
    char *out = (char*)malloc(cap);
    snprintf(out, cap, "{\"state_type\": \"%s\", \"state_name\": \"%s\"}", state_type, state_name);
    return out;
}

/* PoP: _parse_workspace_flag @ hermes_cli/kanban.py:_parse_workspace_flag */
int kanban_parse_workspace_flag (const char *value, char **out_kind, char **out_path, char **errmsg) {
    *out_kind = NULL; *out_path = NULL; *errmsg = NULL;
    if (!value || !*value) { *out_kind = xstrdup("scratch"); return 0; }
    char *v = xstrdup(value);
    char *p = v; while (*p==' '||*p=='\t') p++;
    /* trim trailing */
    size_t L = strlen(p); while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]='\0';
    if (strcmp(p,"scratch")==0 || strcmp(p,"worktree")==0) {
        *out_kind = xstrdup(p);
        free(v); return 0;
    }
    struct { const char *pre; const char *kind; } prefixes[] = {
        {"dir:", "dir"}, {"worktree:", "worktree"}
    };
    for (size_t i=0;i<2;i++){
        size_t pl = strlen(prefixes[i].pre);
        if (strncmp(p, prefixes[i].pre, pl)==0){
            char *path = p + pl;
            while (*path==' '||*path=='\t') path++;
            if (!*path){ *errmsg = xstrdup("--workspace dir: requires a path after the colon"); free(v); return -1; }
            /* expanduser ~ */
            if (path[0]=='~'){
                const char *home = getenv("HOME");
                if (!home) home="";
                size_t hl=strlen(home);
                char *e=(char*)malloc(hl+strlen(path)+1);
                if (path[1]=='/'||path[1]=='\0') snprintf(e,hl+strlen(path)+1,"%s%s",home,path+1);
                else snprintf(e,hl+strlen(path)+1,"%s/%s",home,path+1);
                *out_path = e;
            } else {
                *out_path = xstrdup(path);
            }
            *out_kind = xstrdup(prefixes[i].kind);
            free(v); return 0;
        }
    }
    { char buf[512]; snprintf(buf, sizeof(buf),
        "unknown --workspace value '%s': use scratch, worktree, worktree:<path>, or dir:<path>",
        value ? value : ""); *errmsg = xstrdup(buf); }
    free(v); return -1;
}

/* PoP: _parse_branch_flag @ hermes_cli/kanban.py:_parse_branch_flag */
char *kanban_parse_branch_flag (const char *value, char **errmsg) {
    *errmsg = NULL;
    if (value == NULL) return NULL;
    char *b = xstrdup(value);
    char *p = b; while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p); while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]='\0';
    if (!*p){ *errmsg = xstrdup("--branch requires a non-empty name"); free(b); return NULL; }
    if (p[0]=='-'){ *errmsg = xstrdup("--branch must not start with '-'"); free(b); return NULL; }
    for (char *q=p; *q; q++) if (isspace((unsigned char)*q)){ *errmsg = xstrdup("--branch must not contain whitespace"); free(b); return NULL; }
    char *res = xstrdup(p);
    free(b);
    return res;
}

/* PoP: _parse_duration @ hermes_cli/kanban.py:_parse_duration */
long kanban_parse_duration(const char *value) {
    if (value == NULL || value[0]=='\0') return -2;
    char *s = xstrdup(value);
    char *p = s; while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p); while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]='\0';
    for (char *q=p; *q; q++) *q = (char)tolower((unsigned char)*q);
    /* bare integer */
    char *end=NULL;
    long bare = strtol(p, &end, 10);
    if (*end=='\0') { free(s); return bare; }
    /* suffixed */
    static const char *units = "smhd";
    static const long mult[] = {1,60,3600,86400};
    size_t n = strlen(p);
    char last = p[n-1];
    for (int i=0;i<4;i++){
        if (last==units[i]){
            char *num=p; num[n-1]='\0';
            char *ne=NULL; double d = strtod(num,&ne);
            if (*ne!='\0'){ free(s); return -1; }
            free(s); return (long)(d * mult[i]);
        }
    }
    free(s); return -1;
}

void kanban_task_free(kanban_task_t *t) {
    if (!t) return;
    free(t->id); free(t->title); free(t->body); free(t->assignee);
    free(t->status); free(t->priority); free(t->tenant);
    free(t->workspace_kind); free(t->workspace_path); free(t->branch_name);
    free(t->project_id); free(t->created_by); free(t->result);
    free(t->session_id); free(t->workflow_template_id); free(t->current_step_key);
    free(t);
}

/* PoP: _profile_author @ hermes_cli/kanban.py:_profile_author */
char *kanban_profile_author (void) {
    const char *v = getenv("HERMES_PROFILE_NAME");
    if (v && v[0]) return strdup(v);
    v = getenv("HERMES_PROFILE");
    if (v && v[0]) return strdup(v);
    /* Python falls back to the active profile name (hermes_cli.profiles
     * .get_active_profile_name()) before the literal "user". Mirror that. */
    char *active = profile_get_active_name();
    if (active) return active;
    return strdup("user");
}

/* PoP: _worker_run_id_for @ hermes_cli/kanban.py:_worker_run_id_for */
long kanban_worker_run_id_for (const char *task_id) {
    const char *env_task = getenv("HERMES_KANBAN_TASK");
    if (!env_task || strcmp(env_task, task_id ? task_id : "") != 0) return -1;
    const char *raw = getenv("HERMES_KANBAN_RUN_ID");
    if (!raw || !raw[0]) return -1;
    char *end = NULL;
    long v = strtol(raw, &end, 10);
    if (*end != '\0') return -1;
    return v;
}

/* ── boards summary helpers (v696 parity batch) ────────────────────── */

/* PoP: kbn_board_task_counts @ hermes_cli/kanban.py:_board_task_counts */
json_t *kbn_board_task_counts(const char *slug) {
    /* {status: count} for a board via GROUP BY on the tasks table. Safe on
     * an empty/missing DB (returns {} like Python). */
    json_t *out = json_object();
    if (!out || !slug || !*slug) return out;

    char *db_path = kanban_db_path(slug);
    if (!db_path) return out;
    struct stat st;
    if (stat(db_path, &st) != 0) { free(db_path); return out; }

    sqlite3 *db = kdb_connect_path(db_path);
    free(db_path);
    if (!db) return out;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT status, COUNT(*) AS n FROM tasks GROUP BY status", -1,
                           &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *status = sqlite3_column_text(stmt, 0);
            int n = sqlite3_column_int(stmt, 1);
            if (status)
                json_set(out, (const char *)status, json_int((double)n));
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return out;
}

/* PoP: kbn_cmd_boards_list @ hermes_cli/kanban.py:_cmd_boards_list */
json_t *kbn_cmd_boards_list(int include_archived) {
    /* Enrich each board entry with is_current + per-status counts + total.
     * Returns [{...board..., "is_current":bool,"counts":{...},"total":N}]. */
    char *boards_json = kdb_list_boards(include_archived);
    json_t *boards = boards_json ? json_parse(boards_json, NULL) : NULL;
    free(boards_json);
    if (!boards || !json_is_array(boards)) {
        if (boards) json_free(boards);
        return json_array();
    }
    char *current = get_current_board();

    size_t n = json_array_size(boards);
    for (size_t i = 0; i < n; i++) {
        json_t *b = json_array_get(boards, i);
        if (!b || !json_is_object(b)) continue;
        const char *slug = json_string_value(json_object_get(b, "slug"));
        bool is_current = current && slug && strcmp(current, slug) == 0;
        json_set(b, "is_current", json_new_bool(is_current));
        json_t *counts = kbn_board_task_counts(slug);
        long total = 0;
        if (!counts) counts = json_object();
        for (size_t k = 0; k < counts->c.count; k++) {
            const json_t *v = counts->c.items[k];
            if (v && v->type == JSON_NUMBER) total += (long)v->num_val;
        }
        json_set(b, "counts", counts);   /* steals */
        json_set(b, "total", json_int((double)total));
    }
    free(current);
    return boards;
}
