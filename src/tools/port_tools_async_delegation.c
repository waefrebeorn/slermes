/*
 * port_tools_async_delegation.c — Faithful C port of tools/async_delegation.py.
 * Background (async) delegation registry: dispatch a child on a daemon worker
 * thread, return a handle immediately, deliver a completion event via an
 * injected sink when it finishes. Capacity-gated; interrupt_all cancels.
 *
 * Self-contained: injectable runner + sink, no process_registry dependency.
 * (Replaces a prior stub that only incremented a counter and fabricated IDs.)
 */

#include "async_delegation.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "libuuid/uuid.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* ---- record ---------------------------------------------------------- */

typedef struct async_delegation_rec {
    char *delegation_id;
    char *goal;
    char *context;
    char **toolsets;          /* NULL-terminated, or NULL */
    int n_toolsets;
    char *role;
    char *model;
    char *session_key;
    char *origin_ui_session_id;
    char *origin_session_id;
    char *parent_session_id;
    char **goals;             /* batch goal list (NULL-terminated, or NULL) */
    int n_goals;
    int is_batch;
    char *status;             /* "running" | "stalling" | "finalizing" | "completed" | "error" | "cancelled" */
    double dispatched_at;
    double completed_at;
    async_delegation_runner_t runner;
    async_delegation_interrupt_t interrupt_fn;
    async_delegation_sink_t sink;
    int done;                 /* worker finished (for tests/await) */
    pthread_t thread;
    struct async_delegation_rec *next;  /* intrusive list */
    /* stall-monitor state (Python: progress_fn sampling) */
    char *progress_token;     /* last sampled progress token (strdup'd) */
    double progress_ts;       /* last refresh timestamp */
    double interrupted_at;     /* when status flipped to stalling */
    double stall_quiet_seconds;
    double stall_threshold_seconds;
    int stall_in_tool;
} async_delegation_rec_t;

/* ---- module state ---------------------------------------------------- */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static async_delegation_rec_t *g_head = NULL;   /* running + retained completed */
static int g_max_retained = ASYNC_DELEGATION_MAX_RETAINED_COMPLETED;
static pthread_cond_t g_done_cond = PTHREAD_COND_INITIALIZER;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Free all heap-owned fields of a record (does NOT free `rec` itself or its
 * thread; used by prune/shutdown/reset). Caller must hold g_lock if freeing
 * struct, but this only touches heap fields so it's lock-agnostic. */
static void free_record_contents(async_delegation_rec_t *r) {
    if (!r) return;
    free(r->delegation_id);
    free(r->goal);
    free(r->context);
    free(r->role);
    free(r->model);
    free(r->session_key);
    free(r->origin_ui_session_id);
    free(r->origin_session_id);
    free(r->parent_session_id);
    if (r->toolsets) {
        for (int i = 0; i < r->n_toolsets; i++) free(r->toolsets[i]);
        free(r->toolsets);
        r->toolsets = NULL;
        r->n_toolsets = 0;
    }
    if (r->goals) {
        for (int i = 0; i < r->n_goals; i++) free(r->goals[i]);
        free(r->goals);
        r->goals = NULL;
        r->n_goals = 0;
    }
    free(r->status);
    free(r->progress_token);
}

/* ---- helpers --------------------------------------------------------- */

/* Caller must hold g_lock. Count running records. */
static int running_count_locked(void) {
    int n = 0;
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (strcmp(r->status, "running") == 0 || strcmp(r->status, "stalling") == 0
            || strcmp(r->status, "finalizing") == 0) n++;
    return n;
}

