/*
 * kanban_handlers.c — Kanban tool handlers + registration.
 *
 * This is the tool-facing half of what used to be the file-based monolith
 * src/tools/kanban.c (972 lines). It is split from the storage backend
 * (now src/tools/kanban_adapter.c, which delegates to the sqlite engine in
 * kanban_db.h). The handlers here contain NO storage logic — they parse args,
 * apply the worker/orchestrator scoping rules, and call the adapter API.
 *
 * Minimal includes. No god header.
 *
 * PoP: faithful port of tools/kanban_tools.py handlers.
 */

#include "hermes_kanban.h"
#include "hermes_json.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "kanban_db.h"
#include "base64.h"
#include "libhttp/http.h"

/* src/hermes_cli/kanban_db.c -- the review transitions; no header declares them. */
int kanban_request_changes(const char *task_id, const char *reason,
                           char *out_impl, size_t out_sz);
int kanban_request_review(sqlite3 *conn, const char *task_id, const char *summary,
                          const char *metadata_json, const char *reviewer_in,
                          long expected_run_id, bool force,
                          char *out_reason, size_t reason_sz);

#include <sys/stat.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_PATH 4096
#define KANBAN_LIST_LIMIT_DEFAULT 50
#define KANBAN_LIST_LIMIT_MAX 200

/* ---- scoping helpers (ported from kanban_tools.py) ---- */

static bool kanban_mode(void) {
    return getenv("HERMES_KANBAN_TASK") != NULL;
}
static bool kanban_orchestrator(void) {
    return getenv("HERMES_KANBAN_TASK") == NULL;
}
static const char *default_task_id(const char *arg) {
    if (arg && *arg) return arg;
    return getenv("HERMES_KANBAN_TASK");
}
static const char *enforce_ownership(const char *tid) {
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (!env_tid) return NULL;
    if (strcmp(tid, env_tid) != 0) return "worker is scoped to a different task";
    return NULL;
}
static const char *require_orchestrator(const char *tool_name) {
    (void)tool_name;
    if (kanban_mode()) return "orchestrator-only tool";
    return NULL;
}

/* ---- handlers ---- */

/* PoP: handle_show @ tools/kanban_tools.py:_handle_show */
static char *handle_show(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) {
        json_free(args);
        return strdup("{\"error\":\"task_id is required (or set HERMES_KANBAN_TASK)\"}");
    }
    json_t *task = kanban_show_task(tid);   /* upstream shape: {task, parents, unsatisfied_parents, ...} */
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        json_free(args);
        return strdup(err);
    }
    char *out = json_serialize(task);
    json_free(task);
    json_free(args);
    return out ? out : strdup("{\"error\":\"OOM\"}");
}

