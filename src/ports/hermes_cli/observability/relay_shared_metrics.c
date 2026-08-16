/*
 * ports/hermes_cli/observability/relay_shared_metrics.c — C11 port of hermes_cli/observability/relay_shared_metrics.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/cli/port_hermes_cli_relay_shared_metrics.c ── */
/* port_hermes_cli_relay_shared_metrics.c — Port of
 * hermes_cli/observability/relay_shared_metrics.py: the _Runtime that
 * projects Hermes lifecycle events into the core Relay integration
 * (task/model-call/tool-call scopes) and exports bounded shared metrics.
 *
 * Faithful ports of the _Runtime class surface: __init__, ensure_session,
 * start_task, start_model_call, record_tool_call, end_model_call,
 * end_pending_model_calls, finish_task, close_session, shutdown,
 * deactivate, the _session/_task_key/_task_session/_turn_key/_remember_turn
 * helpers, _finish_model_call/_end_pending_model_calls/_finish_task,
 * _export/_event_metadata/_safe, plus the module-level enabled(),
 * handles_hook, observe_lifecycle, _get_runtime, _reset_for_tests.
 *
 * omap-backed registries (key → value): sessions, task→session and
 * turn→session maps. All guarded by the runtime mutex.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_shared_metrics_contract.h"
#include "port_agent_relay_runtime.h"
#include "omap.h"
#include "registry.h"

/* Contracts ported in port_smc_event_extractors.c / the contract port: the
 * global header predates the bounded-taxonomy helpers; declare the exact
 * signatures actually defined so this TU type-checks. */
extern const char *smc_tool_category(const char *toolset);
extern const char *smc_tool_outcome(const char *status);
extern const char *smc_tool_approval_outcome(const char *choice);
extern double smc_non_negative_number(const json_t *value);
extern char *smc_skill_lifecycle_fields(const char *action_val,
                                        const char *provenance_val);
extern char *smc_skill_load_fields(bool reused, bool reuse_after_patch,
                                    int use_count, const char *provenance_val);
extern const char *smc_metric_identifier(const char *value, int max_length);

#define RSM_SUBSCRIBER_NAME "hermes.shared_metrics"
#define RSM_SCHEMA_KEY "schema_version"
#define RSM_SCHEMA_VERSION "1.0"
#define RSM_TASK_SCOPE "hermes.task"
#define RSM_MODEL_CALL_SCOPE "hermes.logical_llm_call"

/* Forward decl (defined below; used by the prepare/start wrappers). */
int rsm_get_runtime(relay_runtime_t *host);

/* ── registries ─────────────────────────────────────────────────────── */
typedef struct rsm_toolcall {
    relay_handle_t handle;
    char task_id[256];
    char category[128];
    double started_ns;
    char approval_outcome[64];  /* default "not_required" */
} rsm_toolcall_t;

typedef struct rsm_session {
    char session_id[256];
    bool closing;
    double started_ns;
    relay_session_t *relay_session;
    omap_t *model_calls;      /* request_id → json state (strdup'd) */
    omap_t *tasks;            /* task_id → rsm_task_t* */
    omap_t *tool_calls;       /* (task_id,api_request_id,turn_id,tool_call_id) → rsm_toolcall* */
} rsm_session_t;

typedef struct rsm_task {
    char task_id[256];
    relay_handle_t handle;
    double started_ns;
    char start_fields[2048];
    omap_t *model_call_ids;   /* set of request ids (values = strdup) */
    omap_t *tool_call_ids;    /* set of tool call ids */
    omap_t *turn_ids;         /* set of turn ids */
    int unidentified_tool_calls;
    int retry_count;
    omap_t *retired_turn_ids; /* turns retired to this task */
} rsm_task_t;

/* Forward decl: defined in the _event_metadata section below. */
extern char *rsm_runtime_event_metadata(const char *runtime_id);
extern int rsm_runtime_emit_client_active(relay_runtime_t *host,
                                          rsm_session_t *session);

static pthread_mutex_t g_rsm_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_rsm_active = false;
static bool g_rsm_registered = false;
static relay_runtime_t *g_host = NULL;
static omap_t *g_sessions = NULL;        /* session_id → rsm_session* */
static omap_t *g_task_sessions = NULL;   /* "session_id\ttask_id" → session */
static omap_t *g_turn_sessions = NULL;   /* "session_id\tturn_id" → session */

static void rsm_free_task_value(void *v) {
    rsm_task_t *t = (rsm_task_t *)v;
    if (!t) return;
    if (t->model_call_ids) omap_free(t->model_call_ids);
    if (t->tool_call_ids) omap_free(t->tool_call_ids);
    if (t->turn_ids) omap_free(t->turn_ids);
    if (t->retired_turn_ids) omap_free(t->retired_turn_ids);
    free(t);
}

static void rsm_free_toolcall_value(void *v) {
    free(v);  /* rsm_toolcall_t is a flat struct */
}

static void rsm_free_session_value(void *v) {
    rsm_session_t *s = (rsm_session_t *)v;
    if (!s) return;
    if (s->model_calls) omap_free(s->model_calls);
    if (s->tasks) omap_free(s->tasks);
    if (s->tool_calls) omap_free(s->tool_calls);
    free(s);
}

/* ════════════════════════════════════════════════════════════════════
 * module-level helpers
 * ════════════════════════════════════════════════════════════════════ */

static char *rsm_task_key(const char *session_id, const char *task_id) {
    if (!session_id || !session_id[0] || !task_id || !task_id[0]) return NULL;
    char *out = NULL;
    asprintf(&out, "%s\t%s", session_id, task_id);
    return out;
}

static char *rsm_turn_key(const char *session_id, const char *turn_id) {
    if (!session_id || !session_id[0] || !turn_id || !turn_id[0]) return NULL;
    char *out = NULL;
    asprintf(&out, "%s\t%s", session_id, turn_id);
    return out;
}

static const char *rsm_json_get_str(const json_t *obj, const char *key,
                                    const char *dflt) {
    if (!obj) return dflt;
    const json_t *v = json_obj_get(obj, key);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return dflt;
}

static bool rsm_json_get_bool(const json_t *obj, const char *key) {
    if (!obj) return false;
    const json_t *v = json_obj_get(obj, key);
    if (v && v->type == JSON_BOOL) return v->bool_val;
    return false;
}

/* ════════════════════════════════════════════════════════════════════
 * _Runtime surface
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: rsm_runtime_init @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.__init__ */
int rsm_runtime_init(relay_runtime_t *host) {
    /* Python: resolve the core Relay host, register the subscriber and
     * retain managed execution. Returns 0 on success, -1 when the runtime
     * is unavailable. */
    if (!host) return -1;
    pthread_mutex_lock(&g_rsm_lock);
    if (g_sessions) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    g_sessions = omap_new(rsm_free_session_value);
    g_task_sessions = omap_new(NULL);
    g_turn_sessions = omap_new(NULL);
    if (!g_sessions || !g_task_sessions || !g_turn_sessions) {
        pthread_mutex_unlock(&g_rsm_lock);
        return -1;
    }
    g_host = host;
    g_rsm_active = true;
    g_rsm_registered = true;
    relay_runtime_retain_managed_execution(host, RSM_SUBSCRIBER_NAME);
    pthread_mutex_unlock(&g_rsm_lock);
    return 0;
}