/* Caller must hold g_lock. Drop oldest completed beyond the retention cap. */
/* PoP: prune_completed_locked @ tools/async_delegation.py:_prune_completed_locked */
static void prune_completed_locked(void) {
    int completed = 0;
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (strcmp(r->status, "running") != 0) completed++;
    if (completed <= g_max_retained) return;
    int to_drop = completed - g_max_retained;
    async_delegation_rec_t **pp = &g_head;
    while (*pp && to_drop > 0) {
        async_delegation_rec_t *r = *pp;
        if (strcmp(r->status, "running") != 0) {
            *pp = r->next;
            free(r->delegation_id); free(r->goal); free(r->context);
            free(r->role); free(r->model); free(r->session_key);
            if (r->toolsets) { for (int i = 0; i < r->n_toolsets; i++) free(r->toolsets[i]); free(r->toolsets); }
            free(r);
            to_drop--;
        } else {
            pp = &r->next;
        }
    }
}

/* Capture toolsets into a malloc'd NULL-terminated array of strdup'd strings. */
static char **copy_toolsets(const char *const *ts) {
    if (!ts) return NULL;
    int n = 0;
    while (ts[n]) n++;
    if (n == 0) return NULL;
    char **out = malloc((n + 1) * sizeof(char *));
    for (int i = 0; i < n; i++) out[i] = xstrdup(ts[i]);
    out[n] = NULL;
    return out;
}

/* Duplicate a NULL-terminated string array into a json array of strings. */
static json_node_t *dup_string_array(char **arr, int n) {
    json_node_t *ja = json_new_array();
    for (int i = 0; i < n; i++)
        json_array_append(ja, json_string(arr[i] ? arr[i] : ""));
    return ja;
}

/* ---- finalize + completion event ------------------------------------ */

static void build_and_emit_event(async_delegation_rec_t *rec, json_node_t *result, const char *status) {
    json_node_t *evt = json_new_object();
    json_object_set(evt, "type", json_string("async_delegation"));
    json_object_set(evt, "delegation_id", json_string(rec->delegation_id));
    json_object_set(evt, "session_key", json_string(rec->session_key ? rec->session_key : ""));
    json_object_set(evt, "origin_ui_session_id", json_string(rec->origin_ui_session_id ? rec->origin_ui_session_id : ""));
    json_object_set(evt, "origin_session_id", json_string(rec->origin_session_id ? rec->origin_session_id : ""));
    json_object_set(evt, "parent_session_id", rec->parent_session_id ? json_string(rec->parent_session_id) : json_null());
    json_object_set(evt, "goal", json_string(rec->goal ? rec->goal : ""));
    json_object_set(evt, "context", rec->context ? json_string(rec->context) : json_null());
    if (rec->toolsets) {
        json_node_t *ts = json_new_array();
        for (int i = 0; i < rec->n_toolsets; i++) json_array_append(ts, json_string(rec->toolsets[i]));
        json_object_set(evt, "toolsets", ts);
    } else {
        json_object_set(evt, "toolsets", json_null());
    }
    json_object_set(evt, "role", json_string(rec->role ? rec->role : ""));
    const char *rmodel = result ? json_get_str(result, "model", NULL) : NULL;
    json_object_set(evt, "model", json_string(rmodel ? rmodel : (rec->model ? rec->model : "")));
    json_object_set(evt, "status", json_string(status));
    if (rec->is_batch) json_object_set(evt, "is_batch", json_bool(1));
    json_object_set(evt, "goals", rec->goals ? dup_string_array(rec->goals, rec->n_goals) : json_new_array());
    const char *summary = result ? json_get_str(result, "summary", NULL) : NULL;
    json_object_set(evt, "summary", summary ? json_string(summary) : json_null());
    const char *error = result ? json_get_str(result, "error", NULL) : NULL;
    json_object_set(evt, "error", error ? json_string(error) : json_null());
    long api_calls = result ? (long)json_get_num(result, "api_calls", 0) : 0;
    json_object_set(evt, "api_calls", json_number((double)api_calls));
    double dur = result ? json_get_num(result, "duration_seconds", -1) : -1;
    if (dur < 0) dur = rec->completed_at - rec->dispatched_at;
    json_object_set(evt, "duration_seconds", json_number(dur));
    json_object_set(evt, "dispatched_at", json_number(rec->dispatched_at));
    json_object_set(evt, "completed_at", json_number(rec->completed_at));
    const char *exit_reason = result ? json_get_str(result, "exit_reason", NULL) : NULL;
    json_object_set(evt, "exit_reason", exit_reason ? json_string(exit_reason) : json_null());
    if (rec->is_batch) {
        json_node_t *results = result ? json_object_get(result, "results") : NULL;
        json_object_set(evt, "results", results ? json_copy(results) : json_new_array());
        double td = result ? json_get_num(result, "total_duration_seconds", -1) : -1;
        if (td < 0) td = dur;
        json_object_set(evt, "total_duration_seconds", json_number(td));
    }

    if (rec->sink) rec->sink(evt);
    json_free(evt);
}