static char *handle_list(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *guard = require_orchestrator("kanban_list");
    if (guard) return strdup("{\"error\":\"orchestrator-only\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *assignee = json_get_str(args, "assignee", "");
    const char *status   = json_get_str(args, "status", "");
    const char *tenant   = json_get_str(args, "tenant", "");
    bool include_archived = json_get_bool(args, "include_archived", false);
    /* limit is the caller's number and stays that way. kanban_all_task_ids() writes the number of
     * ids it found back through its pointer, so passing &limit made every reply report
     * "limit": <count> -- a request for 50 answered limit 1. Ask for one more than the caller wants,
     * the way upstream does, so truncation is visible without dumping the board. */
    long limit_arg = (long)json_get_num(args, "limit", KANBAN_LIST_LIMIT_DEFAULT);
    if (limit_arg < 1 || limit_arg > KANBAN_LIST_LIMIT_MAX) {
        json_free(args);
        char why[128];
        snprintf(why, sizeof(why),
                 "{\"error\":\"limit must be between 1 and %d\"}", KANBAN_LIST_LIMIT_MAX);
        return strdup(why);
    }
    int limit = (int)limit_arg;
    int fetch = limit + 1;

    /* Dependencies cleared since the last dispatcher tick become ready before we list. */
    kanban_recompute_ready();

    /* The engine's list covers all boards; for the tool we filter by the
     * default board via the adapter. We gather via kanban_read_task per id. */
    char **ids = kanban_all_task_ids(&fetch);  /* adapter helper */
    json_t *tasks = json_array();
    int count = 0;
    bool truncated = false;
    for (int i = 0; ids && ids[i]; i++) {
        json_t *t = kanban_read_task(ids[i]);
        if (!t) continue;
        const char *t_status = json_get_str(t, "status", "");
        const char *t_assignee = json_get_str(t, "assignee", "");
        const char *t_tenant = json_get_str(t, "tenant", "");
        bool skip = false;
        if (*status && strcmp(t_status, status) != 0) skip = true;
        if (*assignee && strcmp(t_assignee, assignee) != 0) skip = true;
        if (*tenant && strcmp(t_tenant, tenant) != 0) skip = true;
        if (!include_archived && strcmp(t_status, "archived") == 0) skip = true;
        if (!skip) {
            if (count >= limit) { truncated = true; json_free(t); break; }
            json_append(tasks, t);
            count++;
        } else json_free(t);
    }
    kanban_all_task_ids_free(ids);

    json_t *result = json_object();
    json_set(result, "tasks", tasks);
    json_set(result, "count", json_number((double)count));
    json_set(result, "limit", json_number((double)limit));
    json_set(result, "truncated", json_bool(truncated));
    if (truncated && limit < KANBAN_LIST_LIMIT_MAX)
        json_set(result, "next_limit", json_number((double)(limit * 2 > KANBAN_LIST_LIMIT_MAX
                                                               ? KANBAN_LIST_LIMIT_MAX : limit * 2)));
    else
        json_set(result, "next_limit", json_null());
    char *out = json_serialize(result);
    json_free(result);
    json_free(args);
    return out ? out : strdup("{\"error\":\"OOM\"}");
}

/* PoP: handle_complete @ tools/kanban_tools.py:_handle_complete */
static char *handle_complete(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup("{\"error\":\"ownership violation\"}"); }
    const char *summary = json_get_str(args, "summary", "");
    const char *result_str = json_get_str(args, "result", "");
    const char *metadata = json_get_str(args, "metadata", "");
    if (!*summary && !*result_str) {
        json_free(args);
        return strdup("{\"error\":\"provide at least one of: summary or result\"}");
    }
    bool ok = kanban_complete_task(tid, *summary ? summary : NULL,
                                   *result_str ? result_str : NULL,
                                   *metadata ? metadata : NULL);
    json_free(args);
    if (!ok) { char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    return strdup("{\"ok\":true,\"status\":\"done\"}");
}

/* PoP: handle_block @ tools/kanban_tools.py:_handle_block */
static char *handle_block(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    const char *reason = json_get_str(args, "reason", "");
    bool sticky = json_get_bool(args, "sticky", false);
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup("{\"error\":\"ownership violation\"}"); }
    json_t *t = kanban_read_task(tid);
    if (!t) { json_free(args); char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    /* Upstream validates `kind` against kanban_db.VALID_BLOCK_KINDS and has no `sticky` argument at
     * all: a block reason recorded by an explicit kanban_block is what makes a block survive
     * auto-promotion, and a re-kind to needs_input is how the engine marks it. This used to pass the
     * literal "sticky" as the block kind, so the store held a kind that is not in the enum and the
     * real `kind` argument was never read. */
    const char *kind = json_get_str(args, "kind", "");
    static const char *VALID_KINDS[] = {"dependency", "needs_input", "capability", "transient", NULL};
    if (*kind) {
        bool known = false;
        for (int i = 0; VALID_KINDS[i]; i++)
            if (strcmp(kind, VALID_KINDS[i]) == 0) { known = true; break; }
        if (!known) {
            json_free(t);
            json_free(args);
            return strdup("{\"error\":\"kind must be one of: capability, dependency, needs_input, transient (or omit it)\"}");
        }
    } else if (sticky) {
        kind = "needs_input";   /* the legacy sticky flag maps to the kind upstream re-kinds to */
    }
    bool ok = kanban_block_task(tid, reason, *kind ? kind : NULL) ? true : false;
    json_free(t);
    json_free(args);
    if (!ok) return strdup("{\"error\":\"block failed\"}");
    char r[256]; snprintf(r,sizeof(r),"{\"ok\":true,\"status\":\"blocked\",\"sticky\":%s}", sticky?"true":"false");
    return strdup(r);
}

/* PoP: handle_heartbeat @ tools/kanban_tools.py:_handle_heartbeat */
static char *handle_heartbeat(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup("{\"error\":\"ownership violation\"}"); }
    json_t *t = kanban_read_task(tid);
    if (!t) { json_free(args); char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    kanban_heartbeat(tid);
    json_free(t);
    json_free(args);
    return strdup("{\"ok\":true}");
}

/* PoP: handle_comment @ tools/kanban_tools.py:_handle_comment */
static char *handle_comment(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    const char *body = json_get_str(args, "body", "");
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    if (!*body) { json_free(args); return strdup("{\"error\":\"body is required\"}"); }
    bool ok = kanban_add_comment(tid, "worker", body);
    json_free(args);
    if (!ok) { char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); return strdup(err); }
    return strdup("{\"ok\":true}");
}