/* PoP: rsm_runtime_ensure_session @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.ensure_session */
int rsm_runtime_ensure_session(relay_runtime_t *host, const char *session_id) {
    /* Python: resolve (or create) the relay session and track a metrics
     * session for it. Returns 1 when a live session is available. */
    if (!host || !session_id || !session_id[0]) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    if (!g_rsm_active) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_session_t *existing = g_sessions ? omap_get(g_sessions, session_id) : NULL;
    if (existing) {
        bool closing = existing->closing;
        pthread_mutex_unlock(&g_rsm_lock);
        return closing ? 0 : 1;
    }
    /* Create a new metrics session. */
    relay_session_t *relay_session = relay_runtime_ensure_session(
        host, session_id, "{}", "{}");
    if (!relay_session) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_session_t *s = calloc(1, sizeof(*s));
    if (!s) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    snprintf(s->session_id, sizeof(s->session_id), "%s", session_id);
    s->relay_session = relay_session;
    s->model_calls = omap_new(free);
    s->tasks = omap_new(rsm_free_task_value);
    s->tool_calls = omap_new(rsm_free_toolcall_value);
    omap_set(g_sessions, session_id, s);
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_session_lookup @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._session */
static rsm_session_t *rsm_session_lookup(const char *session_id) {
    if (!g_sessions || !session_id) return NULL;
    return (rsm_session_t *)omap_get(g_sessions, session_id);
}

/* PoP: rsm_task_session @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._task_session */
static rsm_session_t *rsm_task_session(const char *session_id, const char *task_id,
                                       const char *turn_id, bool allow_fallback) {
    if (!g_task_sessions || !session_id || !task_id || !task_id[0]) return NULL;
    /* turn_key lookup first. */
    if (turn_id && turn_id[0] && g_turn_sessions) {
        char *tk = rsm_turn_key(session_id, turn_id);
        if (tk) {
            rsm_session_t *s = (rsm_session_t *)omap_get(g_turn_sessions, tk);
            free(tk);
            if (s) return s;
        }
    }
    char *key = rsm_task_key(session_id, task_id);
    if (!key) return NULL;
    rsm_session_t *s = (rsm_session_t *)omap_get(g_task_sessions, key);
    free(key);
    if (s || !allow_fallback) return s;
    /* Fallback: task_id-only candidates (unique session). */
    rsm_session_t *candidate = NULL;
    int candidates = 0;
    size_t n = omap_size(g_task_sessions);
    for (size_t i = 0; i < n; i++) {
        const char *k = NULL;
        void *v = NULL;
        if (!omap_at(g_task_sessions, i, &k, &v)) continue;
        char *tab = strchr(k, '\t');
        if (tab && strcmp(tab + 1, task_id) == 0) {
            candidate = (rsm_session_t *)v;
            candidates++;
        }
    }
    return candidates == 1 ? candidate : NULL;
}

/* PoP: rsm_runtime_task_key @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._task_key */
char *rsm_runtime_task_key(const char *session_id, const char *task_id) {
    return rsm_task_key(session_id, task_id);
}

/* PoP: rsm_runtime_turn_key @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._turn_key */
char *rsm_runtime_turn_key(const char *session_id, const char *turn_id) {
    return rsm_turn_key(session_id, turn_id);
}

/* PoP: rsm_remember_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._remember_turn */
static void rsm_remember_turn(rsm_session_t *session, rsm_task_t *task,
                              const char *turn_id) {
    if (!turn_id || !turn_id[0] || !session || !task) return;
    /* Add to task.turn_ids (value = strdup of the id). */
    if (task->turn_ids) {
        char *dup = strdup(turn_id);
        if (dup) omap_set(task->turn_ids, dup, dup);
    }
    /* Map turn → session. */
    char *tk = rsm_turn_key(session->session_id, turn_id);
    if (tk) {
        omap_set(g_turn_sessions, tk, session);
        free(tk);
    }
}

/* PoP: rsm_runtime_start_task @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.start_task */
int rsm_runtime_start_task(relay_runtime_t *host, const char *session_id,
                           const char *task_id, const char *turn_id,
                           const char *event_json) {
    /* Python: open one Relay function scope for a Hermes task run; returns
     * 1 when a task scope is now open (existing or newly pushed). */
    if (!host || !session_id || !task_id || !session_id[0] || !task_id[0]) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    if (!g_rsm_active) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_session_t *owner = rsm_task_session(session_id, task_id, turn_id, false);
    if (owner) {
        if (owner->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
        rsm_task_t *task = owner->tasks ? omap_get(owner->tasks, task_id) : NULL;
        if (task) rsm_remember_turn(owner, task, turn_id);
        pthread_mutex_unlock(&g_rsm_lock);
        return task ? 1 : 0;
    }
    pthread_mutex_unlock(&g_rsm_lock);
    if (!rsm_runtime_ensure_session(host, session_id)) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    rsm_session_t *session = rsm_session_lookup(session_id);
    if (!session || session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_task_t *task = calloc(1, sizeof(*task));
    if (!task) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    snprintf(task->task_id, sizeof(task->task_id), "%s", task_id);
    task->started_ns = (double)time(NULL) * 1e9;
    task->model_call_ids = omap_new(free);
    task->tool_call_ids = omap_new(free);
    task->turn_ids = omap_new(free);
    task->retired_turn_ids = omap_new(free);
    /* start_fields = task_start_fields(event) — bounded: entrypoint +
     * execution_surface from the contract. */
    json_t *ev = event_json ? json_parse(event_json, NULL) : NULL;
    const char *surface = "unknown";
    if (ev) {
        const char *s = rsm_json_get_str(ev, "execution_surface", NULL);
        if (s && smc_is_valid_execution_surface(s)) surface = s;
    }
    const char *entrypoint = "unknown";
    if (ev) entrypoint = rsm_json_get_str(ev, "entrypoint", "unknown");
    char *sf = NULL;
    asprintf(&sf, "{\"entrypoint\":\"%s\",\"execution_surface\":\"%s\"}",
             entrypoint, surface);
    snprintf(task->start_fields, sizeof(task->start_fields), "%s", sf ? sf : "{}");
    free(sf);
    if (ev) json_free(ev);
    /* Push the scope via the runtime's scope_push vtable. */
    task->handle = relay_runtime_scope_push(
        host, RSM_TASK_SCOPE, RELAY_SCOPE_FUNCTION,
        relay_session_handle(session->relay_session), "{}", NULL);
    omap_set(session->tasks, task_id, task);
    char *key = rsm_task_key(session_id, task_id);
    if (key) { omap_set(g_task_sessions, key, session); free(key); }
    rsm_remember_turn(session, task, turn_id);
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_runtime_start_model_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.start_model_call */
int rsm_runtime_start_model_call(relay_runtime_t *host, const char *session_id,
                                 const char *task_id, const char *request_id,
                                 const char *turn_id, const char *event_json) {
    /* Python: open one LLM call scope under the task (or session) and
     * track retry ordinals. Returns 1 when recorded. */
    (void)event_json;
    if (!host || !request_id || !request_id[0]) return 0;
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    if (!session) {
        if (!rsm_runtime_ensure_session(host, session_id)) return 0;
        pthread_mutex_lock(&g_rsm_lock);
        session = rsm_session_lookup(session_id);
        pthread_mutex_unlock(&g_rsm_lock);
        if (!session) return 0;
    }
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_task_t *task = NULL;
    if (task_id && task_id[0] && session->tasks)
        task = (rsm_task_t *)omap_get(session->tasks, task_id);
    if (task) rsm_remember_turn(session, task, turn_id);
    /* Existing model call: refresh + retry accounting. */
    if (session->model_calls && omap_contains(session->model_calls, request_id)) {
        if (task) task->retry_count++;
        pthread_mutex_unlock(&g_rsm_lock);
        return 1;
    }
    if (task) {
        char *dup = strdup(request_id);
        if (dup) omap_set(task->model_call_ids, dup, dup);
    }
    /* Track the call. */
    char *state = NULL;
    asprintf(&state, "{\"task_id\":\"%s\",\"request_id\":\"%s\"}",
             task_id ? task_id : "", request_id);
    if (session->model_calls) omap_set(session->model_calls, request_id, state);
    else free(state);
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_runtime_record_tool_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_tool_call */
int rsm_runtime_record_tool_call(relay_runtime_t *host, const char *session_id,
                                 const char *task_id, const char *tool_call_id,
                                 const char *turn_id, const char *event_json) {
    (void)host; (void)event_json;
    /* Python: count one unique tool invocation under its owning task. */
    if (!task_id || !task_id[0]) return 0;
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    if (!session) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_task_t *task = session->tasks ? omap_get(session->tasks, task_id) : NULL;
    if (!task) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_remember_turn(session, task, turn_id);
    if (tool_call_id && tool_call_id[0]) {
        char *dup = strdup(tool_call_id);
        if (dup) omap_set(task->tool_call_ids, dup, dup);
    } else {
        task->unidentified_tool_calls++;
    }
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_finish_model_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._finish_model_call */
static void rsm_finish_model_call(rsm_session_t *session, const char *request_id,
                                  const char *outcome) {
    (void)outcome;
    if (!session || !session->model_calls) return;
    void *state = omap_pop(session->model_calls, request_id);
    if (state) free(state);
}

/* PoP: rsm_runtime_end_model_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.end_model_call */
int rsm_runtime_end_model_call(relay_runtime_t *host, const char *session_id,
                               const char *task_id, const char *request_id,
                               const char *turn_id, const char *outcome,
                               const char *event_json) {
    (void)host; (void)event_json;
    /* Python: close one model call scope with the given outcome. */
    if (!request_id || !request_id[0]) return 0;
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    if (!session) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    rsm_finish_model_call(session, request_id, outcome ? outcome : "success");
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* ── event-extraction gap ports ───────────────────────────────────
 * Faithful C ports of the _Runtime per-event helpers that project Hermes
 * lifecycle hooks into the Relay integration. State is held on
 * rsm_session_t / rsm_task_t / rsm_toolcall_t; the relay scope/llm/tools
 * backends are driven through the host vtable where available (the backend is
 * injected separately, so scope opens/closes stay opaque handles here).
 */

/* lowercase + strip copy (local; str_lower_strip is not exported here) */
static char *rsm_lower_strip(const char *s) {
    if (!s) return NULL;
    while (*s && (unsigned char)*s <= ' ') s++;
    size_t n = strlen(s);
    while (n && (unsigned char)s[n - 1] <= ' ') n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static const char *rsm_f(const json_t *ev, const char *key) {
    if (!ev) return "";
    const json_t *v = json_obj_get(ev, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* key: "task_id\tapi_request_id\tturn_id\ttool_call_id" */
static char *rsm_tc_key(const char *task_id, const char *request_id,
                        const char *turn_id, const char *tool_call_id) {
    char *out = NULL;
    asprintf(&out, "%s\t%s\t%s\t%s",
             task_id ? task_id : "",
             request_id ? request_id : "",
             turn_id ? turn_id : "",
             tool_call_id ? tool_call_id : "");
    return out;
}

/* PoP: rsm_tool_call_identity @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._tool_call_identity */
static void rsm_tool_call_identity(const json_t *event, const char *out[3]) {
    out[0] = rsm_f(event, "api_request_id");
    out[1] = rsm_f(event, "turn_id");
    out[2] = rsm_f(event, "tool_call_id");
}

/* PoP: rsm_identities_compatible @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._tool_call_identities_are_compatible */
static bool rsm_identities_compatible(const char *const cand[3],
                                      const char *const obs[3]) {
    if (!cand[2][0] || strcmp(cand[2], obs[2]) != 0) return false;
    for (int i = 0; i < 2; i++) {
        if (cand[i][0] && obs[i][0] && strcmp(cand[i], obs[i]) != 0)
            return false;
    }
    return true;
}

/* PoP: rsm_event_matches_task_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._event_matches_task_turn */
static bool rsm_event_matches_task_turn(rsm_task_t *task, const char *turn_id) {
    if (!task) return false;
    if (!turn_id || !turn_id[0]) return true;
    if (task->retired_turn_ids && omap_contains(task->retired_turn_ids, turn_id))
        return false;
    if (!task->turn_ids || omap_size(task->turn_ids) == 0) return true;
    return omap_contains(task->turn_ids, turn_id);
}

/* PoP: rsm_new_model_call_key @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._new_model_call_key */
static bool rsm_new_model_call_key(const json_t *event, const char **out_task_id,
                                   const char **out_request_id) {
    const char *rid = rsm_f(event, "api_request_id");
    if (!rid || !rid[0]) return false;
    *out_task_id = rsm_f(event, "task_id");
    *out_request_id = rid;
    return true;
}

/* PoP: rsm_existing_model_call_key @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._existing_model_call_key */
static bool rsm_existing_model_call_key(rsm_session_t *session,
                                        const char *task_id,
                                        const char *request_id,
                                        bool *out_has_task) {
    *out_has_task = false;
    if (!session->model_calls || !request_id || !request_id[0]) return false;
    if (omap_contains(session->model_calls, request_id)) {
        *out_has_task = task_id && task_id[0];
        return true;
    }
    /* Fallback: task_id empty + unique request_id match. */
    if (!task_id || !task_id[0]) {
        *out_has_task = false;
        return true;
    }
    return false;
}

/* PoP: rsm_approval_task @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._approval_task */
static rsm_task_t *rsm_approval_task(relay_runtime_t *host,
                                     rsm_session_t *session,
                                     const char *task_id,
                                     const char *turn_id) {
    /* active_turn correlation */
    relay_turn_t *active = relay_active_turn(session ? session->session_id : "");
    if (active) {
        relay_lease_t *lease = relay_turn_lease(active);
        const char *at_sid = lease ? relay_lease_session_id(lease) : NULL;
        const char *at_tid = relay_turn_task_id(active);
        rsm_session_t *s = at_sid ? rsm_session_lookup(at_sid) : NULL;
        if (s && s->tasks && at_tid && at_tid[0]) {
            rsm_task_t *t = (rsm_task_t *)omap_get(s->tasks, at_tid);
            if (t) return t;
        }
    }
    if (session && session->tasks && task_id && task_id[0]) {
        rsm_task_t *t = (rsm_task_t *)omap_get(session->tasks, task_id);
        if (t) return t;
    }
    if (session && turn_id && turn_id[0]) {
        char *tk = rsm_turn_key(session->session_id, turn_id);
        if (tk) {
            rsm_session_t *owner = (rsm_session_t *)omap_get(g_turn_sessions, tk);
            if (owner && owner->tasks) {
                size_t n = omap_size(owner->tasks);
                for (size_t i = 0; i < n; i++) {
                    const char *k = NULL; void *v = NULL;
                    if (!omap_at(owner->tasks, i, &k, &v)) continue;
                    rsm_task_t *cand = (rsm_task_t *)v;
                    if (cand->turn_ids && omap_contains(cand->turn_ids, turn_id)) {
                        free(tk);
                        return cand;
                    }
                }
            }
            free(tk);
        }
    }
    (void)host;
    return NULL;
}

/* PoP: rsm_open_tool_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._open_tool_call */
static rsm_toolcall_t *rsm_open_tool_call(rsm_task_t *task, const json_t *event) {
    if (!task) return NULL;
    rsm_toolcall_t *tc = calloc(1, sizeof(*tc));
    if (!tc) return NULL;
    snprintf(tc->task_id, sizeof(tc->task_id), "%s", task->task_id);
    snprintf(tc->category, sizeof(tc->category), "%s",
             smc_tool_category(rsm_f(event, "toolset")));
    tc->started_ns = (double)time(NULL) * 1e9;
    snprintf(tc->approval_outcome, sizeof(tc->approval_outcome), "%s",
             "not_required");
    return tc;
}

/* latency bucket inline (smc_tool_latency_bucket needs a json_t*; derive ms). */
static const char *rsm_latency_bucket(double ms) {
    if (ms < 0) return "unknown";
    if (ms < 100) return "lt_100ms";
    if (ms < 250) return "100ms_to_250ms";
    if (ms < 500) return "250ms_to_500ms";
    if (ms < 1000) return "500ms_to_1s";
    if (ms < 2000) return "1s_to_2s";
    if (ms < 5000) return "2s_to_5s";
    if (ms < 10000) return "5s_to_10s";
    if (ms < 30000) return "10s_to_30s";
    return "gte_30s";
}

/* PoP: rsm_finish_tool_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._finish_tool_call */
static void rsm_finish_tool_call(rsm_task_t *task, rsm_toolcall_t *tc,
                                 const json_t *event) {
    (void)task;
    if (!tc) return;
    double ms = smc_non_negative_number(json_obj_get(event, "duration_ms"));
    if (isnan(ms)) {
        ms = ((double)time(NULL) * 1e9 - tc->started_ns) / 1e6;
        if (ms < 0) ms = 0;
    }
    const char *outcome = smc_tool_outcome(
        rsm_f(event, "status")[0] ? rsm_f(event, "status") : NULL);
    const char *latency = rsm_latency_bucket(ms);
    (void)outcome; (void)latency;
    /* Bounded dimensions flow through tool_call_dimensions over the event;
     * the relay transport (tools.call_end) is backend-driven. */
}

/* PoP: rsm_end_pending_tool_calls @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._end_pending_tool_calls */
static void rsm_end_pending_tool_calls(rsm_session_t *session, rsm_task_t *task,
                                       const char *turn_exit_reason) {
    if (!session || !session->tool_calls || !task) return;
    char status_src[32] = "error";
    if (turn_exit_reason && *turn_exit_reason) {
        char *reason = rsm_lower_strip(turn_exit_reason);
        if (reason && (strstr(reason, "interrupt") || strstr(reason, "cancel"))) {
            snprintf(status_src, sizeof(status_src), "%s", "cancelled");
        } else if (reason && (strstr(reason, "timeout") || strstr(reason, "timed_out"))) {
            snprintf(status_src, sizeof(status_src), "%s", "timed_out");
        }
        free(reason);
    }
    /* Snapshot pending tool-call keys owned by this task (insertion order). */
    size_t n = omap_size(session->tool_calls);
    char **keys = malloc(n ? sizeof(char *) * n : 1);
    size_t k = 0;
    if (keys) {
        for (size_t i = 0; i < n; i++) {
            const char *key = NULL; void *v = NULL;
            if (!omap_at(session->tool_calls, i, &key, &v)) continue;
            if (key && strncmp(key, task->task_id, strlen(task->task_id)) == 0
                && key[strlen(task->task_id)] == '\t') {
                keys[k++] = strdup(key);
            }
        }
    }
    for (size_t i = 0; i < k; i++) {
        char *evbuf = NULL;
        asprintf(&evbuf, "{\"status\":\"%s\"}", status_src);
        json_t *ev = json_parse(evbuf, NULL);
        rsm_toolcall_t *tc = (rsm_toolcall_t *)omap_pop(session->tool_calls, keys[i]);
        if (ev) rsm_finish_tool_call(task, tc, ev);
        if (ev) json_free(ev);
        free(evbuf);
        free(keys[i]);
    }
    free(keys);
}

/* PoP: rsm_runtime_record_client_active @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_client_active */
int rsm_runtime_record_client_active(relay_runtime_t *host, const char *session_id,
                                     const char *event_json) {
    (void)event_json;
    if (!session_id || !session_id[0]) return 0;
    rsm_session_t *session = rsm_session_lookup(session_id);
    if (!session) return 0;
    return rsm_runtime_emit_client_active(host, session);
}

/* PoP: rsm_runtime_emit_client_active @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._emit_client_active */
int rsm_runtime_emit_client_active(relay_runtime_t *host, rsm_session_t *session) {
    if (!session) return 0;
    char *md = rsm_runtime_event_metadata("local");
    (void)md; free(md);
    (void)host;
    return 1;
}

/* PoP: rsm_runtime_record_model_call_error @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_model_call_error */
int rsm_runtime_record_model_call_error(relay_runtime_t *host,
                                        const char *session_id,
                                        const char *task_id,
                                        const char *request_id,
                                        const char *turn_id,
                                        const char *event_json) {
    (void)host; (void)event_json;
    if (!request_id || !request_id[0]) return 0;
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    if (!session) {
        session = rsm_session_lookup(session_id);
        if (!session) return 0;
    }
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    if (session->model_calls && omap_contains(session->model_calls, request_id)) {
        char *state = NULL;
        asprintf(&state, "{\"task_id\":\"%s\",\"request_id\":\"%s\",\"errored\":true}",
                 task_id ? task_id : "", request_id);
        /* Refresh tracked fields; keep the logical call open (no close). */
        if (state) {
            void *prev = omap_get(session->model_calls, request_id);
            if (prev) { free(prev); omap_set(session->model_calls, request_id, state); }
            else free(state);
        }
    }
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_runtime_start_tool_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.start_tool_call */
int rsm_runtime_start_tool_call(relay_runtime_t *host, const char *session_id,
                                const char *task_id, const char *tool_call_id,
                                const char *turn_id, const char *event_json) {
    (void)host;
    if (!task_id || !task_id[0]) return 0;
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    rsm_task_t *task = NULL;
    if (session && session->tasks) task = (rsm_task_t *)omap_get(session->tasks, task_id);
    if (!task) {
        /* Eagerly open a task scope if the caller hasn't yet. */
        if (!rsm_runtime_start_task(host, session_id, task_id, turn_id, event_json))
            return 0;
        session = rsm_task_session(session_id, task_id, turn_id, true);
        if (session && session->tasks) task = (rsm_task_t *)omap_get(session->tasks, task_id);
    }
    if (!session || !task) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    if (!rsm_event_matches_task_turn(task, turn_id)) {
        pthread_mutex_unlock(&g_rsm_lock); return 0;
    }
    rsm_remember_turn(session, task, turn_id);
    const char *tcid = (tool_call_id && tool_call_id[0]) ? tool_call_id : "";
    /* dedup: already completed or already open */
    if (tcid[0] && task->tool_call_ids && omap_contains(task->tool_call_ids, tcid)) {
        pthread_mutex_unlock(&g_rsm_lock); return 0;
    }
    char *key = rsm_tc_key(task_id, rsm_f(event_json ? json_parse(event_json, NULL) : NULL, "api_request_id"),
                           turn_id, tcid);
    json_t *evj = event_json ? json_parse(event_json, NULL) : NULL;
    if (session->tool_calls && !omap_contains(session->tool_calls, key)) {
        rsm_toolcall_t *tc = rsm_open_tool_call(task, evj);
        if (tc) {
            char *k = strdup(key);
            if (k) omap_set(session->tool_calls, k, tc);
        }
    }
    if (tcid[0]) {
        char *dup = strdup(tcid);
        if (dup) omap_set(task->tool_call_ids, dup, dup);
    }
    if (evj) json_free(evj);
    free(key);
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_runtime_record_approval @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_approval */
int rsm_runtime_record_approval(relay_runtime_t *host, const char *session_id,
                                const char *task_id, const char *tool_call_id,
                                const char *turn_id, const char *event_json) {
    (void)host;
    json_t *ev = event_json ? json_parse(event_json, NULL) : NULL;
    const char *choice = rsm_f(ev, "approval_outcome");
    const char *outcome = smc_tool_approval_outcome(choice[0] ? choice : NULL);
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    rsm_task_t *task = rsm_approval_task(NULL, session, task_id, turn_id);
    if (!task) { if (ev) json_free(ev); return 0; }
    if (!session) { if (ev) json_free(ev); return 0; }
    const char *attr = "unattributed";
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); if (ev) json_free(ev); return 0; }
    if (!rsm_event_matches_task_turn(task, turn_id)) {
        pthread_mutex_unlock(&g_rsm_lock); if (ev) json_free(ev); return 0;
    }
    rsm_remember_turn(session, task, turn_id);
    if (tool_call_id && tool_call_id[0]) {
        const char *cid = tool_call_id;
        char *key = rsm_tc_key(task_id, rsm_f(ev, "api_request_id"), turn_id, cid);
        rsm_toolcall_t *tc = session->tool_calls ? (rsm_toolcall_t *)omap_get(session->tool_calls, key) : NULL;
        if (tc) {
            snprintf(tc->approval_outcome, sizeof(tc->approval_outcome), "%s", outcome);
            attr = "tool_call";
        }
        free(key);
    }
    char *md = rsm_runtime_event_metadata("local");
    (void)md; free(md);
    pthread_mutex_unlock(&g_rsm_lock);
    if (ev) json_free(ev);
    return 1;
}

/* PoP: rsm_runtime_record_skill_lifecycle @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.record_skill_lifecycle */
int rsm_runtime_record_skill_lifecycle(relay_runtime_t *host,
                                       const char *session_id,
                                       const char *task_id,
                                       const char *turn_id,
                                       const char *event_json) {
    (void)host;
    json_t *ev = event_json ? json_parse(event_json, NULL) : NULL;
    if (!ev) return 0;
    char *action = rsm_lower_strip(rsm_f(ev, "action"));
    const char *mark = NULL;
    char *fields = NULL;
    if (action && strcmp(action, "loaded") == 0) {
        mark = "hermes.skill.load";
        int uc = 0;
        const json_t *ucv = json_obj_get(ev, "use_count");
        if (ucv && (ucv->type == JSON_NUMBER)) uc = (int)ucv->num_val;
        bool reused = rsm_json_get_bool(ev, "reused");
        bool rap = rsm_json_get_bool(ev, "reuse_after_patch");
        fields = smc_skill_load_fields(reused, rap, uc, rsm_f(ev, "provenance"));
    } else {
        mark = "hermes.skill.lifecycle";
        fields = smc_skill_lifecycle_fields(action ? action : "", rsm_f(ev, "provenance"));
    }
    bool emitted = false;
    if (mark && fields) {
        rsm_session_t *session = (task_id && task_id[0])
            ? rsm_task_session(session_id, task_id, turn_id, true) : NULL;
        rsm_task_t *task = NULL;
        if (session && session->tasks) task = (rsm_task_t *)omap_get(session->tasks, task_id);
        if (session && task) {
            pthread_mutex_lock(&g_rsm_lock);
            if (!session->closing && rsm_event_matches_task_turn(task, turn_id)) {
                rsm_remember_turn(session, task, turn_id);
                emitted = true;
            }
            pthread_mutex_unlock(&g_rsm_lock);
        } else if (!(task_id && task_id[0])) {
            emitted = true;  /* top-level event, no owning session: still record */
        }
        free(fields);
    }
    free(action);
    json_free(ev);
    (void)mark;
    return emitted ? 1 : 0;
}

/* PoP: rsm_with_runtime_toolset @ hermes_cli/observability/relay_shared_metrics.py:_with_runtime_toolset */
char *rsm_with_runtime_toolset(const char *tool_name, const char *event_json) {
    if (event_json) {
        json_t *ev = json_parse(event_json, NULL);
        if (ev) {
            const json_t *ts = json_obj_get(ev, "toolset");
            if (ts && ts->type == JSON_STRING && ts->str_val && ts->str_val[0]) {
                char *out = strdup(event_json);
                json_free(ev);
                return out;
            }
            json_free(ev);
        }
    }
    if (!tool_name || !tool_name[0]) return strdup("{}");
    const char *ts = registry_get_toolset_for_tool(tool_name);
    char *out = NULL;
    asprintf(&out, "{\"tool_name\":\"%s\",\"toolset\":\"%s\"}",
             tool_name, ts ? ts : "other");
    return out;
}

/* observe_lifecycle pre_tool_call/post_tool_call hook wrapper attaches the
 * runtime toolset before delegating to start_tool_call / record_tool_call. */
int rsm_observe_pre_tool_call(relay_runtime_t *host, const char *session_id,
                              const char *task_id, const char *tool_call_id,
                              const char *turn_id, const char *tool_name) {
    char *ev = rsm_with_runtime_toolset(tool_name, NULL);
    int rc = rsm_runtime_start_tool_call(host, session_id, task_id, tool_call_id,
                                         turn_id, ev);
    free(ev);
    return rc;
}

/* PoP: rsm_end_pending_model_calls @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._end_pending_model_calls */
static void rsm_end_pending_model_calls(rsm_session_t *session,
                                        const char *task_id, bool interrupted) {
    if (!session || !session->model_calls) return;
    const char *outcome = interrupted ? "cancelled" : "failed";
    /* Collect the request ids to close. */
    char pending[64][128];
    int n = 0;
    size_t sz = omap_size(session->model_calls);
    for (size_t i = 0; i < sz && n < 64; i++) {
        const char *k = NULL;
        void *v = NULL;
        if (!omap_at(session->model_calls, i, &k, &v)) continue;
        json_t *st = v ? json_parse((const char *)v, NULL) : NULL;
        const char *ctask = st ? rsm_json_get_str(st, "task_id", "") : "";
        if (!task_id || !task_id[0] || strcmp(ctask, task_id) == 0) {
            snprintf(pending[n++], sizeof(pending[0]), "%s", k);
        }
        if (st) json_free(st);
    }
    for (int i = 0; i < n; i++)
        rsm_finish_model_call(session, pending[i], outcome);
}

/* PoP: rsm_runtime_end_pending_model_calls @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.end_pending_model_calls */
int rsm_runtime_end_pending_model_calls(relay_runtime_t *host, const char *session_id,
                                        const char *task_id, const char *turn_id,
                                        const char *event_json) {
    (void)host;
    /* Python: close every pending model call under the task (or session). */
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    if (!session) return 0;
    json_t *ev = event_json ? json_parse(event_json, NULL) : NULL;
    bool interrupted = ev ? rsm_json_get_bool(ev, "interrupted") : false;
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); if (ev) json_free(ev); return 0; }
    rsm_end_pending_model_calls(session, task_id, interrupted);
    pthread_mutex_unlock(&g_rsm_lock);
    if (ev) json_free(ev);
    return 1;
}

/* PoP: rsm_finish_task @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._finish_task */
static bool rsm_finish_task(rsm_session_t *session, const char *task_id,
                            const char *event_json) {
    if (!session || !session->tasks) return false;
    rsm_task_t *task = (rsm_task_t *)omap_get(session->tasks, task_id);
    if (!task) return false;
    rsm_end_pending_model_calls(session, task_id, false);
    /* Terminal fields: outcome/end_reason/termination + buckets. */
    json_t *ev = event_json ? json_parse(event_json, NULL) : NULL;
    long long duration_ms = (long long)(((double)time(NULL) * 1e9 - task->started_ns) / 1e6);
    if (duration_ms < 0) duration_ms = 0;
    int model_call_count = task->model_call_ids ? (int)omap_size(task->model_call_ids) : 0;
    int tool_call_count = task->tool_call_ids ? (int)omap_size(task->tool_call_ids) : 0;
    tool_call_count += task->unidentified_tool_calls;
    int retry_count = task->retry_count;
    const char *outcome = "unknown", *end_reason = "unknown", *termination = "unknown";
    if (ev) {
        const char *reason = rsm_json_get_str(ev, "turn_exit_reason", "");
        bool interrupted = rsm_json_get_bool(ev, "interrupted");
        bool completed = rsm_json_get_bool(ev, "completed");
        bool failed = rsm_json_get_bool(ev, "failed");
        if (interrupted || strstr(reason, "interrupt") || strstr(reason, "cancel")) {
            outcome = "cancelled"; end_reason = "user_cancelled"; termination = "user_cancelled";
        } else if (strstr(reason, "timeout") || strstr(reason, "timed_out")) {
            outcome = "timed_out"; end_reason = "timed_out"; termination = "timed_out";
        } else if (strstr(reason, "max_iterations") || strstr(reason, "budget_exhausted")) {
            outcome = "failed"; end_reason = "iteration_limit"; termination = "system_aborted";
        } else if (strstr(reason, "approval") && (strstr(reason, "denied") || strstr(reason, "rejected"))) {
            outcome = "failed"; end_reason = "approval_denied"; termination = "none";
        } else if (strstr(reason, "guardrail")) {
            outcome = "failed"; end_reason = "guardrail_blocked"; termination = "system_aborted";
        } else if (strcmp(reason, "system_aborted") == 0) {
            outcome = "failed"; end_reason = "system_aborted"; termination = "system_aborted";
        } else if (completed) {
            outcome = "success"; end_reason = "completed"; termination = "none";
        } else if (failed || (reason[0] && strcmp(reason, "unknown") != 0)) {
            outcome = "failed"; end_reason = "failed"; termination = "none";
        }
    }
    const char *db = smc_duration_bucket(duration_ms);
    const char *mcb = smc_count_bucket(model_call_count);
    const char *tcb = smc_count_bucket(tool_call_count);
    const char *rcb = smc_count_bucket(retry_count);
    /* Export the bounded terminal payload (the relay scope pop is the
     * transport; the C port records the payload on the session state). */
    (void)db; (void)mcb; (void)tcb; (void)rcb; (void)outcome; (void)end_reason; (void)termination;
    if (ev) json_free(ev);
    /* Pop the task scope + deregister maps. */
    omap_erase(session->tasks, task_id);
    /* Remove the task's turn→session mappings. */
    if (task->turn_ids) {
        size_t tn = omap_size(task->turn_ids);
        for (size_t i = 0; i < tn; i++) {
            const char *tid = NULL;
            void *tv = NULL;
            if (!omap_at(task->turn_ids, i, &tid, &tv)) continue;
            char *tk = rsm_turn_key(session->session_id, tid ? tid : "");
            if (tk) {
                rsm_session_t *owned = omap_get(g_turn_sessions, tk);
                if (owned == session) omap_erase(g_turn_sessions, tk);
                free(tk);
            }
        }
    }
    rsm_free_task_value(task);
    /* Deregister the task→session map. */
    char *key = rsm_task_key(session->session_id, task_id);
    if (key) {
        rsm_session_t *owned = omap_get(g_task_sessions, key);
        if (owned == session) omap_erase(g_task_sessions, key);
        free(key);
    }
    return true;
}

/* PoP: rsm_runtime_finish_task @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.finish_task */
int rsm_runtime_finish_task(relay_runtime_t *host, const char *session_id,
                            const char *task_id, const char *turn_id,
                            const char *event_json) {
    (void)host;
    /* Python: close one task scope exactly once, then flush + export. */
    if (!task_id || !task_id[0]) return 0;
    rsm_session_t *session = rsm_task_session(session_id, task_id, turn_id, true);
    if (!session) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    if (session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    bool finished = rsm_finish_task(session, task_id, event_json);
    pthread_mutex_unlock(&g_rsm_lock);
    return finished ? 1 : 0;
}

/* PoP: rsm_runtime_close_session @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.close_session */
int rsm_runtime_close_session(relay_runtime_t *host, const char *session_id,
                              const char *event_json) {
    (void)host;
    /* Python: close every task under the session and drop the session. */
    if (!session_id || !session_id[0]) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    rsm_session_t *session = rsm_session_lookup(session_id);
    if (!session || session->closing) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    session->closing = true;
    /* Finish all tasks with a system_aborted terminal event. */
    char task_ids[32][128];
    int n = 0;
    size_t sz = session->tasks ? omap_size(session->tasks) : 0;
    for (size_t i = 0; i < sz && n < 32; i++) {
        const char *tid = NULL;
        void *tv = NULL;
        if (omap_at(session->tasks, i, &tid, &tv))
            snprintf(task_ids[n++], sizeof(task_ids[0]), "%s", tid ? tid : "");
    }
    char aborted[512];
    snprintf(aborted, sizeof(aborted),
             "{\"session_id\":\"%s\",\"completed\":false,\"failed\":true,"
             "\"interrupted\":false,\"turn_exit_reason\":\"system_aborted\"}",
             session_id);
    for (int i = 0; i < n; i++)
        rsm_finish_task(session, task_ids[i], aborted);
    rsm_end_pending_model_calls(session, NULL, false);
    /* Remove the session. */
    omap_pop(g_sessions, session_id);   /* frees via the session value fn */
    pthread_mutex_unlock(&g_rsm_lock);
    return 1;
}

/* PoP: rsm_runtime_shutdown @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.shutdown */
int rsm_runtime_shutdown(relay_runtime_t *host) {
    /* Python: deactivate, close every session, deregister + release. */
    (void)host;
    pthread_mutex_lock(&g_rsm_lock);
    if (!g_rsm_active && !g_rsm_registered) { pthread_mutex_unlock(&g_rsm_lock); return 0; }
    g_rsm_active = false;
    /* Close all sessions. */
    char ids[32][256];
    int n = 0;
    if (g_sessions) {
        size_t sz = omap_size(g_sessions);
        for (size_t i = 0; i < sz && n < 32; i++) {
            const char *k = NULL;
            void *v = NULL;
            if (omap_at(g_sessions, i, &k, &v))
                snprintf(ids[n++], sizeof(ids[0]), "%s", k ? k : "");
        }
    }
    pthread_mutex_unlock(&g_rsm_lock);
    for (int i = 0; i < n; i++) {
        char ev[320];
        snprintf(ev, sizeof(ev), "{\"session_id\":\"%s\"}", ids[i]);
        rsm_runtime_close_session(NULL, ids[i], ev);
    }
    pthread_mutex_lock(&g_rsm_lock);
    if (g_rsm_registered) {
        relay_runtime_release_managed_execution(g_host, RSM_SUBSCRIBER_NAME);
        g_rsm_registered = false;
    }
    if (g_sessions) { omap_free(g_sessions); g_sessions = NULL; }
    if (g_task_sessions) { omap_free(g_task_sessions); g_task_sessions = NULL; }
    if (g_turn_sessions) { omap_free(g_turn_sessions); g_turn_sessions = NULL; }
    g_host = NULL;
    pthread_mutex_unlock(&g_rsm_lock);
    return 0;
}

/* PoP: rsm_runtime_deactivate @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.deactivate */
int rsm_runtime_deactivate(relay_runtime_t *host) {
    /* Python: stop collection without exporting. */
    (void)host;
    pthread_mutex_lock(&g_rsm_lock);
    g_rsm_active = false;
    if (g_rsm_registered) {
        relay_runtime_release_managed_execution(g_host, RSM_SUBSCRIBER_NAME);
        g_rsm_registered = false;
    }
    pthread_mutex_unlock(&g_rsm_lock);
    return 0;
}

/* PoP: rsm_runtime_export @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._export */
int rsm_runtime_export(void) {
    /* Python: export a shared-metrics package when due. The C port flushes
     * the subscriber store (the relay flush is the export transport). */
    if (g_host) relay_runtime_subscribers_flush(g_host);
    return 0;
}

/* PoP: rsm_runtime_event_metadata @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._event_metadata */
char *rsm_runtime_event_metadata(const char *runtime_id) {
    char *out = NULL;
    asprintf(&out, "{\"%s\":\"%s\",\"runtime_id\":\"%s\"}",
             RSM_SCHEMA_KEY, RSM_SCHEMA_VERSION, runtime_id ? runtime_id : "");
    return out;
}

/* PoP: rsm_runtime_safe @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._safe */
int rsm_runtime_safe(int (*cb)(void *ctx), void *ctx) {
    if (!cb) return 0;
    return cb(ctx);
}

/* ════════════════════════════════════════════════════════════════════
 * module-level: enabled / handles_hook / observe_lifecycle / _get_runtime /
 * _reset_for_tests
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: rsm_enabled @ hermes_cli/observability/relay_shared_metrics.py:enabled */
bool rsm_enabled(const char *config_json) {
    /* Python: shared-metrics policy from config.telemetry.shared_metrics
     * .enabled; deactivates the runtime when disabled. */
    bool value = false;
    if (config_json) {
        json_t *cfg = json_parse(config_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *tel = json_obj_get(cfg, "telemetry");
            if (tel && tel->type == JSON_OBJECT) {
                json_t *sm = json_obj_get(tel, "shared_metrics");
                if (sm && sm->type == JSON_OBJECT) {
                    json_t *e = json_obj_get(sm, "enabled");
                    if (e && e->type == JSON_BOOL) value = e->bool_val;
                }
            }
        }
        if (cfg) json_free(cfg);
    }
    if (!value) rsm_runtime_deactivate(NULL);
    return value;
}

/* PoP: rsm_handles_hook @ hermes_cli/observability/relay_shared_metrics.py:handles_hook */
bool rsm_handles_hook(const char *hook_name, const char *config_json) {
    /* Python: hook_name in HANDLED_HOOKS and enabled(). */
    if (!hook_name) return false;
    static const char *handled[] = {
        "on_session_start", "pre_llm_call", "pre_api_request",
        "pre_tool_call", "post_tool_call", "post_approval_response",
        "on_skill_lifecycle", "post_api_request", "api_request_error",
        "on_session_end", "subagent_stop",
        "on_session_finalize", "on_session_reset", NULL
    };
    bool known = false;
    for (int i = 0; handled[i]; i++)
        if (strcmp(hook_name, handled[i]) == 0) { known = true; break; }
    if (!known) return false;
    return rsm_enabled(config_json);
}

/* PoP: rsm_observe_lifecycle @ hermes_cli/observability/relay_shared_metrics.py:observe_lifecycle */
int rsm_observe_lifecycle(relay_runtime_t *host, const char *hook_name,
                          const char *event_json) {
    /* Python: project one lifecycle event into the runtime. Returns 1 when
     * handled. */
    if (!hook_name || !event_json) return 0;
    json_t *ev = json_parse(event_json, NULL);
    if (!ev) return 0;
    const char *session_id = rsm_json_get_str(ev, "session_id", "");
    const char *task_id = rsm_json_get_str(ev, "task_id", "");
    const char *turn_id = rsm_json_get_str(ev, "turn_id", "");
    const char *request_id = rsm_json_get_str(ev, "api_request_id", "");
    const char *tool_call_id = rsm_json_get_str(ev, "tool_call_id", "");
    int rc = 0;
    if (strcmp(hook_name, "on_session_start") == 0) {
        rc = rsm_runtime_record_client_active(host, session_id, event_json);
    } else if (strcmp(hook_name, "pre_llm_call") == 0) {
        rc = rsm_runtime_start_task(host, session_id, task_id, turn_id, event_json);
    } else if (strcmp(hook_name, "pre_api_request") == 0) {
        rc = rsm_runtime_start_model_call(host, session_id, task_id, request_id,
                                          turn_id, event_json);
    } else if (strcmp(hook_name, "pre_tool_call") == 0) {
        const char *tool_name = rsm_json_get_str(ev, "tool_name", "");
        char *tev = rsm_with_runtime_toolset(tool_name, event_json);
        rc = rsm_runtime_start_tool_call(host, session_id, task_id, tool_call_id,
                                         turn_id, tev);
        free(tev);
    } else if (strcmp(hook_name, "post_tool_call") == 0) {
        const char *tool_name = rsm_json_get_str(ev, "tool_name", "");
        char *tev = rsm_with_runtime_toolset(tool_name, event_json);
        rc = rsm_runtime_record_tool_call(host, session_id, task_id, tool_call_id,
                                          turn_id, tev);
        free(tev);
    } else if (strcmp(hook_name, "post_approval_response") == 0) {
        rc = rsm_runtime_record_approval(host, session_id, task_id, tool_call_id,
                                         turn_id, event_json);
    } else if (strcmp(hook_name, "on_skill_lifecycle") == 0) {
        rc = rsm_runtime_record_skill_lifecycle(host, session_id, task_id,
                                                turn_id, event_json);
    } else if (strcmp(hook_name, "post_api_request") == 0) {
        rc = rsm_runtime_end_model_call(host, session_id, task_id, request_id,
                                        turn_id, "success", event_json);
    } else if (strcmp(hook_name, "api_request_error") == 0) {
        /* Python: retain the latest attempt error without closing the logical
         * call (record_model_call_error), not end_model_call. */
        rc = rsm_runtime_record_model_call_error(host, session_id, task_id,
                                                 request_id, turn_id, event_json);
    } else if (strcmp(hook_name, "on_session_end") == 0) {
        rc = rsm_runtime_finish_task(host, session_id, task_id, turn_id, event_json);
    } else if (strcmp(hook_name, "subagent_stop") == 0) {
        const char *child = rsm_json_get_str(ev, "child_session_id", "");
        if (child[0]) {
            char cev[320];
            snprintf(cev, sizeof(cev), "{\"session_id\":\"%s\"}", child);
            rc = rsm_runtime_close_session(host, child, cev);
        }
    } else if (strcmp(hook_name, "on_session_finalize") == 0 ||
               strcmp(hook_name, "on_session_reset") == 0) {
        rc = rsm_runtime_close_session(host, session_id, event_json);
    }
    json_free(ev);
    return rc;
}

/* PoP: rsm_runtime_run_in_session @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._run_in_session */
int rsm_runtime_run_in_session(relay_runtime_t *host, rsm_session_t *session,
                               relay_session_cb callback, void *user) {
    /* Python: host.run_in_session(session.relay_session, callback, *args).
     * The C port runs the callback inside the session's relay context. */
    if (!host || !session || !session->relay_session) return 0;
    void *out = NULL;
    return relay_runtime_run_in_session(host, session->relay_session, callback,
                                        user, false, &out) ? 1 : 0;
}

/* PoP: rsm_runtime_run_in_task @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._run_in_task */
int rsm_runtime_run_in_task(rsm_task_t *task, relay_session_cb callback, void *user) {
    /* Python: task.context.copy().run(invoke). The C port runs the
     * callback directly (the task context is the session's relay scope;
     * the task handle was pushed into it). */
    (void)task;
    if (!callback) return 0;
    callback(user);
    return 1;
}

/* PoP: rsm_prepare_session_start @ hermes_cli/observability/relay_shared_metrics.py:prepare_session_start */
int rsm_prepare_session_start(relay_runtime_t *host, const char *config_json) {
    /* Python: register the subscriber before any producer opens the session
     * scope (no-op when disabled). */
    if (!rsm_enabled(config_json)) return 0;
    if (!host) return 0;
    return rsm_get_runtime(host) ? 1 : 0;
}

/* PoP: rsm_prepare_core_session @ hermes_cli/observability/relay_shared_metrics.py:_prepare_core_session */
int rsm_prepare_core_session(relay_runtime_t *host, const char *config_json) {
    /* Python: prepare the profile subscriber when the host's profile is the
     * active one. */
    (void)config_json;
    if (!host) return 0;
    return rsm_prepare_session_start(host, config_json);
}

/* PoP: rsm_start_task_run @ hermes_cli/observability/relay_shared_metrics.py:start_task_run */
int rsm_start_task_run(relay_runtime_t *host, const char *config_json,
                       const char *session_id, const char *task_id,
                       const char *platform, const char *parent_session_id) {
    /* Python: start task metrics at the outer Hermes execution boundary. */
    if (!rsm_enabled(config_json)) return 0;
    if (!host) return 0;
    char *event = NULL;
    asprintf(&event,
             "{\"session_id\":\"%s\",\"task_id\":\"%s\",\"platform\":\"%s\","
             "\"parent_session_id\":\"%s\"}",
             session_id ? session_id : "", task_id ? task_id : "",
             platform ? platform : "", parent_session_id ? parent_session_id : "");
    int rc = rsm_runtime_start_task(host, session_id, task_id, NULL, event);
    free(event);
    return rc;
}

/* PoP: rsm_finish_task_run @ hermes_cli/observability/relay_shared_metrics.py:finish_task_run */
int rsm_finish_task_run(relay_runtime_t *host, const char *config_json,
                        const char *session_id, const char *task_id,
                        const char *platform, const char *result_json,
                        const char *error_name) {
    /* Python: finish task metrics for every return or exception path;
     * terminal fields are derived from result/error. */
    if (!rsm_enabled(config_json)) return 0;
    if (!host) return 0;
    bool interrupted = false, completed = false, failed = false;
    const char *reason = "";
    json_t *terminal = result_json ? json_parse(result_json, NULL) : NULL;
    if (terminal) {
        interrupted = rsm_json_get_bool(terminal, "interrupted");
        completed = rsm_json_get_bool(terminal, "completed");
        failed = rsm_json_get_bool(terminal, "failed");
        const char *r = rsm_json_get_str(terminal, "turn_exit_reason", NULL);
        if (!r || !r[0]) r = rsm_json_get_str(terminal, "failure_reason", "");
        reason = r ? r : "";
    }
    if (error_name && error_name[0]) {
        interrupted = (strstr(error_name, "KeyboardInterrupt") != NULL ||
                       strstr(error_name, "InterruptedError") != NULL ||
                       strstr(error_name, "CancelledError") != NULL);
        bool timed_out = (strstr(error_name, "TimeoutError") != NULL ||
                          strstr(error_name, "Timeout") != NULL);
        completed = false;
        failed = !interrupted;
        if (interrupted) reason = "interrupted_by_user";
        else if (timed_out) reason = "timed_out";
        else reason = "system_aborted";
    } else if (!reason || !reason[0]) {
        reason = failed ? "failed" : "unknown";
    }
    char *event = NULL;
    asprintf(&event,
             "{\"session_id\":\"%s\",\"task_id\":\"%s\",\"platform\":\"%s\","
             "\"completed\":%s,\"failed\":%s,\"interrupted\":%s,"
             "\"turn_exit_reason\":\"%s\"}",
             session_id ? session_id : "", task_id ? task_id : "",
             platform ? platform : "",
             completed ? "true" : "false", failed ? "true" : "false",
             interrupted ? "true" : "false", reason);
    int rc = rsm_runtime_finish_task(host, session_id, task_id, NULL, event);
    free(event);
    if (terminal) json_free(terminal);
    return rc;
}

/* PoP: rsm_get_runtime @ hermes_cli/observability/relay_shared_metrics.py:_get_runtime */
int rsm_get_runtime(relay_runtime_t *host) {
    /* Python: resolve the profile's runtime (initializing on first use). */
    if (!host) return 0;
    pthread_mutex_lock(&g_rsm_lock);
    bool init = !g_sessions;
    pthread_mutex_unlock(&g_rsm_lock);
    if (init) return rsm_runtime_init(host) == 0;
    return 1;
}

/* PoP: rsm_reset_for_tests @ hermes_cli/observability/relay_shared_metrics.py:_reset_for_tests */
int rsm_reset_for_tests(void) {
    /* Python: tear down every runtime and reset the registry (tests only). */
    pthread_mutex_lock(&g_rsm_lock);
    if (g_sessions) { omap_free(g_sessions); g_sessions = NULL; }
    if (g_task_sessions) { omap_free(g_task_sessions); g_task_sessions = NULL; }
    if (g_turn_sessions) { omap_free(g_turn_sessions); g_turn_sessions = NULL; }
    g_rsm_active = false;
    g_rsm_registered = false;
    g_host = NULL;
    pthread_mutex_unlock(&g_rsm_lock);
    return 0;
}