/* ---- finalization helpers (Python: _begin_finalization / _finish_finalization) */

/* Caller must hold g_lock. Atomically claim terminal delivery while keeping
 * the record active. Returns a shallow copy of the record at claim time
 * (ownership of heap fields transferred to caller), or NULL if no record
 * exists or its status is not running/stalling. */
/* PoP: _begin_finalization @ tools/async_delegation.py:_begin_finalization */
static async_delegation_rec_t *begin_finalization(const char *delegation_id) {
    if (!delegation_id) return NULL;
    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *r = NULL;
    for (async_delegation_rec_t *p = g_head; p; p = p->next) {
        if (p->delegation_id && strcmp(p->delegation_id, delegation_id) == 0) { r = p; break; }
    }
    if (!r || (strcmp(r->status, "running") != 0 && strcmp(r->status, "stalling") != 0)) {
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }
    /* Stay active until durable persistence and queue publication finish. */
    free(r->status);
    r->status = xstrdup("finalizing");
    r->completed_at = now_sec();
    r->interrupt_fn = NULL;       /* child is done */
    r->progress_token = NULL;     /* stop stale-monitor sampling */

    /* Shallow-copy the claim the caller owns (so finalize can emit + free). */
    async_delegation_rec_t *claim = calloc(1, sizeof(*claim));
    claim->delegation_id = xstrdup(r->delegation_id);
    claim->goal = xstrdup(r->goal);
    claim->context = xstrdup(r->context);
    claim->role = xstrdup(r->role);
    claim->model = xstrdup(r->model);
    claim->session_key = xstrdup(r->session_key);
    claim->origin_ui_session_id = xstrdup(r->origin_ui_session_id);
    claim->origin_session_id = xstrdup(r->origin_session_id);
    claim->parent_session_id = xstrdup(r->parent_session_id);
    claim->is_batch = r->is_batch;
    claim->dispatched_at = r->dispatched_at;
    claim->completed_at = r->completed_at;
    if (r->goals) {
        claim->n_goals = r->n_goals;
        claim->goals = malloc((r->n_goals + 1) * sizeof(char *));
        for (int i = 0; i < r->n_goals; i++) claim->goals[i] = xstrdup(r->goals[i]);
        claim->goals[r->n_goals] = NULL;
    }
    if (r->toolsets) {
        claim->toolsets = malloc((r->n_toolsets + 1) * sizeof(char *));
        for (int i = 0; i < r->n_toolsets; i++) claim->toolsets[i] = xstrdup(r->toolsets[i]);
        claim->toolsets[r->n_toolsets] = NULL;
        claim->n_toolsets = r->n_toolsets;
    }
    pthread_mutex_unlock(&g_lock);
    return claim;
}

/* Mark the record terminal and prune. Caller does NOT hold the lock. */
/* PoP: _finish_finalization @ tools/async_delegation.py:_finish_finalization */
static void finish_finalization(const char *delegation_id, const char *status) {
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *p = g_head; p; p = p->next) {
        if (p->delegation_id && strcmp(p->delegation_id, delegation_id) == 0) {
            free(p->status);
            p->status = xstrdup(status);
            break;
        }
    }
    prune_completed_locked();
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);
}