/* PoP: handle_create @ tools/kanban_tools.py:_handle_create */
static char *handle_create(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!kanban_orchestrator())
        return strdup("{\"error\":\"Kanban orchestration blocked: sub-agent scope (HERMES_KANBAN_TASK set)\"}");
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    kanban_task_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.title = json_get_str(args, "title", "");
    spec.assignee = json_get_str(args, "assignee", "");
    /* Python's kanban_create spells this field "description"; callers use both. Reading only "body"
     * silently discarded the description of every task created by an upstream-shaped caller. */
    spec.body = json_get_str(args, "body", "");
    if (!*spec.body) spec.body = json_get_str(args, "description", "");
    spec.priority = (int)json_get_num(args, "priority", 0);
    spec.tenant = json_get_str(args, "tenant", "");
    spec.workspace_kind = json_get_str(args, "workspace_kind", "");
    spec.workspace_path = json_get_str(args, "workspace_path", "");
    spec.triage = json_get_bool(args, "triage", false);
    spec.initial_status = json_get_str(args, "initial_status", "");
    /* Upstream validates this (kanban_db.py: VALID_INITIAL_STATUSES) and raises; accepting any
     * string here means a typo is silently ignored and the task lands in the default state. */
    if (*spec.initial_status && strcmp(spec.initial_status, "running") != 0 &&
        strcmp(spec.initial_status, "blocked") != 0) {
        json_free(args);
        return strdup("{\"error\":\"initial_status must be one of: blocked, running\"}");
    }
    spec.created_by = getenv("HERMES_PROFILE");
    if (!spec.created_by || !*spec.created_by) spec.created_by = "worker";
    if (!*spec.title || !*spec.assignee) {
        json_free(args);
        return strdup("{\"error\":\"title and assignee are required\"}");
    }
    /* parents: the schema advertises it, the adapter reads spec->parents_json, and nothing ever set
     * it -- so kanban_create(parents=[...]) created an unlinked task and every later
     * unsatisfied_parents/children answer was empty. Upstream links them at create time. */
    char *parents_json = NULL;
    json_t *parr = json_obj_get(args, "parents");
    if (parr && json_len(parr) > 0) {
        parents_json = json_serialize(parr);
        spec.parents_json = parents_json;
    }
    char *new_id = kanban_create_task(&spec);
    free(parents_json);
    json_free(args);
    if (!new_id) return strdup("{\"error\":\"create failed\"}");

    /* Report the status the store actually gave the row. This used to print "running" while the
     * engine stored "ready" -- an answer that contradicted the board it had just written. */
    const char *status = "created";
    char status_buf[32];
    json_t *stored = kanban_read_task(new_id);
    if (stored) {
        const char *st = json_get_str(stored, "status", "");
        if (st && *st) {
            snprintf(status_buf, sizeof(status_buf), "%s", st);
            status = status_buf;
        }
        json_free(stored);
    }
    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"task_id\":\"%s\",\"status\":\"%s\"}",
             new_id, status);
    free(new_id);
    return strdup(out);
}

