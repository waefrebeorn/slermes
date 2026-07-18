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
    int is_batch;
    char *status;             /* "running" | "completed" | "error" | "cancelled" */
    double dispatched_at;
    double completed_at;
    async_delegation_runner_t runner;
    async_delegation_interrupt_t interrupt_fn;
    async_delegation_sink_t sink;
    int done;                 /* worker finished (for tests/await) */
    pthread_t thread;
    struct async_delegation_rec *next;  /* intrusive list */
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

/* ---- helpers --------------------------------------------------------- */

/* Caller must hold g_lock. Count running records. */
static int running_count_locked(void) {
    int n = 0;
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (strcmp(r->status, "running") == 0) n++;
    return n;
}

/* Caller must hold g_lock. Drop oldest completed beyond the retention cap. */
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

/* ---- finalize + completion event ------------------------------------ */

static void build_and_emit_event(async_delegation_rec_t *rec, json_node_t *result, const char *status) {
    json_node_t *evt = json_new_object();
    json_object_set(evt, "type", json_string("async_delegation"));
    json_object_set(evt, "delegation_id", json_string(rec->delegation_id));
    json_object_set(evt, "session_key", json_string(rec->session_key ? rec->session_key : ""));
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

static void finalize(async_delegation_rec_t *rec, json_node_t *result, const char *status) {
    double completed_at = now_sec();
    pthread_mutex_lock(&g_lock);
    if (rec->status && strcmp(rec->status, "running") == 0) {
        free(rec->status);
        rec->status = xstrdup(status);
    }
    rec->completed_at = completed_at;
    rec->interrupt_fn = NULL; /* child is done */
    prune_completed_locked();
    rec->done = 1;
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);

    build_and_emit_event(rec, result, status);
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
        free(r->delegation_id); free(r->goal); free(r->context);
        free(r->role); free(r->model); free(r->session_key);
        if (r->toolsets) { for (int i = 0; i < r->n_toolsets; i++) free(r->toolsets[i]); free(r->toolsets); }
        free(r->status);
        free(r);
        r = nx;
    }
    g_head = NULL;
    pthread_mutex_unlock(&g_lock);
}

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
        free(rec->delegation_id); free(rec->goal); free(rec->context);
        free(rec->role); free(rec->model); free(rec->session_key);
        if (rec->toolsets) { for (int i = 0; i < rec->n_toolsets; i++) free(rec->toolsets[i]); free(rec->toolsets); }
        free(rec->status); free(rec);
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
        free(rec->delegation_id); free(rec->goal); free(rec->context);
        free(rec->role); free(rec->model); free(rec->session_key);
        if (rec->toolsets) { for (int i = 0; i < rec->n_toolsets; i++) free(rec->toolsets[i]); free(rec->toolsets); }
        free(rec->status); free(rec);
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
    const char *session_key, async_delegation_runner_t runner,
    async_delegation_interrupt_t interrupt_fn, async_delegation_sink_t sink,
    int max_async_children)
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
        free(rec->delegation_id); free(rec->goal); free(rec->context);
        free(rec->role); free(rec->model); free(rec->session_key);
        if (rec->toolsets) { for (int i = 0; i < rec->n_toolsets; i++) free(rec->toolsets[i]); free(rec->toolsets); }
        free(rec->status); free(rec);
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
        free(rec->delegation_id); free(rec->goal); free(rec->context);
        free(rec->role); free(rec->model); free(rec->session_key);
        if (rec->toolsets) { for (int i = 0; i < rec->n_toolsets; i++) free(rec->toolsets[i]); free(rec->toolsets); }
        free(rec->status); free(rec);
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
        json_object_set(o, "status", json_string(r->status ? r->status : ""));
        json_object_set(o, "dispatched_at", json_number(r->dispatched_at));
        json_object_set(o, "completed_at", json_number(r->completed_at));
        if (r->is_batch) json_object_set(o, "is_batch", json_bool(1));
        json_array_append(arr, o);
    }
    pthread_mutex_unlock(&g_lock);
    return arr;
}

int async_delegation_interrupt_all(const char *reason) {
    (void)reason;
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (strcmp(r->status, "running") == 0) {
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
        free(r->delegation_id); free(r->goal); free(r->context);
        free(r->role); free(r->model); free(r->session_key);
        if (r->toolsets) { for (int i = 0; i < r->n_toolsets; i++) free(r->toolsets[i]); free(r->toolsets); }
        free(r->status);
        free(r);
        r = nx;
    }
    g_head = NULL;
    pthread_mutex_unlock(&g_lock);
}