/* Match a record against optional session selectors (OR semantics, like Python). */
static int matches_session_selectors(const async_delegation_rec_t *r,
                                      const char *session_key,
                                      const char *origin_ui_session_id,
                                      const char *parent_session_id) {
    if (origin_ui_session_id && *origin_ui_session_id
        && r->origin_ui_session_id && strcmp(r->origin_ui_session_id, origin_ui_session_id) == 0) return 1;
    if (session_key && *session_key
        && r->session_key && strcmp(r->session_key, session_key) == 0) return 1;
    if (parent_session_id && *parent_session_id
        && r->parent_session_id && strcmp(r->parent_session_id, parent_session_id) == 0) return 1;
    return 0;
}

/* Caller must hold g_lock. True if status is running/stalling/finalizing. */
static int is_live_status(const char *status) {
    return status
        && (strcmp(status, "running") == 0
            || strcmp(status, "stalling") == 0
            || strcmp(status, "finalizing") == 0);
}

/* PoP: finalize @ tools/async_delegation.py:_finalize */
/* PoP: finalize @ tools/image_source.py:_finalize */
static void finalize(async_delegation_rec_t *rec, json_node_t *result, const char *status) {
    async_delegation_rec_t *claim = begin_finalization(rec->delegation_id);
    if (!claim) return;
    build_and_emit_event(claim, result, status);
    free_record_contents(claim);
    free(claim);
    finish_finalization(rec->delegation_id, status);
    if (result) json_free(result);
}

/* Worker thread entry. `arg` is the rec (borrowed; do not free here). */
static void *worker_entry(void *arg) {
    async_delegation_rec_t *rec = (async_delegation_rec_t *)arg;
    json_node_t *result = NULL;
    const char *status = "error";
    if (rec->runner) {
        result = rec->runner();   /* ownership transferred to finalize */
        if (!result) result = json_new_object();
        const char *st = json_get_str(result, "status", NULL);
        status = (st && *st) ? st : "completed";
    } else {
        result = json_new_object();
        status = "completed";
    }
    finalize(rec, result, status);
    return NULL;
}

/* ---- public API ------------------------------------------------------ */

void async_delegation_init(void) { /* mutex/cond statically initialized */ }

void async_delegation_shutdown(void) {
    async_delegation_interrupt_all("shutdown");
    pthread_t threads[64];
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r && n < 64; r = r->next) {
        if (r->thread) threads[n++] = r->thread;
    }
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL);

    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *r = g_head;
    while (r) {
        async_delegation_rec_t *nx = r->next;
        free_record_contents(r);
        free(r);
        r = nx;
    }
    g_head = NULL;
    pthread_mutex_unlock(&g_lock);
}

/* PoP: async_delegation_active_count @ tools/async_delegation.py:active_count */
int async_delegation_active_count(void) {
    pthread_mutex_lock(&g_lock);
    int n = running_count_locked();
    pthread_mutex_unlock(&g_lock);
    return n;
}

char *async_delegation_new_id(void) {
    char *u = uuid_v4();
    char *id = malloc(strlen(u) + 8);
    snprintf(id, strlen(u) + 8, "deleg_%s", u);
    free(u);
    return id;
}