/* PoP: handle_unblock @ tools/kanban_tools.py:_handle_unblock */
static char *handle_unblock(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *guard = require_orchestrator("kanban_unblock");
    if (guard) return strdup("{\"error\":\"orchestrator-only\"}");
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = json_get_str(args, "task_id", "");
    if (!*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    bool ok = kanban_unblock_task( tid) ? true : false;
    if (!ok) { char err[256]; snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}",tid); json_free(args); return strdup(err); }
    char out[256];
    snprintf(out, sizeof(out), "{\"ok\":true,\"task_id\":\"%s\",\"status\":\"ready\"}", tid);
    json_free(args);                              /* tid points into args */
    return strdup(out);
}

/* PoP: handle_link @ tools/kanban_tools.py:_handle_link */
static char *handle_link(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *parent_id = json_get_str(args, "parent_id", "");
    const char *child_id  = json_get_str(args, "child_id", "");
    if (!*parent_id || !*child_id) {
        json_free(args);
        return strdup("{\"error\":\"both parent_id and child_id are required\"}");
    }
    if (strcmp(parent_id, child_id) == 0) {
        json_free(args); return strdup("{\"error\":\"self-links are not allowed\"}");
    }
    bool ok = kanban_link_tasks(parent_id, child_id);
    /* the failure branch below reads both tasks by id, so keep the strings alive until then */
    char parent_buf[128], child_buf[128];
    snprintf(parent_buf, sizeof(parent_buf), "%s", parent_id);
    snprintf(child_buf, sizeof(child_buf), "%s", child_id);
    parent_id = parent_buf; child_id = child_buf;
    json_free(args);
    if (!ok) {
        json_t *p = kanban_read_task(parent_id);
        json_t *c = kanban_read_task(child_id);
        bool missing = (!p || !c);
        json_free(p); json_free(c);
        if (missing) {
            char err[256];
            snprintf(err,sizeof(err),"{\"error\":\"task %s not found\"}", !p?parent_id:child_id);
            return strdup(err);
        }
        return strdup("{\"error\":\"cycle detected\"}");
    }
    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"parent_id\":\"%s\",\"child_id\":\"%s\"}", parent_id, child_id);
    return strdup(out);
}


/* ==================================================================
 *  Attachments and the review handoff.
 *
 *  The engine (src/hermes_cli/kanban_tasks.c, kanban_db.c) already implements
 *  kdb_add_attachment / kdb_list_attachments / kanban_request_review; these
 *  handlers are the tool-facing half, with the same worker scoping the other
 *  kanban handlers use.
 * ================================================================== */

static char *kb_err(const char *msg) {
    char buf[512];
    snprintf(buf, sizeof buf, "{\"ok\":false,\"error\":\"%s\"}", msg ? msg : "error");
    return strdup(buf);
}

/* PoP: handle_attachments @ tools/kanban_tools.py:_handle_attachments */
static char *handle_attachments(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *tid = default_task_id(args ? json_get_str(args, "task_id", "") : NULL);
    if (!tid || !*tid) { json_free(args); return kb_err("task_id is required (or set HERMES_KANBAN_TASK)"); }
    const char *scope = enforce_ownership(tid);
    if (scope) { json_free(args); return kb_err(scope); }

    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { json_free(args); return kb_err("cannot open the kanban board"); }

    int n = 0;
    kanban_attach_t **list = kdb_list_attachments(conn, tid, &n);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "task_id", json_string(tid));
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        json_t *o = json_object();
        json_set(o, "id", json_number((double)kdb_attach_id(list[i])));
        json_set(o, "filename", json_string(kdb_attach_filename(list[i]) ? kdb_attach_filename(list[i]) : ""));
        json_set(o, "content_type", json_string(kdb_attach_content_type(list[i]) ? kdb_attach_content_type(list[i]) : ""));
        json_set(o, "size", json_number((double)kdb_attach_size(list[i])));
        json_set(o, "stored_path", json_string(kdb_attach_stored_path(list[i]) ? kdb_attach_stored_path(list[i]) : ""));
        json_append(arr, o);
    }
    if (list) kdb_attachment_list_free(list);
    json_set(out, "attachments", arr);
    json_set(out, "count", json_number((double)n));
    kdb_close(conn);
    json_free(args);

    char *txt = json_serialize_pretty(out, JSON_INDENT(2));
    json_free(out);
    return txt ? txt : kb_err("serialize failed");
}