json_node_t *async_delegation_dispatch(
    const char *goal, const char *context, const char *const *toolsets,
    const char *role, const char *model, const char *session_key,
    const char *origin_ui_session_id, const char *origin_session_id,
    const char *parent_session_id,
    async_delegation_runner_t runner, async_delegation_interrupt_t interrupt_fn,
    async_delegation_sink_t sink, int max_async_children)
{
    if (max_async_children <= 0) max_async_children = ASYNC_DELEGATION_DEFAULT_MAX_CHILDREN;
    async_delegation_rec_t *rec = calloc(1, sizeof(*rec));
    rec->delegation_id = async_delegation_new_id();
    rec->goal = xstrdup(goal ? goal : "");
    rec->context = xstrdup(context ? context : "");
    rec->toolsets = copy_toolsets(toolsets);
    rec->n_toolsets = 0;
    if (rec->toolsets) while (rec->toolsets[rec->n_toolsets]) rec->n_toolsets++;
    rec->role = xstrdup(role ? role : "");
    rec->model = xstrdup(model ? model : "");
    rec->session_key = xstrdup(session_key ? session_key : "");
    rec->origin_ui_session_id = xstrdup(origin_ui_session_id ? origin_ui_session_id : "");
    rec->origin_session_id = xstrdup(origin_session_id ? origin_session_id : "");
    rec->parent_session_id = xstrdup(parent_session_id ? parent_session_id : "");
    rec->is_batch = 0;
    rec->status = xstrdup("running");
    rec->dispatched_at = now_sec();
    rec->runner = runner;
    rec->interrupt_fn = interrupt_fn;
    rec->sink = sink;
    rec->done = 0;

    json_node_t *reject = NULL;
    pthread_mutex_lock(&g_lock);
    if (running_count_locked() >= max_async_children) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Async delegation capacity reached (%d running). Wait for one to finish, "
            "or run this task synchronously (background=false). Raise "
            "delegation.max_async_children in config.yaml to allow more.",
            max_async_children);
        reject = json_new_object();
        json_object_set(reject, "status", json_string("rejected"));
        json_object_set(reject, "error", json_string(buf));
        pthread_mutex_unlock(&g_lock);
        free_record_contents(rec);
        free(rec);
        return reject;
    }
    rec->next = g_head;
    g_head = rec;
    pthread_mutex_unlock(&g_lock);

    if (pthread_create(&rec->thread, NULL, worker_entry, rec) != 0) {
        pthread_mutex_lock(&g_lock);
        if (g_head == rec) g_head = rec->next;
        else for (async_delegation_rec_t *p = g_head; p; p = p->next)
            if (p->next == rec) { p->next = rec->next; break; }
        pthread_mutex_unlock(&g_lock);
        json_node_t *rj = json_new_object();
        json_object_set(rj, "status", json_string("rejected"));
        json_object_set(rj, "error", json_string("Failed to schedule async delegation"));
        free_record_contents(rec);
        free(rec);
        return rj;
    }

    json_node_t *ok = json_new_object();
    json_object_set(ok, "status", json_string("dispatched"));
    json_object_set(ok, "delegation_id", json_string(rec->delegation_id));
    return ok;
}

json_node_t *async_delegation_dispatch_batch(
    const char *const *goals, int n_goals, const char *context,
    const char *const *toolsets, const char *role, const char *model,
    const char *session_key, const char *origin_ui_session_id,
    const char *origin_session_id, const char *parent_session_id,
    async_delegation_runner_t runner, async_delegation_interrupt_t interrupt_fn,
    async_delegation_sink_t sink, int max_async_children)
{
    if (max_async_children <= 0) max_async_children = ASYNC_DELEGATION_DEFAULT_MAX_CHILDREN;
    async_delegation_rec_t *rec = calloc(1, sizeof(*rec));
    rec->delegation_id = async_delegation_new_id();
    if (n_goals == 1)
        rec->goal = xstrdup(goals[0] ? goals[0] : "");
    else {
        size_t cap = 64;
        for (int i = 0; i < n_goals; i++) cap += (goals[i] ? strlen(goals[i]) : 0) + 3;
        rec->goal = malloc(cap);
        int off = snprintf(rec->goal, cap, "%d parallel subagents:", n_goals);
        for (int i = 0; i < n_goals; i++) {
            const char *g = goals[i] ? goals[i] : "";
            char tmp[48];
            snprintf(tmp, sizeof(tmp), "%.40s", g);
            off += snprintf(rec->goal + off, cap - off, " %s;", tmp);
        }
    }
    rec->context = xstrdup(context ? context : "");
    rec->toolsets = copy_toolsets(toolsets);
    rec->n_toolsets = 0;
    if (rec->toolsets) while (rec->toolsets[rec->n_toolsets]) rec->n_toolsets++;
    rec->role = xstrdup(role ? role : "");
    rec->model = xstrdup(model ? model : "");
    rec->session_key = xstrdup(session_key ? session_key : "");
    rec->origin_ui_session_id = xstrdup(origin_ui_session_id ? origin_ui_session_id : "");
    rec->origin_session_id = xstrdup(origin_session_id ? origin_session_id : "");
    rec->parent_session_id = xstrdup(parent_session_id ? parent_session_id : "");
    /* Keep the full goal list for active_task_count()/listings (Python: goals). */
    if (n_goals > 0) {
        rec->goals = calloc((size_t)n_goals + 1, sizeof(char *));
        for (int i = 0; i < n_goals; i++) rec->goals[i] = xstrdup(goals[i] ? goals[i] : "");
        rec->n_goals = n_goals;
    }
    rec->is_batch = 1;
    rec->status = xstrdup("running");
    rec->dispatched_at = now_sec();
    rec->runner = runner;
    rec->interrupt_fn = interrupt_fn;
    rec->sink = sink;
    rec->done = 0;

    json_node_t *reject = NULL;
    pthread_mutex_lock(&g_lock);
    if (running_count_locked() >= max_async_children) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Async delegation capacity reached (%d running). Wait for one to finish, "
            "or raise delegation.max_async_children in config.yaml to allow more.",
            max_async_children);
        reject = json_new_object();
        json_object_set(reject, "status", json_string("rejected"));
        json_object_set(reject, "error", json_string(buf));
        pthread_mutex_unlock(&g_lock);
        free_record_contents(rec);
        free(rec);
        return reject;
    }
    rec->next = g_head;
    g_head = rec;
    pthread_mutex_unlock(&g_lock);

    if (pthread_create(&rec->thread, NULL, worker_entry, rec) != 0) {
        pthread_mutex_lock(&g_lock);
        if (g_head == rec) g_head = rec->next;
        else for (async_delegation_rec_t *p = g_head; p; p = p->next)
            if (p->next == rec) { p->next = rec->next; break; }
        pthread_mutex_unlock(&g_lock);
        json_node_t *rj = json_new_object();
        json_object_set(rj, "status", json_string("rejected"));
        json_object_set(rj, "error", json_string("Failed to schedule async delegation batch"));
        free_record_contents(rec);
        free(rec);
        return rj;
    }

    json_node_t *ok = json_new_object();
    json_object_set(ok, "status", json_string("dispatched"));
    json_object_set(ok, "delegation_id", json_string(rec->delegation_id));
    return ok;
}

json_node_t *async_delegation_list(void) {
    json_node_t *arr = json_new_array();
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        json_node_t *o = json_new_object();
        json_object_set(o, "delegation_id", json_string(r->delegation_id));
        json_object_set(o, "goal", json_string(r->goal ? r->goal : ""));
        json_object_set(o, "context", r->context ? json_string(r->context) : json_null());
        if (r->toolsets) {
            json_node_t *ts = json_new_array();
            for (int i = 0; i < r->n_toolsets; i++) json_array_append(ts, json_string(r->toolsets[i]));
            json_object_set(o, "toolsets", ts);
        } else json_object_set(o, "toolsets", json_null());
        json_object_set(o, "role", json_string(r->role ? r->role : ""));
        json_object_set(o, "model", json_string(r->model ? r->model : ""));
        json_object_set(o, "session_key", json_string(r->session_key ? r->session_key : ""));
        json_object_set(o, "origin_ui_session_id", json_string(r->origin_ui_session_id ? r->origin_ui_session_id : ""));
        json_object_set(o, "origin_session_id", json_string(r->origin_session_id ? r->origin_session_id : ""));
        if (r->parent_session_id)
            json_object_set(o, "parent_session_id", json_string(r->parent_session_id));
        json_object_set(o, "status", json_string(r->status ? r->status : ""));
        json_object_set(o, "dispatched_at", json_number(r->dispatched_at));
        json_object_set(o, "completed_at", json_number(r->completed_at));
        if (r->is_batch) json_object_set(o, "is_batch", json_bool(1));
        if (r->is_batch && r->goals) {
            json_node_t *g = json_new_array();
            for (int i = 0; i < r->n_goals; i++)
                json_array_append(g, json_string(r->goals[i] ? r->goals[i] : ""));
            json_object_set(o, "goals", g);
        }
        json_array_append(arr, o);
    }
    pthread_mutex_unlock(&g_lock);
    return arr;
}