/* Only the leaf of a caller-supplied name is kept: "a/b/report.pdf" -> "report.pdf". */
static const char *leaf_name(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\\\');
    const char *cut = slash > bslash ? slash : bslash;
    return cut ? cut + 1 : path;
}

/* PoP: handle_attach @ tools/kanban_tools.py:_handle_attach */
static char *handle_attach(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *tid = default_task_id(args ? json_get_str(args, "task_id", "") : NULL);
    const char *name = args ? json_get_str(args, "filename", "") : "";
    const char *b64 = args ? json_get_str(args, "content_base64", "") : "";
    const char *ctype = args ? json_get_str(args, "content_type", "") : "";

    if (!tid || !*tid) { json_free(args); return kb_err("task_id is required (or set HERMES_KANBAN_TASK)"); }
    if (!name || !*name) { json_free(args); return kb_err("filename is required"); }
    if (!b64 || !*b64) { json_free(args); return kb_err("content_base64 is required"); }
    const char *scope = enforce_ownership(tid);
    if (scope) { json_free(args); return kb_err(scope); }

    size_t nbytes = 0;
    unsigned char *bytes = base64_decode(b64, &nbytes);
    if (!bytes) { json_free(args); return kb_err("content_base64 is not valid base64"); }
    if (nbytes > 25u * 1024u * 1024u) {
        free(bytes); json_free(args);
        return kb_err("decoded content exceeds the 25 MB attachment cap");
    }

    /* The task id becomes a path component: refuse anything with a separator or a dot-dot. */
    if (!*tid || strchr(tid, '/') || strchr(tid, '\\') || strcmp(tid, "..") == 0) {
        free(bytes); json_free(args); return kb_err("task_id is not a valid path component");
    }
    char home[1024], dir[1200], stored[1600];
    hermes_get_home(home, sizeof home);
    /* mkdir(2) creates one level: build the chain, ignoring EEXIST at each step. */
    snprintf(dir, sizeof dir, "%s/kanban", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/kanban/attachments", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/kanban/attachments/%s", home, tid);
    mkdir(dir, 0755);
    snprintf(stored, sizeof stored, "%s/%s", dir, leaf_name(name));

    FILE *f = fopen(stored, "wb");
    if (!f) {
        char msg[256]; snprintf(msg, sizeof msg, "cannot write attachment: %s", strerror(errno));
        free(bytes); json_free(args); return kb_err(msg);
    }
    size_t wrote = fwrite(bytes, 1, nbytes, f);
    fclose(f);
    free(bytes);
    if (wrote != nbytes) { json_free(args); return kb_err("short write while storing the attachment"); }

    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { json_free(args); return kb_err("cannot open the kanban board"); }
    int rc = kdb_add_attachment(conn, tid, leaf_name(name), stored,
                                ctype && *ctype ? ctype : "application/octet-stream",
                                (long)nbytes, getenv("HERMES_PROFILE") ? getenv("HERMES_PROFILE") : "hermes");
    kdb_close(conn);

    if (rc <= 0) { json_free(args); return kb_err("the board rejected the attachment (unknown task?)"); }

    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "task_id", json_string(tid));
    json_set(out, "attachment_id", json_number((double)rc));
    json_set(out, "filename", json_string(leaf_name(name)));
    json_set(out, "stored_path", json_string(stored));
    json_set(out, "size", json_number((double)nbytes));
    json_free(args);
    char *txt = json_serialize_pretty(out, JSON_INDENT(2));
    json_free(out);
    return txt ? txt : kb_err("serialize failed");
}