/* PoP: async_delegation_interrupt_all @ tools/async_delegation.py:interrupt_all */
int async_delegation_interrupt_all(const char *reason) {
    (void)reason;
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (is_live_status(r->status)) {
            if (r->interrupt_fn) r->interrupt_fn();
            free(r->status);
            r->status = xstrdup("cancelled");
            r->completed_at = now_sec();
            r->interrupt_fn = NULL;
            r->done = 1;
            n++;
        }
    }
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);
    return n;
}

void async_delegation_reset_for_tests(void) {
    /* Snapshot live worker threads, release the lock, THEN join (a worker's
     * finalize needs g_lock, so we must not hold it while joining). */
    pthread_t threads[64];
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r && n < 64; r = r->next) {
        if (r->thread) threads[n++] = r->thread;
    }
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL);

    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *r = g_head;
    while (r) {
        async_delegation_rec_t *nx = r->next;
        free_record_contents(r);
        free(r);
        r = nx;
    }
    g_head = NULL;
    pthread_mutex_unlock(&g_lock);
}

/* ── Session-scoped queries + interrupt (async_delegation.py gaps) ── */

/* PoP: active_for_session @ tools/async_delegation.py:active_for_session */
int async_delegation_active_for_session(const char *session_key,
                                        const char *origin_ui_session_id,
                                        const char *parent_session_id) {
    if ((!session_key || !*session_key)
        && (!origin_ui_session_id || !*origin_ui_session_id)
        && (!parent_session_id || !*parent_session_id)) return 0;
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (is_live_status(r->status)
            && matches_session_selectors(r, session_key, origin_ui_session_id, parent_session_id)) n++;
    pthread_mutex_unlock(&g_lock);
    return n;
}

/* PoP: active_task_count @ tools/async_delegation.py:active_task_count */
int async_delegation_active_task_count(void) {
    int total = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (!is_live_status(r->status)) continue;
        if (r->is_batch) {
            total += (r->n_goals > 0) ? r->n_goals : 1;
        } else {
            total += 1;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return total;
}

/* PoP: has_live_for_session @ tools/async_delegation.py:has_live_for_session */
bool async_delegation_has_live_for_session(const char *session_key,
                                           const char *origin_ui_session_id,
                                           const char *parent_session_id) {
    if ((!session_key || !*session_key)
        && (!origin_ui_session_id || !*origin_ui_session_id)
        && (!parent_session_id || !*parent_session_id)) return false;
    bool found = false;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (is_live_status(r->status)
            && matches_session_selectors(r, session_key, origin_ui_session_id, parent_session_id)) {
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return found;
}

/* PoP: interrupt_for_session @ tools/async_delegation.py:interrupt_for_session */
int async_delegation_interrupt_for_session(const char *session_key,
                                           const char *origin_ui_session_id,
                                           const char *parent_session_id) {
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (is_live_status(r->status)
            && matches_session_selectors(r, session_key, origin_ui_session_id, parent_session_id)) {
            if (r->interrupt_fn) r->interrupt_fn();
            free(r->status);
            r->status = xstrdup("cancelled");
            r->completed_at = now_sec();
            r->interrupt_fn = NULL;
            r->done = 1;
            n++;
        }
    }
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);
    return n;
}