/* PoP: handle_attach_url @ tools/kanban_tools.py:_handle_attach_url */
static char *handle_attach_url(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *tid = default_task_id(args ? json_get_str(args, "task_id", "") : NULL);
    const char *url = args ? json_get_str(args, "url", "") : "";
    const char *name = args ? json_get_str(args, "filename", "") : "";
    const char *ctype = args ? json_get_str(args, "content_type", "") : "";

    if (!tid || !*tid) { json_free(args); return kb_err("task_id is required (or set HERMES_KANBAN_TASK)"); }
    if (!url || !*url) { json_free(args); return kb_err("url is required"); }
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        json_free(args); return kb_err("url must be http:// or https://");
    }
    const char *scope = enforce_ownership(tid);
    if (scope) { json_free(args); return kb_err(scope); }
    if (!*tid || strchr(tid, '/') || strchr(tid, '\\') || strcmp(tid, "..") == 0) {
        json_free(args); return kb_err("task_id is not a valid path component");
    }

    http_t *h = http_new(30);
    if (!h) { json_free(args); return kb_err("cannot create the HTTP client"); }
    http_resp_t *resp = http_get(h, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        int st = resp ? resp->status : -1;
        if (resp) http_resp_free(resp);
        http_free(h);
        json_free(args);
        char msg[128]; snprintf(msg, sizeof msg, "failed to fetch the URL (HTTP %d)", st);
        return kb_err(msg);
    }

    /* Default the stored name from the URL's last path segment. */
    char derived[256] = "attachment.bin";
    if (!name || !*name) {
        const char *q = strchr(url, '?');
        size_t ulen = q ? (size_t)(q - url) : strlen(url);
        const char *slash = NULL;
        for (size_t i = 0; i < ulen; i++) if (url[i] == '/') slash = url + i;
        if (slash && slash + 1 < url + ulen) {
            size_t n = ulen - (size_t)(slash + 1 - url);
            if (n >= sizeof derived) n = sizeof derived - 1;
            memcpy(derived, slash + 1, n);
            derived[n] = '\0';
        }
        name = derived;
    }
    if (resp->body_len > 25u * 1024u * 1024u) {
        http_resp_free(resp); http_free(h); json_free(args);
        return kb_err("fetched content exceeds the 25 MB attachment cap");
    }

    char home[1024], dir[1200], stored[1600];
    hermes_get_home(home, sizeof home);
    snprintf(dir, sizeof dir, "%s/kanban", home);              mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/kanban/attachments", home);  mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/kanban/attachments/%s", home, tid); mkdir(dir, 0755);
    snprintf(stored, sizeof stored, "%s/%s", dir, leaf_name(name));

    FILE *f = fopen(stored, "wb");
    if (!f) {
        http_resp_free(resp); http_free(h); json_free(args);
        return kb_err("cannot write the attachment file");
    }
    size_t wrote = fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);
    size_t nbytes = resp->body_len;
    http_resp_free(resp);
    http_free(h);
    if (wrote != nbytes) { json_free(args); return kb_err("short write while storing the attachment"); }

    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { json_free(args); return kb_err("cannot open the kanban board"); }
    int rc = kdb_add_attachment(conn, tid, leaf_name(name), stored,
                                ctype && *ctype ? ctype : "application/octet-stream",
                                (long)nbytes, getenv("HERMES_PROFILE") ? getenv("HERMES_PROFILE") : "hermes");
    kdb_close(conn);
    if (rc <= 0) { json_free(args); return kb_err("the board rejected the attachment (unknown task?)"); }

    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "task_id", json_string(tid));
    json_set(out, "attachment_id", json_number((double)rc));
    json_set(out, "filename", json_string(leaf_name(name)));
    json_set(out, "stored_path", json_string(stored));
    json_set(out, "size", json_number((double)nbytes));
    json_free(args);
    char *txt = json_serialize_pretty(out, JSON_INDENT(2));
    json_free(out);
    return txt ? txt : kb_err("serialize failed");
}


/* PoP: handle_request_changes @ tools/kanban_tools.py:_handle_request_changes */
static char *handle_request_changes(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *tid = default_task_id(args ? json_get_str(args, "task_id", "") : NULL);
    const char *reason = args ? json_get_str(args, "reason", "") : "";

    if (!tid || !*tid) { json_free(args); return kb_err("task_id is required (or set HERMES_KANBAN_TASK)"); }
    if (!reason || !*reason) {
        json_free(args);
        return kb_err("reason is required - describe the changes needed");
    }

    char impl[128] = "";
    int ok = kanban_request_changes(tid, reason, impl, sizeof impl);

    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok != 0));
    json_set(out, "task_id", json_string(tid));
    if (ok) {
        json_set(out, "status", json_string("ready"));
        if (impl[0]) json_set(out, "implementer", json_string(impl));
    } else {
        json_set(out, "error", json_string("could not request changes (task not found, not in review, or reason is blank)"));
    }
    json_free(args);
    char *txt = json_serialize_pretty(out, JSON_INDENT(2));
    json_free(out);
    return txt ? txt : kb_err("serialize failed");
}

/* PoP: handle_request_review @ tools/kanban_tools.py:_handle_request_review */
static char *handle_request_review(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *tid = default_task_id(args ? json_get_str(args, "task_id", "") : NULL);
    const char *summary = args ? json_get_str(args, "summary", "") : "";
    const char *reviewer = args ? json_get_str(args, "reviewer", "") : "";
    const json_t *meta = args ? json_obj_get(args, "metadata") : NULL;

    if (!tid || !*tid) { json_free(args); return kb_err("task_id is required (or set HERMES_KANBAN_TASK)"); }
    if (!summary || !*summary) { json_free(args); return kb_err("summary is required - describe what was implemented and how it was verified"); }
    const char *scope = enforce_ownership(tid);
    if (scope) { json_free(args); return kb_err(scope); }

    char *meta_json = NULL;
    if (meta) meta_json = json_serialize((json_t *)meta);

    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) { free(meta_json); json_free(args); return kb_err("cannot open the kanban board"); }

    char reason[512] = "";
    int ok = kanban_request_review(conn, tid, summary, meta_json,
                                   reviewer && *reviewer ? reviewer : NULL,
                                   -1, false, reason, sizeof reason);
    kdb_close(conn);
    free(meta_json);

    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok != 0));
    json_set(out, "task_id", json_string(tid));
    if (ok) {
        json_set(out, "status", json_string("review"));
    } else {
        json_set(out, "error", json_string(reason[0] ? reason : "could not request review (unknown id or not in running/ready)"));
    }
    json_free(args);
    char *txt = json_serialize_pretty(out, JSON_INDENT(2));
    json_free(out);
    return txt ? txt : kb_err("serialize failed");
}

/* ---- registration ---- */

void registry_init_kanban(void) {
    registry_register("kanban_show",
        "Read a task's full state — title, body, assignee, parent task handoffs, your prior attempts on this task if any, comments, and recent events. Use this to (re)orient yourself before starting work, especially on retries.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"}},\"required\":[]}",
        handle_show);
    registry_register("kanban_list",
        "List Kanban task summaries so an orchestrator profile can discover work to route. Supports assignee, status, tenant, include_archived, and limit filters. Orchestrator-only.",
        "{\"type\":\"object\",\"properties\":{\"assignee\":{\"type\":\"string\"},\"status\":{\"type\":\"string\",\"enum\":[\"triage\",\"todo\",\"ready\",\"running\",\"blocked\",\"done\",\"archived\"]},\"tenant\":{\"type\":\"string\"},\"include_archived\":{\"type\":\"boolean\"},\"limit\":{\"type\":\"integer\"}},\"required\":[]}",
        handle_list);
    registry_register("kanban_complete",
        "Mark your current task done with a structured handoff. At least one of summary or result is required.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"summary\":{\"type\":\"string\"},\"result\":{\"type\":\"string\"},\"metadata\":{\"type\":\"object\"},\"created_cards\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"artifacts\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[]}",
        handle_complete);
    registry_register("kanban_block",
        "Mark your current task as blocked. Provide a reason. Use sticky=true if this should survive auto-promotion.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"reason\":{\"type\":\"string\"},\"sticky\":{\"type\":\"boolean\"}},\"required\":[]}",
        handle_block);
    registry_register("kanban_heartbeat",
        "Extend the liveness window for this task.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"}},\"required\":[]}",
        handle_heartbeat);
    registry_register("kanban_comment",
        "Add a structured comment to a Kanban task.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"}},\"required\":[\"body\"]}",
        handle_comment);
    registry_register("kanban_create",
        "Create a new Kanban task. title and assignee are required.",
        "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"assignee\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"},\"priority\":{\"type\":\"integer\"},\"tenant\":{\"type\":\"string\"},\"workspace_kind\":{\"type\":\"string\",\"enum\":[\"scratch\",\"dir\",\"worktree\"]},\"workspace_path\":{\"type\":\"string\"},\"triage\":{\"type\":\"boolean\"},\"initial_status\":{\"type\":\"string\",\"enum\":[\"running\",\"blocked\"]}},\"required\":[\"title\",\"assignee\"]}",
        handle_create);
    registry_register("kanban_unblock",
        "Move a blocked Kanban task back to ready. Orchestrator-only.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"}},\"required\":[\"task_id\"]}",
        handle_unblock);
    registry_register("kanban_attachments",
        "List a task's attached files, with the absolute on-disk path you can read. Rows carry id, filename, content_type, size and uploader.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"}},\"required\":[]}",
        handle_attachments);
    registry_register("kanban_attach",
        "Attach a file to a task by passing its bytes inline (base64). Stored as a real attachment (not a comment link), capped at 25 MB.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"},\"filename\":{\"type\":\"string\",\"description\":\"File name to store it under. Directory components are stripped.\"},\"content_base64\":{\"type\":\"string\",\"description\":\"The file contents, base64-encoded. Max 25 MB decoded.\"},\"content_type\":{\"type\":\"string\",\"description\":\"Optional MIME type.\"}},\"required\":[\"filename\",\"content_base64\"]}",
        handle_attach);
    registry_register("kanban_request_review",
        "Hand a finished, self-verified task to a reviewer. Moves the task to the review column. Unlike kanban_block this is not a blocker.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"},\"summary\":{\"type\":\"string\",\"description\":\"What was implemented and how it was verified, in one or two sentences.\"},\"reviewer\":{\"type\":\"string\",\"description\":\"Optional reviewer profile.\"},\"metadata\":{\"type\":\"object\",\"description\":\"Optional structured metadata for the event payload.\"}},\"required\":[\"summary\"]}",
        handle_request_review);
    registry_register("kanban_request_changes",
        "Return a review-owned task to its implementer with the changes that are needed. Moves the card out of review and reassigns it to whoever implemented it.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"},\"reason\":{\"type\":\"string\",\"description\":\"Describe the changes needed. Required.\"}},\"required\":[\"reason\"]}",
        handle_request_changes);
    registry_register("kanban_attach_url",
        "Attach a file to a task by URL. Fetches the content, stores it as a real attachment under the task's attachments dir (25 MB cap) and records it on the board.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"},\"url\":{\"type\":\"string\",\"description\":\"http(s) URL to fetch.\"},\"filename\":{\"type\":\"string\",\"description\":\"File name to store it under; defaults to the URL's last path segment.\"},\"content_type\":{\"type\":\"string\",\"description\":\"Optional MIME type.\"}},\"required\":[\"url\"]}",
        handle_attach_url);
    registry_register("kanban_link",
        "Add a parent->child dependency edge. Cycles and self-links rejected.",
        "{\"type\":\"object\",\"properties\":{\"parent_id\":{\"type\":\"string\"},\"child_id\":{\"type\":\"string\"}},\"required\":[\"parent_id\",\"child_id\"]}",
        handle_link);
}
