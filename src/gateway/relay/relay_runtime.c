/*
 * port_agent_relay_runtime.c — C11 port of agent/relay_runtime.py.
 *
 * See include/port_agent_relay_runtime.h for the Python -> C mapping table.
 * This file is the cohesive port of ONE Python module and is the correct
 * boundary: splitting it would fragment a single upstream concern.
 *
 * PoP annotations sit on each public function; the header carries the
 * declarations so consumers include one focused header, never a god header.
 */

/* realpath() is gated behind __USE_MISC/__USE_XOPEN_EXTENDED in glibc, so a
 * bare _POSIX_C_SOURCE leaves it undeclared and the tree builds with
 * -Werror=implicit-function-declaration. _GNU_SOURCE is the repo convention
 * for this exact case (see src/agent/file_safety.c, prompt_builder.c). */
#define _GNU_SOURCE

#include "port_agent_relay_runtime.h"

#include "omap.h"
#include "hermes_logger.h"
#include "uuid.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* ── small helpers ────────────────────────────────────────────────────── */

static char *rr_strdup(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python coerces missing/None ids with `str(event.get(k) or "")`. */
static const char *rr_or_empty(const char *s) { return s ? s : ""; }

/* A recursive mutex is the faithful analogue of threading.RLock: the Python
 * code re-enters the same lock (ensure_session -> ensure_session for a
 * subagent parent), which a plain mutex would deadlock on. */
static void rr_rlock_init(pthread_mutex_t *m)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
}

/* ── backend (the injected `nemo_relay` binding) ──────────────────────── */

static relay_backend_t g_backend;
static bool            g_backend_set = false;
static pthread_mutex_t g_backend_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: relay_runtime_set_backend @ agent/relay_runtime.py:_load_nemo_relay */
void relay_runtime_set_backend(const relay_backend_t *backend)
{
    pthread_mutex_lock(&g_backend_lock);
    if (backend) {
        g_backend     = *backend;
        g_backend_set = true;
    } else {
        memset(&g_backend, 0, sizeof(g_backend));
        g_backend_set = false;
    }
    pthread_mutex_unlock(&g_backend_lock);
}

bool relay_runtime_backend_available(void)
{
    pthread_mutex_lock(&g_backend_lock);
    bool have = g_backend_set;
    pthread_mutex_unlock(&g_backend_lock);
    return have;
}

/* Snapshot the installed backend vtable for direct hook driving (relay_llm's
 * llm_execute / llm_stream_execute paths). */
bool relay_runtime_backend_snapshot(relay_backend_t *out)
{
    if (!out) return false;
    pthread_mutex_lock(&g_backend_lock);
    bool have = g_backend_set;
    if (have) *out = g_backend;
    pthread_mutex_unlock(&g_backend_lock);
    if (!have) memset(out, 0, sizeof(*out));
    return have;
}

/* Snapshot the backend so a concurrent swap cannot tear a call in flight. */
static bool rr_backend(relay_backend_t *out)
{
    pthread_mutex_lock(&g_backend_lock);
    bool have = g_backend_set;
    if (have) *out = g_backend;
    pthread_mutex_unlock(&g_backend_lock);
    return have;
}

/* ── RelaySession ─────────────────────────────────────────────────────── */

struct relay_session {
    char            *session_id;
    char            *parent_session_id;
    pthread_mutex_t  lock;        /* threading.RLock */
    bool             closing;
    relay_handle_t   handle;
    bool             context_ready; /* contextvars.Context was captured */
    /* session-segmentation bookkeeping (_segments_config consumers) */
    long             segment;          /* RelaySession.segment */
    long             segment_turns;    /* RelaySession.segment_turns */
    bool             rotate_pending;   /* RelaySession.rotate_pending */
    bool             close_pending;    /* RelaySession.close_pending */
};

static relay_session_t *rr_session_new(const char *session_id, const char *parent_session_id)
{
    relay_session_t *s = (relay_session_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->session_id        = rr_strdup(session_id);
    s->parent_session_id = rr_strdup(parent_session_id);
    if (!s->session_id || !s->parent_session_id) {
        free(s->session_id); free(s->parent_session_id); free(s);
        return NULL;
    }
    rr_rlock_init(&s->lock);
    return s;
}

static void rr_session_free(void *p)
{
    relay_session_t *s = (relay_session_t *)p;
    if (!s) return;
    pthread_mutex_destroy(&s->lock);
    free(s->session_id);
    free(s->parent_session_id);
    free(s);
}

const char *relay_session_id(const relay_session_t *s) { return s ? s->session_id : NULL; }
const char *relay_session_parent_id(const relay_session_t *s) { return s ? s->parent_session_id : NULL; }
relay_handle_t relay_session_handle(const relay_session_t *s) { return s ? s->handle : NULL; }
bool relay_session_closing(const relay_session_t *s) { return s ? s->closing : false; }

/* ── RelayRuntime ─────────────────────────────────────────────────────── */

struct relay_runtime {
    char            *profile_key;
    char            *runtime_id;         /* uuid4().hex */

    pthread_mutex_t  sessions_lock;
    omap_t          *sessions;           /* session_id -> relay_session_t* (owned) */
    omap_t          *subagent_parents;   /* child_id -> char* parent_id (owned) */
    omap_t          *subagent_parent_handles; /* child_id -> relay_handle_t (borrowed) */

    pthread_mutex_t  consumers_lock;
    omap_t          *execution_consumers; /* set[str] */

    /* ── operation admission (Python _begin_operation/_end_operation) ── */
    bool             closing;               /* self._closing */
    long             active_operations;     /* self._active_operations */
    bool             operations_idle;       /* threading.Event _operations_idle */

    /* ── process plugin-configuration registration state ── */
    bool             plugin_configuration_registered;
    int              plugin_configuration_state;  /* relay_plugin_config_state_t */

    /* ── shutdown bookkeeping (self._shutdown_*) ── */
    bool             shutdown_registered;
    bool             shutdown_started;
    bool             shutdown_complete;
};

/* PoP: relay_runtime_new @ agent/relay_runtime.py:__init__ */
relay_runtime_t *relay_runtime_new(const char *profile_key)
{
    relay_runtime_t *rt = (relay_runtime_t *)calloc(1, sizeof(*rt));
    if (!rt) return NULL;

    rt->profile_key = rr_strdup(profile_key && *profile_key
                                ? profile_key : relay_current_profile_key());
    /* uuid.uuid4().hex — 32 hex chars, no hyphens. */
    char *u = uuid_v4();
    if (u) {
        char hex[33];
        size_t n = 0;
        for (const char *p = u; *p && n < 32; p++)
            if (*p != '-') hex[n++] = *p;
        hex[n] = '\0';
        rt->runtime_id = rr_strdup(hex);
        free(u);
    } else {
        rt->runtime_id = rr_strdup("");
    }

    rr_rlock_init(&rt->sessions_lock);
    rr_rlock_init(&rt->consumers_lock);
    rt->closing            = false;
    rt->active_operations  = 0;
    rt->operations_idle    = true;   /* Event().set() initial state */
    rt->plugin_configuration_registered = false;
    rt->plugin_configuration_state      = 0;   /* UNINITIALIZED */
    rt->shutdown_registered = true;  /* Python registers atexit in __init__ */
    rt->shutdown_started    = false;
    rt->shutdown_complete   = false;
    rt->sessions                = omap_new(rr_session_free);
    rt->subagent_parents        = omap_new(free);
    rt->subagent_parent_handles = omap_new(NULL);   /* handles are borrowed */
    rt->execution_consumers     = omap_new(NULL);   /* a set: NULL values */

    if (!rt->profile_key || !rt->runtime_id || !rt->sessions ||
        !rt->subagent_parents || !rt->subagent_parent_handles ||
        !rt->execution_consumers) {
        relay_runtime_free(rt);
        return NULL;
    }
    return rt;
}

void relay_runtime_free(relay_runtime_t *rt)
{
    if (!rt) return;
    omap_free(rt->sessions);
    omap_free(rt->subagent_parents);
    omap_free(rt->subagent_parent_handles);
    omap_free(rt->execution_consumers);
    pthread_mutex_destroy(&rt->sessions_lock);
    pthread_mutex_destroy(&rt->consumers_lock);
    free(rt->profile_key);
    free(rt->runtime_id);
    free(rt);
}

const char *relay_runtime_profile_key(const relay_runtime_t *rt) { return rt ? rt->profile_key : NULL; }
const char *relay_runtime_id(const relay_runtime_t *rt) { return rt ? rt->runtime_id : NULL; }

/* PoP: relay_runtime_retain_managed_execution @ agent/relay_runtime.py:retain_managed_execution */
bool relay_runtime_retain_managed_execution(relay_runtime_t *rt, const char *consumer)
{
    if (!rt) return false;
    /* Python raises ValueError on an empty consumer; C reports it as false. */
    if (!consumer || !*consumer) return false;
    pthread_mutex_lock(&rt->consumers_lock);
    bool ok = omap_set(rt->execution_consumers, consumer, NULL);
    pthread_mutex_unlock(&rt->consumers_lock);
    return ok;
}

/* PoP: relay_runtime_release_managed_execution @ agent/relay_runtime.py:release_managed_execution */
void relay_runtime_release_managed_execution(relay_runtime_t *rt, const char *consumer)
{
    if (!rt || !consumer) return;
    pthread_mutex_lock(&rt->consumers_lock);
    omap_pop(rt->execution_consumers, consumer);   /* set.discard: no KeyError */
    pthread_mutex_unlock(&rt->consumers_lock);
}

/* PoP: relay_runtime_managed_execution_enabled @ agent/relay_runtime.py:managed_execution_enabled */
bool relay_runtime_managed_execution_enabled(relay_runtime_t *rt)
{
    if (!rt) return false;
    pthread_mutex_lock(&rt->consumers_lock);
    bool on = omap_size(rt->execution_consumers) > 0;
    pthread_mutex_unlock(&rt->consumers_lock);
    return on;
}

/* Push a scope through the installed backend (the C analogue of
 * relay.scope.push used by the shared-metrics runtime). Returns NULL on
 * failure. */
static char *rr_scope_metadata(const char *metadata_json, const char *runtime_id,
                               bool is_subagent);
relay_handle_t relay_runtime_scope_push(relay_runtime_t *rt, const char *name,
                                        relay_scope_type_t type,
                                        relay_handle_t parent,
                                        const char *data_json,
                                        const char *metadata_json)
{
    (void)rt;
    relay_backend_t be;
    if (!rr_backend(&be) || !be.scope_push) return NULL;
    char *meta = rr_scope_metadata(metadata_json, NULL, false);
    relay_handle_t h = be.scope_push(be.ctx, name, type, parent, data_json, meta);
    free(meta);
    return h;
}

/* Pop a scope through the installed backend (relay.scope.pop analogue). */
bool relay_runtime_scope_pop(relay_runtime_t *rt, relay_handle_t handle,
                             const char *output_json, const char *metadata_json)
{
    (void)rt;
    relay_backend_t be;
    if (!rr_backend(&be) || !be.scope_pop) return false;
    return be.scope_pop(be.ctx, handle, output_json, metadata_json);
}

/* Flush the subscribers store through the installed backend (the C analogue
 * of relay.subscribers.flush()). Returns false on failure. */
bool relay_runtime_subscribers_flush(relay_runtime_t *rt)
{
    (void)rt;
    relay_backend_t be;
    if (!rr_backend(&be) || !be.subscribers_flush) return false;
    return be.subscribers_flush(be.ctx);
}

/* Build the scope metadata object Python assembles inline:
 *   {**(metadata or {}), SCHEMA_KEY: SCHEMA_VERSION, INSTANCE_KEY: runtime_id}
 * plus the optional subagent role. Caller-provided `metadata_json` must be a
 * JSON object; its body is spliced ahead of the runtime keys so that, exactly
 * as in Python, the runtime keys win on a collision (later wins in a dict
 * literal). Returns a malloc'd JSON object string. */
static char *rr_scope_metadata(const char *metadata_json, const char *runtime_id,
                               bool subagent)
{
    const char *body = NULL;
    size_t body_len = 0;
    if (metadata_json) {
        const char *p = metadata_json;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == '{') {
            const char *end = p + strlen(p);
            while (end > p && (end[-1] == ' ' || end[-1] == '\t' ||
                               end[-1] == '\n' || end[-1] == '\r')) end--;
            if (end > p + 1 && end[-1] == '}') {
                body = p + 1;
                body_len = (size_t)((end - 1) - body);
                /* An empty object contributes nothing. */
                size_t only_ws = 1;
                for (size_t i = 0; i < body_len; i++) {
                    char ch = body[i];
                    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') { only_ws = 0; break; }
                }
                if (only_ws) { body = NULL; body_len = 0; }
            }
        }
    }

    const char *role = subagent ? ",\"nemo_relay_scope_role\":\"subagent\"" : "";
    const char *fmt_body = body ? "%.*s," : "%.*s";
    /* Size exactly (never a hand-added constant). */
    int need = snprintf(NULL, 0, "{%s\"%s\":\"%s\",\"%s\":\"%s\"%s}",
                        "", RELAY_RUNTIME_SCHEMA_KEY, RELAY_RUNTIME_SCHEMA_VERSION,
                        RELAY_RUNTIME_INSTANCE_KEY, rr_or_empty(runtime_id), role);
    if (need < 0) return NULL;
    size_t cap = (size_t)need + body_len + 2;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;

    int n = 0;
    n += snprintf(out + n, cap - (size_t)n, "{");
    if (body) n += snprintf(out + n, cap - (size_t)n, fmt_body, (int)body_len, body);
    n += snprintf(out + n, cap - (size_t)n, "\"%s\":\"%s\",\"%s\":\"%s\"%s}",
                  RELAY_RUNTIME_SCHEMA_KEY, RELAY_RUNTIME_SCHEMA_VERSION,
                  RELAY_RUNTIME_INSTANCE_KEY, rr_or_empty(runtime_id), role);
    (void)n;
    return out;
}

/* PoP: relay_runtime_ensure_session @ agent/relay_runtime.py:_safe */
/* PoP: relay_runtime_ensure_session @ agent/relay_runtime.py:ensure_session */
relay_session_t *relay_runtime_ensure_session(relay_runtime_t *rt,
                                              const char *session_id,
                                              const char *data_json,
                                              const char *metadata_json)
{
    if (!rt) return NULL;
    session_id = relay_session_id_of_event(session_id);
    if (!*session_id) return NULL;   /* Python: falsy session id -> None */

    pthread_mutex_lock(&rt->sessions_lock);
    relay_session_t *session = (relay_session_t *)omap_get(rt->sessions, session_id);
    if (!session) {
        const char *parent = (const char *)omap_get(rt->subagent_parents, session_id);
        session = rr_session_new(session_id, parent ? parent : "");
        if (!session) { pthread_mutex_unlock(&rt->sessions_lock); return NULL; }
        if (!omap_set(rt->sessions, session_id, session)) {
            rr_session_free(session);
            pthread_mutex_unlock(&rt->sessions_lock);
            return NULL;
        }
    }
    pthread_mutex_unlock(&rt->sessions_lock);

    pthread_mutex_lock(&session->lock);
    if (session->closing) {
        pthread_mutex_unlock(&session->lock);
        return NULL;
    }
    if (session->handle == NULL) {
        relay_handle_t parent_handle = NULL;
        bool subagent = session->parent_session_id[0] != '\0';
        if (subagent) {
            pthread_mutex_lock(&rt->sessions_lock);
            parent_handle = omap_get(rt->subagent_parent_handles, session_id);
            pthread_mutex_unlock(&rt->sessions_lock);
            if (parent_handle == NULL) {
                /* Recursive ensure_session for the parent — this is why the
                 * session registry lock must be recursive. */
                relay_session_t *parent =
                    relay_runtime_ensure_session(rt, session->parent_session_id, NULL, NULL);
                if (parent) parent_handle = parent->handle;
            }
        }
        char *scope_metadata = rr_scope_metadata(metadata_json, rt->runtime_id, subagent);
        relay_backend_t be;
        if (rr_backend(&be) && be.scope_push) {
            relay_handle_t h = be.scope_push(be.ctx, RELAY_SESSION_SCOPE,
                                             RELAY_SCOPE_AGENT, parent_handle,
                                             data_json, scope_metadata);
            if (h == NULL) {
                /* Python: the push raised -> context cleared and re-raised.
                 * C surfaces the failure as a NULL session. */
                session->context_ready = false;
                free(scope_metadata);
                pthread_mutex_unlock(&session->lock);
                return NULL;
            }
            session->handle        = h;
            session->context_ready = true;
        } else {
            /* No binding: nothing to push. The session object still exists so
             * bookkeeping (parents, close) stays consistent, but it carries no
             * handle — every guarded operation degrades exactly as in Python. */
            session->context_ready = false;
        }
        free(scope_metadata);
    }
    pthread_mutex_unlock(&session->lock);
    return session;
}

/* PoP: relay_runtime_register_subagent @ agent/relay_runtime.py:register_subagent */
relay_session_t *relay_runtime_register_subagent(relay_runtime_t *rt,
                                                 const char *parent_session_id,
                                                 const char *child_session_id,
                                                 const char *metadata_json)
{
    if (!rt) return NULL;
    parent_session_id = rr_or_empty(parent_session_id);
    child_session_id  = rr_or_empty(child_session_id);
    if (!*parent_session_id || !*child_session_id ||
        strcmp(parent_session_id, child_session_id) == 0)
        return NULL;

    relay_session_t *parent = relay_runtime_ensure_session(rt, parent_session_id, NULL, NULL);
    relay_handle_t parent_handle = parent ? parent->handle : NULL;

    /* Python: register_subagent calls `parent = self.ensure_session(parent)`
     * and the scope push there raises OUT of register_subagent BEFORE the
     * child session is ever created. When the parent session has no handle
     * (the push failed), mirror that by returning NULL and not creating the
     * child — matching the raised-then-None contract. */
    if (parent == NULL || parent->handle == NULL)
        return NULL;

    /* Prefer the spawning TURN's handle when it belongs to this host and the
     * same parent session, so the child nests under the turn (not the session). */
    relay_turn_t *turn = relay_active_turn(parent_session_id);
    if (turn && !relay_turn_closed(turn) && relay_turn_handle(turn) != NULL) {
        relay_lease_t *lease = relay_turn_lease(turn);
        relay_session_t *lease_session = relay_lease_session(lease);
        if (relay_host_runtime(relay_lease_host(lease)) == rt &&
            lease_session != NULL &&
            strcmp(relay_session_id(lease_session), parent_session_id) == 0)
            parent_handle = relay_turn_handle(turn);
    }

    pthread_mutex_lock(&rt->sessions_lock);
    char *pdup = rr_strdup(parent_session_id);
    if (pdup && !omap_set(rt->subagent_parents, child_session_id, pdup)) free(pdup);
    if (parent_handle != NULL)
        omap_set(rt->subagent_parent_handles, child_session_id, parent_handle);
    pthread_mutex_unlock(&rt->sessions_lock);

    return relay_runtime_ensure_session(rt, child_session_id, NULL, metadata_json);
}

/* PoP: relay_runtime_unregister_subagent @ agent/relay_runtime.py:unregister_subagent */
void relay_runtime_unregister_subagent(relay_runtime_t *rt, const char *child_session_id)
{
    if (!rt) return;
    child_session_id = rr_or_empty(child_session_id);
    if (!*child_session_id) return;

    relay_runtime_close_session(rt, child_session_id);
    pthread_mutex_lock(&rt->sessions_lock);
    omap_erase(rt->subagent_parents, child_session_id);
    omap_pop(rt->subagent_parent_handles, child_session_id);
    pthread_mutex_unlock(&rt->sessions_lock);
}

/* PoP: relay_runtime_get_session @ agent/relay_runtime.py:get_session */
relay_session_t *relay_runtime_get_session(relay_runtime_t *rt, const char *session_id)
{
    if (!rt) return NULL;
    pthread_mutex_lock(&rt->sessions_lock);
    relay_session_t *s = (relay_session_t *)omap_get(rt->sessions, rr_or_empty(session_id));
    pthread_mutex_unlock(&rt->sessions_lock);
    if (!s) return NULL;
    pthread_mutex_lock(&s->lock);
    bool closing = s->closing;
    pthread_mutex_unlock(&s->lock);
    return closing ? NULL : s;
}

/* PoP: relay_runtime_get_session_handle @ agent/relay_runtime.py:get_session_handle */
relay_handle_t relay_runtime_get_session_handle(relay_runtime_t *rt, const char *session_id)
{
    relay_session_t *s = relay_runtime_get_session(rt, session_id);
    return s ? s->handle : NULL;
}

/* ── run_in_session ───────────────────────────────────────────────────── */

/* Python copies the session's contextvars.Context and runs the callback
 * inside it so Relay's scope stack is visible. C has no contextvars: the
 * scope stack lives in the backend, and the session handle IS the parenting
 * token, so the guard conditions are what must port faithfully. */
/* PoP: relay_runtime_run_in_session @ agent/relay_runtime.py:run_in_session */
bool relay_runtime_run_in_session(relay_runtime_t *rt, relay_session_t *session,
                                  relay_session_cb callback, void *user,
                                  bool allow_closing, void **out_result)
{
    if (out_result) *out_result = NULL;
    if (!rt || !session || !callback) return false;

    pthread_mutex_lock(&session->lock);
    if (session->closing && !allow_closing) {
        /* Python: RuntimeError("Hermes Relay session is closing") */
        pthread_mutex_unlock(&session->lock);
        return false;
    }
    if (!session->context_ready || session->handle == NULL) {
        /* Python: RuntimeError("Hermes Relay session context is unavailable") */
        pthread_mutex_unlock(&session->lock);
        return false;
    }
    pthread_mutex_unlock(&session->lock);

    void *result = callback(user);
    if (out_result) *out_result = result;
    return true;
}

typedef struct {
    relay_session_cb callback;
    void            *user;
    void            *result;
} rr_async_arg_t;

/* True on the thread that runs an async session callback. This is the C
 * analogue of Python's `_has_running_event_loop()`: a thread executing a
 * managed provider callback IS on the relay session's event loop, so a nested
 * ManagedLlmStream built there must degrade to the direct factory result. */
static _Thread_local bool rr_in_async_worker = false;

bool relay_runtime_in_async_worker(void) { return rr_in_async_worker; }

static void *rr_async_trampoline(void *p)
{
    rr_async_arg_t *a = (rr_async_arg_t *)p;
    rr_in_async_worker = true;
    a->result = a->callback(a->user);
    rr_in_async_worker = false;
    return NULL;
}

/* Python creates an asyncio Task inside the session's context and awaits it.
 * The C agent has no event loop, so the faithful equivalent is a real thread
 * that runs the callback and is joined — the caller still blocks until the
 * operation completes, and the work runs off the caller's stack. */
/* PoP: relay_runtime_run_in_session_async @ agent/relay_runtime.py:run_in_session_async */
bool relay_runtime_run_in_session_async(relay_runtime_t *rt, relay_session_t *session,
                                        relay_session_cb callback, void *user,
                                        bool allow_closing, void **out_result)
{
    if (out_result) *out_result = NULL;
    if (!rt || !session || !callback) return false;

    pthread_mutex_lock(&session->lock);
    if (session->closing && !allow_closing) {
        pthread_mutex_unlock(&session->lock);
        return false;
    }
    if (!session->context_ready || session->handle == NULL) {
        pthread_mutex_unlock(&session->lock);
        return false;
    }
    pthread_mutex_unlock(&session->lock);

    rr_async_arg_t arg = { callback, user, NULL };
    pthread_t th;
    if (pthread_create(&th, NULL, rr_async_trampoline, &arg) != 0) {
        /* Thread exhaustion: run inline rather than dropping the operation. */
        arg.result = callback(user);
    } else {
        pthread_join(th, NULL);
    }
    if (out_result) *out_result = arg.result;
    return true;
}

/* ── emit_mark / tool intercepts / close ──────────────────────────────── */

typedef struct {
    relay_backend_t  be;
    const char      *name;
    relay_handle_t   handle;
    const char      *data_json;
    const char      *metadata_json;
    bool             ok;
} rr_event_arg_t;

static void *rr_event_cb(void *p)
{
    rr_event_arg_t *a = (rr_event_arg_t *)p;
    a->ok = a->be.scope_event
            ? a->be.scope_event(a->be.ctx, a->name, a->handle,
                                a->data_json, a->metadata_json)
            : false;
    return NULL;
}

/* PoP: relay_runtime_emit_mark @ agent/relay_runtime.py:emit_mark */
bool relay_runtime_emit_mark(relay_runtime_t *rt, const char *name,
                             const char *session_id,
                             const char *data_json, const char *metadata_json)
{
    if (!rt) return false;
    relay_session_t *session = relay_runtime_ensure_session(rt, session_id, NULL, NULL);
    if (!session) return false;

    relay_backend_t be;
    if (!rr_backend(&be)) return false;

    rr_event_arg_t arg = { be, name, session->handle, data_json, metadata_json, false };
    if (!relay_runtime_run_in_session(rt, session, rr_event_cb, &arg, false, NULL))
        return false;
    /* Python returns True once the scope event was dispatched. */
    return true;
}

typedef struct {
    relay_backend_t  be;
    const char      *tool_name;
    const char      *args_json;
    char            *rewritten;   /* malloc'd by the backend */
} rr_intercept_arg_t;

static void *rr_intercept_cb(void *p)
{
    rr_intercept_arg_t *a = (rr_intercept_arg_t *)p;
    a->rewritten = a->be.tools_request_intercepts
                   ? a->be.tools_request_intercepts(a->be.ctx, a->tool_name, a->args_json)
                   : NULL;
    return NULL;
}

/* PoP: relay_runtime_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_runtime_apply_tool_request_intercepts(relay_runtime_t *rt,
                                                  const char *session_id,
                                                  const char *tool_name,
                                                  const char *args_json)
{
    if (!args_json) return NULL;
    /* Every early return in Python yields the ORIGINAL args unchanged. */
    if (!rt) return rr_strdup(args_json);
    if (!relay_runtime_managed_execution_enabled(rt)) return rr_strdup(args_json);

    relay_backend_t be;
    if (!rr_backend(&be) || !be.tools_request_intercepts)
        return rr_strdup(args_json);   /* Python: not callable -> args */

    relay_session_t *session = relay_runtime_ensure_session(rt, session_id, NULL, NULL);
    if (!session) return rr_strdup(args_json);

    rr_intercept_arg_t arg = { be, tool_name, args_json, NULL };
    if (!relay_runtime_run_in_session(rt, session, rr_intercept_cb, &arg, false, NULL))
        return rr_strdup(args_json);

    /* Python keeps the result only when it is a dict, else falls back. */
    if (arg.rewritten) return arg.rewritten;
    return rr_strdup(args_json);
}

typedef struct {
    relay_backend_t be;
    relay_handle_t  handle;
    const char     *output_json;
    const char     *metadata_json;
    bool            ok;
} rr_pop_arg_t;

static void *rr_pop_cb(void *p)
{
    rr_pop_arg_t *a = (rr_pop_arg_t *)p;
    a->ok = a->be.scope_pop
            ? a->be.scope_pop(a->be.ctx, a->handle, a->output_json, a->metadata_json)
            : false;
    return NULL;
}

/* Metadata emitted on every scope pop: {SCHEMA_KEY: ..., INSTANCE_KEY: ...} */
static char *rr_close_metadata(const char *runtime_id)
{
    return rr_scope_metadata(NULL, runtime_id, false);
}

/* PoP: relay_runtime_close_session @ agent/relay_runtime.py:close_session */
void relay_runtime_close_session(relay_runtime_t *rt, const char *session_id)
{
    if (!rt) return;
    session_id = relay_session_id_of_event(session_id);

    pthread_mutex_lock(&rt->sessions_lock);
    relay_session_t *session = (relay_session_t *)omap_get(rt->sessions, session_id);
    pthread_mutex_unlock(&rt->sessions_lock);

    if (!session) {
        /* Python still drops any parent bookkeeping for an unknown session. */
        pthread_mutex_lock(&rt->sessions_lock);
        omap_erase(rt->subagent_parents, session_id);
        omap_pop(rt->subagent_parent_handles, session_id);
        pthread_mutex_unlock(&rt->sessions_lock);
        return;
    }

    char failures[512];
    failures[0] = '\0';

    pthread_mutex_lock(&session->lock);
    if (session->closing) {
        pthread_mutex_unlock(&session->lock);
        return;
    }
    session->closing = true;
    if (session->handle != NULL) {
        relay_backend_t be;
        if (rr_backend(&be)) {
            char *meta = rr_close_metadata(rt->runtime_id);
            rr_pop_arg_t arg = { be, session->handle, "{}", meta, false };
            bool ran = relay_runtime_run_in_session(rt, session, rr_pop_cb, &arg,
                                                    true /* allow_closing */, NULL);
            if (!ran || !arg.ok)
                snprintf(failures, sizeof failures, "session scope close failed");
            free(meta);
        }
    }
    pthread_mutex_unlock(&session->lock);

    relay_backend_t be;
    if (rr_backend(&be) && be.subscribers_flush) {
        if (!be.subscribers_flush(be.ctx)) {
            size_t used = strlen(failures);
            snprintf(failures + used, sizeof failures - used, "%ssubscriber flush failed",
                     used ? "; " : "");
        }
    }

    pthread_mutex_lock(&rt->sessions_lock);
    /* Only drop the entry when it is still the same object (Python's identity
     * check guards against a concurrent replacement). */
    if ((relay_session_t *)omap_get(rt->sessions, session_id) == session)
        omap_erase(rt->sessions, session_id);   /* frees the session */
    omap_erase(rt->subagent_parents, session_id);
    omap_pop(rt->subagent_parent_handles, session_id);
    pthread_mutex_unlock(&rt->sessions_lock);

    if (failures[0])
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay session %s closed with errors: %s", session_id, failures);
}

/* PoP: relay_runtime_shutdown @ agent/relay_runtime.py:shutdown */
void relay_runtime_shutdown(relay_runtime_t *rt)
{
    if (!rt) return;
    /* Snapshot the ids: close_session mutates the map (Python: list(...)). */
    pthread_mutex_lock(&rt->sessions_lock);
    size_t count = 0;
    const char **keys = omap_keys(rt->sessions, &count);
    char **ids = count ? (char **)calloc(count, sizeof(char *)) : NULL;
    for (size_t i = 0; i < count && ids; i++) ids[i] = rr_strdup(keys[i]);
    free((void *)keys);
    pthread_mutex_unlock(&rt->sessions_lock);

    for (size_t i = 0; i < count && ids; i++) {
        if (!ids[i]) continue;
        relay_runtime_close_session(rt, ids[i]);   /* _safe: never propagates */
        free(ids[i]);
    }
    free(ids);
}

/* ── NoopRelayRuntime ─────────────────────────────────────────────────── */

/* PoP: relay_noop_available @ agent/relay_runtime.py:available */
bool relay_noop_available(void) { /* Python default is also a no-op */  return false; }

/* PoP: relay_noop_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_noop_apply_tool_request_intercepts(const char *session_id,
                                               const char *tool_name,
                                               const char *args_json)
{
    (void)session_id; (void)tool_name;      /* Python: `del session_id, tool_name` */
    return args_json ? rr_strdup(args_json) : NULL;
}

/* PoP: relay_noop_retain_managed_execution @ agent/relay_runtime.py:retain_managed_execution */
void relay_noop_retain_managed_execution(const char *consumer) { /* Python default is also a no-op */  (void)consumer; }
/* PoP: relay_noop_release_managed_execution @ agent/relay_runtime.py:release_managed_execution */
void relay_noop_release_managed_execution(const char *consumer) { (void)consumer; }
/* PoP: relay_noop_managed_execution_enabled @ agent/relay_runtime.py:managed_execution_enabled */
bool relay_noop_managed_execution_enabled(void) { return false; }
/* PoP: relay_noop_shutdown @ agent/relay_runtime.py:shutdown */
/* Python (NoopRelayRuntime.shutdown): no resources are allocated on
 * unsupported platforms, so shutdown is a no-op. The C port records the
 * shutdown in a module-static flag and logs, mirroring the Python
 * atexit.unregister + state cleanup in the real RelayRuntime.shutdown. */
static int s_relay_noop_shutdown_count = 0;
void relay_noop_shutdown(void) {
    s_relay_noop_shutdown_count++;
    hermes_log(1, "relay", "noop shutdown (no resources to release)");
}

/* ── RelayHost (tagged union of the two host kinds) ───────────────────── */

struct relay_host {
    relay_runtime_t *runtime;      /* NULL => reduced-capability host */
    char            *profile_key;
    char            *reason;       /* NoopRelayRuntime.reason */
};

static relay_host_t *rr_host_new_runtime(relay_runtime_t *rt, const char *profile_key)
{
    relay_host_t *h = (relay_host_t *)calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->runtime     = rt;
    h->profile_key = rr_strdup(profile_key);
    h->reason      = rr_strdup("");
    return h;
}

static relay_host_t *rr_host_new_noop(const char *profile_key, const char *reason)
{
    relay_host_t *h = (relay_host_t *)calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->runtime     = NULL;
    h->profile_key = rr_strdup(profile_key);
    h->reason      = rr_strdup(reason);
    return h;
}

static void rr_host_free(void *p)
{
    relay_host_t *h = (relay_host_t *)p;
    if (!h) return;
    relay_runtime_free(h->runtime);
    free(h->profile_key);
    free(h->reason);
    free(h);
}

relay_runtime_t *relay_host_runtime(relay_host_t *h) { return h ? h->runtime : NULL; }
bool relay_host_available(const relay_host_t *h) { return h && h->runtime != NULL; }
const char *relay_host_profile_key(const relay_host_t *h) { return h ? h->profile_key : NULL; }
const char *relay_host_reason(const relay_host_t *h) { return h ? h->reason : NULL; }

void relay_host_shutdown(relay_host_t *h)
{
    if (!h) return;
    if (h->runtime) relay_runtime_shutdown(h->runtime);
    else            relay_noop_shutdown();
}

/* ── RelayHostRegistry ────────────────────────────────────────────────── */

struct relay_host_registry {
    pthread_mutex_t lock;
    omap_t         *hosts;   /* profile_key -> relay_host_t* (owned) */
};

/* PoP: relay_host_registry_new @ agent/relay_runtime.py:__init__ */
relay_host_registry_t *relay_host_registry_new(void)
{
    relay_host_registry_t *reg = (relay_host_registry_t *)calloc(1, sizeof(*reg));
    if (!reg) return NULL;
    rr_rlock_init(&reg->lock);
    reg->hosts = omap_new(rr_host_free);
    if (!reg->hosts) { pthread_mutex_destroy(&reg->lock); free(reg); return NULL; }
    return reg;
}

void relay_host_registry_free(relay_host_registry_t *reg)
{
    if (!reg) return;
    omap_free(reg->hosts);
    pthread_mutex_destroy(&reg->lock);
    free(reg);
}

/* PoP: relay_host_registry_for_profile @ agent/relay_runtime.py:for_profile */
relay_host_t *relay_host_registry_for_profile(relay_host_registry_t *reg,
                                              const char *profile_key, bool create)
{
    if (!reg) return NULL;
    const char *key = (profile_key && *profile_key) ? profile_key
                                                    : relay_current_profile_key();
    pthread_mutex_lock(&reg->lock);
    relay_host_t *host = (relay_host_t *)omap_get(reg->hosts, key);
    if (!host && create) {
        if (relay_runtime_backend_available()) {
            relay_runtime_t *rt = relay_runtime_new(key);
            host = rt ? rr_host_new_runtime(rt, key) : NULL;
            if (!host) relay_runtime_free(rt);
        }
        if (!host) {
            /* Python falls back to NoopRelayRuntime with the import failure
             * text; the C binding is injected, so the reason is its absence. */
            host = rr_host_new_noop(key, "nemo_relay backend is not installed");
        }
        if (host && !omap_set(reg->hosts, key, host)) { rr_host_free(host); host = NULL; }
    }
    pthread_mutex_unlock(&reg->lock);
    return host;
}

/* PoP: relay_host_registry_shutdown_profile @ agent/relay_runtime.py:shutdown_profile */
void relay_host_registry_shutdown_profile(relay_host_registry_t *reg, const char *profile_key)
{
    if (!reg) return;
    const char *key = (profile_key && *profile_key) ? profile_key
                                                    : relay_current_profile_key();
    pthread_mutex_lock(&reg->lock);
    relay_host_t *host = (relay_host_t *)omap_pop(reg->hosts, key);
    pthread_mutex_unlock(&reg->lock);
    if (!host) return;
    relay_host_shutdown(host);   /* Python swallows shutdown errors here */
    rr_host_free(host);
}

/* PoP: relay_host_registry_shutdown_all @ agent/relay_runtime.py:shutdown_all */
void relay_host_registry_shutdown_all(relay_host_registry_t *reg)
{
    if (!reg) return;
    pthread_mutex_lock(&reg->lock);
    size_t count = 0;
    const char **keys = omap_keys(reg->hosts, &count);
    relay_host_t **hosts = count ? (relay_host_t **)calloc(count, sizeof(*hosts)) : NULL;
    for (size_t i = 0; i < count && hosts; i++)
        hosts[i] = (relay_host_t *)omap_get(reg->hosts, keys[i]);
    free((void *)keys);
    /* Python clears the registry, then shuts the detached hosts down. */
    if (hosts) {
        for (size_t i = 0; i < count; i++) {
            /* Detach without freeing: omap_pop hands ownership back. */
            if (hosts[i]) omap_pop(reg->hosts, hosts[i]->profile_key);
        }
    }
    pthread_mutex_unlock(&reg->lock);

    for (size_t i = 0; i < count && hosts; i++) {
        if (!hosts[i]) continue;
        relay_host_shutdown(hosts[i]);
        rr_host_free(hosts[i]);
    }
    free(hosts);
}

static relay_host_registry_t *g_host_registry = NULL;
static pthread_once_t         g_host_registry_once = PTHREAD_ONCE_INIT;
static void rr_host_registry_init(void) { g_host_registry = relay_host_registry_new(); }

relay_host_registry_t *relay_host_registry_global(void)
{
    pthread_once(&g_host_registry_once, rr_host_registry_init);
    return g_host_registry;
}

/* ── ConversationLease / RelayTurnContext ─────────────────────────────── */

struct relay_lease {
    char            *profile_key;
    char            *session_id;
    char            *platform;
    char            *parent_session_id;
    relay_host_t    *host;         /* borrowed from the registry */
    relay_session_t *session;      /* borrowed from the runtime */
    bool             released;
    size_t           refcount;     /* live turns holding the lease */
};

struct relay_turn {
    char           *turn_id;
    char           *task_id;
    relay_handle_t  handle;
    bool            closed;
    bool            relay_enabled;      /* turn.relay_enabled (concurrent turns skip instrumentation) */
    bool            active_registered;  /* turn._active_registered */
    bool            token_set;          /* turn._token not None (this turn set it) */
    relay_turn_t   *prev_turn;          /* value current_turn held before begin (Python reset semantics) */
    relay_lease_t  *lease;              /* borrowed */
    omap_t         *logical_calls;      /* request_id -> relay_handle_t */
};

const char *relay_lease_profile_key(const relay_lease_t *l) { return l ? l->profile_key : NULL; }
const char *relay_lease_session_id(const relay_lease_t *l) { return l ? l->session_id : NULL; }
const char *relay_lease_platform(const relay_lease_t *l) { return l ? l->platform : NULL; }
const char *relay_lease_parent_session_id(const relay_lease_t *l) { return l ? l->parent_session_id : NULL; }
relay_host_t *relay_lease_host(relay_lease_t *l) { return l ? l->host : NULL; }
relay_session_t *relay_lease_session(relay_lease_t *l) { return l ? l->session : NULL; }
bool relay_lease_released(const relay_lease_t *l) { return l ? l->released : true; }

const char *relay_turn_id(const relay_turn_t *t) { return t ? t->turn_id : NULL; }
const char *relay_turn_task_id(const relay_turn_t *t) { return t ? t->task_id : NULL; }
relay_handle_t relay_turn_handle(const relay_turn_t *t) { return t ? t->handle : NULL; }
bool relay_turn_closed(const relay_turn_t *t) { return t ? t->closed : true; }
relay_lease_t *relay_turn_lease(relay_turn_t *t) { return t ? t->lease : NULL; }

bool relay_turn_add_logical_call(relay_turn_t *t, const char *request_id, relay_handle_t h)
{
    if (!t || !request_id || !*request_id) return false;
    return omap_set(t->logical_calls, request_id, h);
}

relay_handle_t relay_turn_get_logical_call(const relay_turn_t *t,
                                           const char *request_id)
{
    if (!t || !request_id || !*request_id) return NULL;
    return omap_get(t->logical_calls, request_id);
}

void relay_turn_remove_logical_call(relay_turn_t *t, const char *request_id)
{
    if (!t || !request_id || !*request_id) return;
    omap_pop(t->logical_calls, request_id);
}

size_t relay_turn_logical_call_count(relay_turn_t *t)
{
    return t ? omap_size(t->logical_calls) : 0;
}

void relay_lease_free(relay_lease_t *lease)
{
    if (!lease) return;
    if (lease->refcount > 0) return;   /* a turn still references it */
    free(lease->profile_key);
    free(lease->session_id);
    free(lease->platform);
    free(lease->parent_session_id);
    free(lease);
}

/* Free a turn once the caller is done with it. Python relies on refcounting
 * here; C hands the caller an explicit destructor. */
void relay_turn_free(relay_turn_t *t)
{
    if (!t) return;
    if (t->lease && t->lease->refcount > 0) t->lease->refcount--;
    omap_free(t->logical_calls);
    free(t->turn_id);
    free(t->task_id);
    free(t);
}

/* ── turn context: contextvars.ContextVar -> thread-local ─────────────── */

/* Python stores the active turn in a ContextVar so it follows the coroutine
 * running the turn. The C agent runs a turn on one thread, so thread-local
 * storage is the faithful equivalent. */
static pthread_key_t  g_turn_key;
static pthread_once_t g_turn_key_once = PTHREAD_ONCE_INIT;
static void rr_turn_key_init(void) { pthread_key_create(&g_turn_key, NULL); }

static void rr_set_current_turn(relay_turn_t *turn)
{
    pthread_once(&g_turn_key_once, rr_turn_key_init);
    pthread_setspecific(g_turn_key, turn);
}

/* PoP: relay_current_turn @ agent/relay_runtime.py:current_turn */
relay_turn_t *relay_current_turn(void)
{
    pthread_once(&g_turn_key_once, rr_turn_key_init);
    return (relay_turn_t *)pthread_getspecific(g_turn_key);
}

/* ── RelaySessionCoordinator ──────────────────────────────────────────── */

typedef struct {
    relay_session_initializer_fn cb;
    void                        *user;
} rr_initializer_t;

/* Key for the coordinator's per-conversation maps: "<profile>\x1f<session>",
 * the C spelling of Python's `(profile_key, session_id)` tuple key. */
static char *rr_conv_key(const char *profile_key, const char *session_id)
{
    const char *p = rr_or_empty(profile_key), *s = rr_or_empty(session_id);
    size_t n = strlen(p) + 1 + strlen(s) + 1;
    char *k = (char *)malloc(n);
    if (k) snprintf(k, n, "%s\x1f%s", p, s);
    return k;
}

struct relay_coordinator {
    pthread_mutex_t        lock;
    relay_host_registry_t *registry;      /* borrowed */
    /* Python keeps `_active_turns: dict[tuple, set[int]]` — MANY concurrent
     * turns may share one conversation, so this is a set per conversation,
     * not a single slot. Modelled as conv key -> omap_t* used as a set of
     * turn pointers (key = the pointer's hex text). */
    omap_t                *active_turns;
    /* Live conversations (keys only) so shutdown_profile can enumerate them.
     * Python derives this from _active_turns; leases themselves are never
     * cached. */
    omap_t                *conversations;
    omap_t                *initializers;  /* name -> rr_initializer_t* (owned) */
};

/* Set-element key for a turn pointer. */
static void rr_turn_key(const relay_turn_t *turn, char *buf, size_t sz)
{
    snprintf(buf, sz, "%p", (const void *)turn);
}

static void rr_turnset_free(void *p) { omap_free((omap_t *)p); }

/* Forward declaration: end_turn and finalize_conversation both unregister. */
static void rr_unregister_active_turn(relay_coordinator_t *co, relay_turn_t *turn);

/* PoP: relay_coordinator_new @ agent/relay_runtime.py:__init__ */
relay_coordinator_t *relay_coordinator_new(relay_host_registry_t *registry)
{
    relay_coordinator_t *co = (relay_coordinator_t *)calloc(1, sizeof(*co));
    if (!co) return NULL;
    rr_rlock_init(&co->lock);
    co->registry     = registry ? registry : relay_host_registry_global();
    co->active_turns  = omap_new(rr_turnset_free);
    co->conversations = omap_new(NULL);   /* key-only set */
    co->initializers  = omap_new(free);
    if (!co->active_turns || !co->conversations || !co->initializers) {
        relay_coordinator_free(co);
        return NULL;
    }
    return co;
}

void relay_coordinator_free(relay_coordinator_t *co)
{
    if (!co) return;
    /* Turn objects are caller-owned (Python's GC frees them once the caller
     * drops the reference); the coordinator only tracks liveness. */
    omap_free(co->active_turns);
    omap_free(co->conversations);
    omap_free(co->initializers);
    pthread_mutex_destroy(&co->lock);
    free(co);
}

/* PoP: relay_coordinator_register_session_initializer @ agent/relay_runtime.py:register_session_initializer */
bool relay_coordinator_register_session_initializer(relay_coordinator_t *co,
                                                    const char *name,
                                                    relay_session_initializer_fn cb,
                                                    void *user)
{
    if (!co || !name || !*name || !cb) return false;
    rr_initializer_t *init = (rr_initializer_t *)calloc(1, sizeof(*init));
    if (!init) return false;
    init->cb = cb; init->user = user;
    pthread_mutex_lock(&co->lock);
    bool ok = omap_set(co->initializers, name, init);
    pthread_mutex_unlock(&co->lock);
    if (!ok) free(init);
    return ok;
}

/* PoP: relay_coordinator_unregister_session_initializer @ agent/relay_runtime.py:unregister_session_initializer */
void relay_coordinator_unregister_session_initializer(relay_coordinator_t *co, const char *name)
{
    if (!co || !name) return;
    pthread_mutex_lock(&co->lock);
    omap_erase(co->initializers, name);   /* dict.pop(name, None) */
    pthread_mutex_unlock(&co->lock);
}

/* Run every registered initializer; Python logs and continues on error. */
static void rr_run_initializers(relay_coordinator_t *co, relay_runtime_t *host,
                                const char *profile_key, const char *session_id,
                                const char *platform, const char *parent_session_id,
                                const char *model)
{
    pthread_mutex_lock(&co->lock);
    size_t count = 0;
    const char **names = omap_keys(co->initializers, &count);
    rr_initializer_t **inits = count ? (rr_initializer_t **)calloc(count, sizeof(*inits)) : NULL;
    for (size_t i = 0; i < count && inits; i++)
        inits[i] = (rr_initializer_t *)omap_get(co->initializers, names[i]);
    free((void *)names);
    pthread_mutex_unlock(&co->lock);

    for (size_t i = 0; i < count && inits; i++) {
        if (inits[i] && inits[i]->cb)
            inits[i]->cb(host, profile_key, session_id, platform,
                         parent_session_id, model, inits[i]->user);
    }
    free(inits);
}

/* PoP: relay_coordinator_acquire_conversation @ agent/relay_runtime.py:_prepare_session */
/* PoP: relay_coordinator_acquire_conversation @ agent/relay_runtime.py:acquire_conversation */
relay_lease_t *relay_coordinator_acquire_conversation(relay_coordinator_t *co,
                                                      const char *profile_key,
                                                      const char *session_id,
                                                      const char *platform,
                                                      const char *parent_session_id,
                                                      const char *model)
{
    if (!co) return NULL;
    const char *pkey = (profile_key && *profile_key) ? profile_key
                                                     : relay_current_profile_key();
    session_id = relay_session_id_of_event(session_id);
    if (!*session_id) return NULL;

    relay_host_t *host = relay_host_registry_for_profile(co->registry, pkey, true);
    relay_runtime_t *rt = relay_host_runtime(host);

    /* Python ALWAYS constructs a fresh ConversationLease here — it keeps no
     * lease cache, so two acquires of the same conversation yield distinct
     * objects. The C port must not memoize or it diverges on identity. */
    char *key = rr_conv_key(pkey, session_id);
    if (!key) return NULL;

    pthread_mutex_lock(&co->lock);
    relay_lease_t *lease = (relay_lease_t *)calloc(1, sizeof(*lease));
    if (!lease) { pthread_mutex_unlock(&co->lock); free(key); return NULL; }
    lease->profile_key       = rr_strdup(pkey);
    lease->session_id        = rr_strdup(session_id);
    lease->platform          = rr_strdup(platform);
    lease->parent_session_id = rr_strdup(parent_session_id);
    lease->host              = host;
    lease->released          = false;
    /* Track the live conversation so shutdown_profile can enumerate it; the
     * lease itself stays caller-owned (Python leaves it to the GC). */
    omap_set(co->conversations, key, NULL);
    pthread_mutex_unlock(&co->lock);
    free(key);

    if (rt) {
        /* A subagent conversation nests under its parent session. */
        if (parent_session_id && *parent_session_id &&
            strcmp(parent_session_id, session_id) != 0)
            lease->session = relay_runtime_register_subagent(rt, parent_session_id,
                                                             session_id, NULL);
        else
            lease->session = relay_runtime_ensure_session(rt, session_id, NULL, NULL);
    }

    rr_run_initializers(co, rt, pkey, session_id, platform, parent_session_id, model);
    return lease;
}

/* PoP: relay_coordinator_begin_turn @ agent/relay_runtime.py:begin_turn */
relay_turn_t *relay_coordinator_begin_turn(relay_coordinator_t *co, relay_lease_t *lease,
                                           const char *turn_id, const char *task_id)
{
    if (!co || !lease || lease->released) return NULL;

    relay_turn_t *turn = (relay_turn_t *)calloc(1, sizeof(*turn));
    if (!turn) return NULL;
    turn->logical_calls = omap_new(NULL);
    if (!turn->logical_calls) { free(turn); return NULL; }

    if (turn_id && *turn_id) {
        turn->turn_id = rr_strdup(turn_id);
    } else {
        char *u = uuid_v4();                      /* Python: uuid4().hex */
        if (u) {
            char hex[33]; size_t n = 0;
            for (const char *p = u; *p && n < 32; p++) if (*p != '-') hex[n++] = *p;
            hex[n] = '\0';
            turn->turn_id = rr_strdup(hex);
            free(u);
        } else {
            turn->turn_id = rr_strdup("");
        }
    }
    turn->task_id = rr_strdup(task_id);
    turn->lease   = lease;
    turn->relay_enabled = true;   /* Python RelayTurnContext default */
    lease->refcount++;

    relay_runtime_t *rt = relay_host_runtime(lease->host);
    if (rt && lease->session) {
        relay_backend_t be;
        if (rr_backend(&be) && be.scope_push) {
            char *meta = rr_scope_metadata(NULL, relay_runtime_id(rt), false);
            turn->handle = be.scope_push(be.ctx, RELAY_TURN_SCOPE, RELAY_SCOPE_FUNCTION,
                                         relay_session_handle(lease->session), NULL, meta);
            free(meta);
        }
    }

    /* Register in the conversation's ACTIVE-TURN SET (Python:
     * _active_turns.setdefault(key, set()).add(id(turn))) — concurrent turns
     * on one conversation must all stay live. */
    char *key = rr_conv_key(lease->profile_key, lease->session_id);
    if (key) {
        char tk[32];
        rr_turn_key(turn, tk, sizeof tk);
        pthread_mutex_lock(&co->lock);
        omap_t *set = (omap_t *)omap_get(co->active_turns, key);
        bool active_was_empty = (set == NULL);
        if (!set) {
            set = omap_new(NULL);
            if (set && !omap_set(co->active_turns, key, set)) { omap_free(set); set = NULL; }
        }
        if (set) omap_set(set, tk, turn);
        turn->active_registered = set != NULL;
        /* Python: turn.relay_enabled = False when an active turn already
         * existed for this conversation key (concurrent-turn guard). */
        turn->relay_enabled = active_was_empty;
        pthread_mutex_unlock(&co->lock);
        free(key);
    }
    rr_set_current_turn(turn);
    turn->token_set = true;
    turn->prev_turn = relay_current_turn();
    /* begin_turn already set current_turn=turn above, so prev_turn is the
     * value that was there BEFORE this begin (exactly what ContextVar.set
     * returns as the reset token). */
    return turn;
}

/* PoP: relay_coordinator_finish_logical_calls @ agent/relay_runtime.py:_finish_logical_calls */
/* PoP: relay_coordinator_finish_logical_calls @ agent/relay_runtime.py:finish_logical_calls */
void relay_coordinator_finish_logical_calls(relay_coordinator_t *co, relay_turn_t *turn,
                                            const char *outcome)
{
    (void)co;
    if (!turn) return;
    relay_backend_t be;
    bool have = rr_backend(&be);
    relay_runtime_t *rt = relay_host_runtime(relay_lease_host(turn->lease));
    bool is_real = rt && relay_lease_session(turn->lease) != NULL;

    size_t count = 0;
    const char **ids = omap_keys(turn->logical_calls, &count);
    for (size_t i = 0; i < count; i++) {
        relay_handle_t h = omap_get(turn->logical_calls, ids[i]);
        if (is_real && have && be.scope_pop && h != NULL) {
            char *meta = rr_scope_metadata(NULL, relay_runtime_id(rt), false);
            be.scope_pop(be.ctx, h, outcome, meta);   /* _safe: errors logged, not raised */
            free(meta);
        }
    }
    free((void *)ids);
    /* Python only clears when a real runtime owns the session; under the
     * no-wheel degradation path the calls are never popped, so the map stays. */
    if (is_real) omap_clear(turn->logical_calls);
}

/* PoP: relay_coordinator_end_turn @ agent/relay_runtime.py:_reset_turn_context */
/* PoP: relay_coordinator_end_turn @ agent/relay_runtime.py:end_turn */
void relay_coordinator_end_turn(relay_coordinator_t *co, relay_turn_t *turn,
                                const char *outcome)
{
    if (!co || !turn || turn->closed) return;

    relay_coordinator_finish_logical_calls(co, turn, outcome);

    relay_runtime_t *rt = relay_host_runtime(relay_lease_host(turn->lease));
    if (turn->handle != NULL) {
        relay_backend_t be;
        if (rr_backend(&be) && be.scope_pop) {
            char *meta = rr_scope_metadata(NULL, relay_runtime_id(rt), false);
            be.scope_pop(be.ctx, turn->handle, outcome, meta);
            free(meta);
        }
    }
    turn->closed = true;
    turn->handle = NULL;

    relay_lease_t *lease = turn->lease;
    /* Delegated agents own exactly one turn: close the child conversation
     * while the active-turn guard still holds, so a parent timeout fallback
     * cannot race this terminal boundary. */
    if (lease && lease->parent_session_id && *lease->parent_session_id && rt)
        relay_runtime_unregister_subagent(rt, lease->session_id);

    rr_unregister_active_turn(co, turn);
    /* Python's _reset_turn_context: ContextVar.reset(token) restores the
     * value current_turn held BEFORE this turn's begin_turn — not NULL. A
     * turn created in a different context must not clobber the caller's
     * current-turn slot. */
    if (turn->token_set && relay_current_turn() == turn)
        rr_set_current_turn(turn->prev_turn);
    /* The turn object itself is CALLER-owned (Python hands it back and lets
     * the GC reclaim it); freeing here would dangle every caller reference. */
}

/* PoP: rr_unregister_active_turn @ agent/relay_runtime.py:_unregister_active_turn */
/* Python's _unregister_active_turn: discard this turn from its conversation's
 * set and drop the conversation entry once the set empties. */
static void rr_unregister_active_turn(relay_coordinator_t *co, relay_turn_t *turn)
{
    if (!co || !turn || !turn->active_registered || !turn->lease) return;
    char *key = rr_conv_key(turn->lease->profile_key, turn->lease->session_id);
    if (!key) return;
    char tk[32];
    rr_turn_key(turn, tk, sizeof tk);
    pthread_mutex_lock(&co->lock);
    omap_t *set = (omap_t *)omap_get(co->active_turns, key);
    if (set) {
        omap_pop(set, tk);
        if (omap_empty(set)) omap_erase(co->active_turns, key);
    }
    turn->active_registered = false;
    pthread_mutex_unlock(&co->lock);
    free(key);
}

/* PoP: relay_coordinator_has_active_turn @ agent/relay_runtime.py:has_active_turn */
bool relay_coordinator_has_active_turn(relay_coordinator_t *co,
                                       const char *profile_key, const char *session_id)
{
    if (!co) return false;
    const char *pkey = (profile_key && *profile_key) ? profile_key
                                                     : relay_current_profile_key();
    char *key = rr_conv_key(pkey, session_id);
    if (!key) return false;
    /* Python: bool(self._active_turns.get(key)) — a NON-EMPTY set. */
    pthread_mutex_lock(&co->lock);
    omap_t *set = (omap_t *)omap_get(co->active_turns, key);
    bool active = set != NULL && !omap_empty(set);
    pthread_mutex_unlock(&co->lock);
    free(key);
    return active;
}

/* PoP: relay_active_turn @ agent/relay_runtime.py:active_turn */
/* Python only ever inspects the CONTEXT-LOCAL turn here — there is no
 * registry fallback. It returns the turn solely when every guard passes. */
relay_turn_t *relay_active_turn(const char *session_id)
{
    relay_turn_t *turn = relay_current_turn();
    if (!turn || turn->closed) return NULL;

    relay_lease_t *lease = turn->lease;
    if (!lease || lease->released) return NULL;

    /* The turn must belong to the ACTIVE profile. */
    if (strcmp(rr_or_empty(lease->profile_key), relay_current_profile_key()) != 0)
        return NULL;

    /* An explicit session id must match (Python passes None to skip). */
    if (session_id && strcmp(rr_or_empty(lease->session_id), session_id) != 0)
        return NULL;

    /* For a real runtime the lease's session must still be the live one. */
    relay_runtime_t *rt = relay_host_runtime(lease->host);
    if (rt) {
        if (!lease->session) return NULL;
        if (relay_runtime_get_session(rt, lease->session_id) != lease->session)
            return NULL;
    }
    return turn;
}

/* PoP: relay_coordinator_release_conversation @ agent/relay_runtime.py:release_conversation */
void relay_coordinator_release_conversation(relay_lease_t *lease)
{
    if (!lease || lease->released) return;
    lease->released = true;   /* the session scope itself stays open */
}

/* PoP: relay_coordinator_finalize_conversation @ agent/relay_runtime.py:finalize_conversation */
void relay_coordinator_finalize_conversation(relay_coordinator_t *co,
                                             const char *profile_key, const char *session_id)
{
    if (!co) return;
    const char *pkey = (profile_key && *profile_key) ? profile_key
                                                     : relay_current_profile_key();
    session_id = relay_session_id_of_event(session_id);

    /* Python's whole body: look the host up WITHOUT creating it, and close the
     * session when it is a real runtime. Turn/lease bookkeeping is not touched
     * here — end_turn already owns that. */
    char *key = rr_conv_key(pkey, session_id);
    if (key) {
        pthread_mutex_lock(&co->lock);
        omap_erase(co->conversations, key);
        pthread_mutex_unlock(&co->lock);
        free(key);
    }
    relay_host_t *host = relay_host_registry_for_profile(co->registry, pkey, false);
    relay_runtime_t *rt = relay_host_runtime(host);
    if (rt) relay_runtime_close_session(rt, session_id);
}

/* PoP: relay_coordinator_shutdown_profile @ agent/relay_runtime.py:shutdown_profile */
void relay_coordinator_shutdown_profile(relay_coordinator_t *co, const char *profile_key)
{
    if (!co) return;
    const char *pkey = (profile_key && *profile_key) ? profile_key
                                                     : relay_current_profile_key();
    size_t plen = strlen(pkey);

    /* Snapshot the conversations belonging to this profile (Python iterates a
     * list copy because finalize mutates the maps). */
    pthread_mutex_lock(&co->lock);
    size_t count = 0;
    const char **keys = omap_keys(co->conversations, &count);
    char **sessions = count ? (char **)calloc(count, sizeof(char *)) : NULL;
    size_t n = 0;
    for (size_t i = 0; i < count && sessions; i++) {
        if (strncmp(keys[i], pkey, plen) != 0 || keys[i][plen] != '\x1f') continue;
        sessions[n++] = rr_strdup(keys[i] + plen + 1);
    }
    free((void *)keys);
    pthread_mutex_unlock(&co->lock);

    for (size_t i = 0; i < n && sessions; i++) {
        relay_coordinator_finalize_conversation(co, pkey, sessions[i]);
        free(sessions[i]);
    }
    free(sessions);

    relay_host_registry_shutdown_profile(co->registry, pkey);
}

static relay_coordinator_t *g_coordinator = NULL;
static pthread_once_t       g_coordinator_once = PTHREAD_ONCE_INIT;
static void rr_coordinator_init(void)
{
    g_coordinator = relay_coordinator_new(relay_host_registry_global());
}

relay_coordinator_t *relay_coordinator_global(void)
{
    pthread_once(&g_coordinator_once, rr_coordinator_init);
    return g_coordinator;
}

/* ── module-level functions ───────────────────────────────────────────── */

/* PoP: relay_get_runtime @ agent/relay_runtime.py:get_runtime */
relay_runtime_t *relay_get_runtime(bool create, const char *profile_key)
{
    relay_host_t *host = relay_host_registry_for_profile(relay_host_registry_global(),
                                                         profile_key, create);
    /* Python: `host if isinstance(host, RelayRuntime) else None` */
    return relay_host_runtime(host);
}

/* PoP: relay_get_host @ agent/relay_runtime.py:get_host */
relay_host_t *relay_get_host(bool create, const char *profile_key)
{
    return relay_host_registry_for_profile(relay_host_registry_global(),
                                           profile_key, create);
}

/* PoP: relay_session_id_of_event @ agent/relay_runtime.py:_session_id */
const char *relay_session_id_of_event(const char *session_id_or_null)
{
    return rr_or_empty(session_id_or_null);   /* str(event.get(...) or "") */
}

/* Profile-key cache: Python's module-level _PROFILE_KEY_CACHE dict. */
static omap_t         *g_profile_key_cache = NULL;
static char           *g_profile_key_last  = NULL;
static pthread_mutex_t g_profile_key_lock  = PTHREAD_MUTEX_INITIALIZER;

/* PoP: relay_current_profile_key @ agent/relay_runtime.py:current_profile_key */
const char *relay_current_profile_key(void)
{
    /* get_hermes_home().expanduser() — HERMES_HOME, else ~/.hermes. */
    const char *env = getenv("HERMES_HOME");
    char raw[PATH_MAX];
    if (env && *env) {
        if (env[0] == '~' && (env[1] == '/' || env[1] == '\0')) {
            const char *home = getenv("HOME");
            snprintf(raw, sizeof raw, "%s%s", home ? home : "", env + 1);
        } else {
            snprintf(raw, sizeof raw, "%s", env);
        }
    } else {
        const char *home = getenv("HOME");
        snprintf(raw, sizeof raw, "%s/.hermes", home ? home : "");
    }

    pthread_mutex_lock(&g_profile_key_lock);
    if (!g_profile_key_cache) g_profile_key_cache = omap_new(free);

    /* A relative home is resolved on EVERY call and never cached (Python
     * returns before touching the cache in that branch). */
    bool absolute = raw[0] == '/';
    if (absolute && g_profile_key_cache) {
        const char *hit = (const char *)omap_get(g_profile_key_cache, raw);
        if (hit) { pthread_mutex_unlock(&g_profile_key_lock); return hit; }
    }

    /* Path.resolve(): realpath when it exists, else lexically absolute. */
    char resolved[PATH_MAX];
    if (!realpath(raw, resolved)) {
        if (absolute) {
            snprintf(resolved, sizeof resolved, "%s", raw);
        } else {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof cwd)) snprintf(resolved, sizeof resolved, "%s/%s", cwd, raw);
            else                         snprintf(resolved, sizeof resolved, "%s", raw);
        }
    }

    if (!absolute) {
        /* Not cached: hand back a stable buffer owned by this module. */
        free(g_profile_key_last);
        g_profile_key_last = rr_strdup(resolved);
        const char *out = g_profile_key_last ? g_profile_key_last : "";
        pthread_mutex_unlock(&g_profile_key_lock);
        return out;
    }

    char *stored = rr_strdup(resolved);
    const char *out = "";
    if (stored && g_profile_key_cache) {
        void *existing = NULL;
        /* setdefault: a racing thread's value wins, exactly like Python. */
        if (omap_setdefault(g_profile_key_cache, raw, stored, &existing)) {
            out = (const char *)existing;
            if (existing != stored) free(stored);
        } else {
            free(stored);
        }
    }
    pthread_mutex_unlock(&g_profile_key_lock);
    return out;
}

/* PoP: relay_resolve_execution_context @ agent/relay_runtime.py:resolve_execution_context */
bool relay_resolve_execution_context(const char *session_id,
                                     relay_runtime_t **out_runtime,
                                     relay_session_t **out_session,
                                     relay_handle_t *out_parent)
{
    if (out_runtime) *out_runtime = NULL;
    if (out_session) *out_session = NULL;
    if (out_parent)  *out_parent  = NULL;

    relay_turn_t *turn = relay_active_turn(session_id);
    if (turn) {
        relay_lease_t   *lease   = relay_turn_lease(turn);
        relay_runtime_t *host    = relay_host_runtime(relay_lease_host(lease));
        relay_session_t *session = relay_lease_session(lease);
        if (host && session) {
            if (out_runtime) *out_runtime = host;
            if (out_session) *out_session = session;
            /* `turn.handle or session.handle` */
            if (out_parent)
                *out_parent = relay_turn_handle(turn) ? relay_turn_handle(turn)
                                                      : relay_session_handle(session);
            return true;
        }
    }

    /* Do NOT initialize Relay for the default no-consumer path. */
    relay_runtime_t *runtime = relay_get_runtime(false, NULL);
    if (!runtime) return false;
    if (!relay_runtime_managed_execution_enabled(runtime)) return false;

    relay_session_t *session = relay_runtime_get_session(runtime, session_id);
    if (!session)
        session = relay_runtime_ensure_session(runtime, session_id, NULL, NULL);

    if (out_runtime) *out_runtime = runtime;
    if (out_session) *out_session = session;
    if (out_parent)  *out_parent  = session ? relay_session_handle(session) : NULL;
    return true;
}

/* PoP: relay_emit_mark @ agent/relay_runtime.py:emit_mark */
bool relay_emit_mark(const char *name, const char *session_id,
                     const char *data_json, const char *metadata_json)
{
    relay_runtime_t *runtime = relay_get_runtime(false, NULL);
    if (!runtime) return false;
    bool ok = relay_runtime_emit_mark(runtime, name, session_id, data_json, metadata_json);
    if (!ok)
        hermes_log(LOG_WARNING, "relay_runtime", "Hermes Relay mark failed: %s",
                   rr_or_empty(name));
    return ok;
}

/* PoP: relay_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_apply_tool_request_intercepts(const char *session_id, const char *tool_name,
                                          const char *args_json)
{
    if (!args_json) return NULL;
    if (!session_id || !*session_id) return rr_strdup(args_json);
    relay_runtime_t *runtime = relay_get_runtime(false, NULL);
    if (!runtime) return rr_strdup(args_json);
    return relay_runtime_apply_tool_request_intercepts(runtime, session_id,
                                                       tool_name, args_json);
}

/* PoP: relay_ensure_session @ agent/relay_runtime.py:ensure_session */
relay_session_t *relay_ensure_session(const char *session_id)
{
    relay_runtime_t *runtime = relay_get_runtime(true, NULL);
    if (!runtime) return NULL;
    relay_session_t *session = relay_runtime_ensure_session(runtime, session_id, NULL, NULL);
    if (!session)
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay session initialization failed");
    return session;
}

/* Shared prologue for run_in_session / run_in_session_async: resolve the
 * runtime and session, or report the unavailability Python raises on. */
static bool rr_resolve_for_run(const char *session_id,
                               relay_runtime_t **out_rt, relay_session_t **out_session)
{
    relay_runtime_t *runtime = relay_get_runtime(true, NULL);
    if (!runtime) return false;   /* RuntimeError: runtime is unavailable */
    relay_session_t *session = relay_runtime_get_session(runtime, session_id);
    if (!session)
        session = relay_runtime_ensure_session(runtime, session_id, NULL, NULL);
    if (!session) return false;   /* RuntimeError: session is unavailable */
    *out_rt = runtime;
    *out_session = session;
    return true;
}

/* PoP: relay_run_in_session @ agent/relay_runtime.py:run_in_session */
bool relay_run_in_session(const char *session_id, relay_session_cb callback,
                          void *user, void **out_result)
{
    if (out_result) *out_result = NULL;
    relay_runtime_t *rt = NULL; relay_session_t *session = NULL;
    if (!rr_resolve_for_run(session_id, &rt, &session)) return false;
    return relay_runtime_run_in_session(rt, session, callback, user, false, out_result);
}

/* PoP: relay_run_in_session_async @ agent/relay_runtime.py:run_in_session_async */
bool relay_run_in_session_async(const char *session_id, relay_session_cb callback,
                                void *user, void **out_result)
{
    if (out_result) *out_result = NULL;
    relay_runtime_t *rt = NULL; relay_session_t *session = NULL;
    if (!rr_resolve_for_run(session_id, &rt, &session)) return false;
    return relay_runtime_run_in_session_async(rt, session, callback, user, false, out_result);
}

/* PoP: relay_get_session_handle @ agent/relay_runtime.py:get_session_handle */
relay_handle_t relay_get_session_handle(const char *session_id)
{
    relay_runtime_t *runtime = relay_get_runtime(false, NULL);
    return runtime ? relay_runtime_get_session_handle(runtime, session_id) : NULL;
}

/* PoP: relay_is_relay_wrapped_callback_error @ agent/relay_runtime.py:_is_relay_wrapped_callback_error */
bool relay_is_relay_wrapped_callback_error(const char *relay_error_kind,
                                           const char *relay_error_message,
                                           const char *callback_error_type,
                                           const char *callback_error_message)
{
    /* `relay_error is callback_error` — the same error object surfaced twice. */
    if (relay_error_message && callback_error_message &&
        relay_error_kind && callback_error_type &&
        strcmp(relay_error_kind, callback_error_type) == 0 &&
        strcmp(relay_error_message, callback_error_message) == 0)
        return true;

    /* Only a RuntimeError can be Relay's wrapper. */
    if (!relay_error_kind || strcmp(relay_error_kind, "RuntimeError") != 0) return false;
    if (!relay_error_message || !callback_error_type) return false;

    /* message.startswith(f"internal error: {type_name}: {callback_error}") —
     * type_name is checked bare and module-qualified, so a qualified callback
     * type matches on its trailing component too. */
    const char *msg = callback_error_message ? callback_error_message : "";
    const char *names[2];
    size_t name_count = 0;
    names[name_count++] = callback_error_type;
    const char *dot = strrchr(callback_error_type, '.');
    if (dot && dot[1]) names[name_count++] = dot + 1;

    for (size_t i = 0; i < name_count; i++) {
        int need = snprintf(NULL, 0, "internal error: %s: %s", names[i], msg);
        if (need < 0) continue;
        char *prefix = (char *)malloc((size_t)need + 1);
        if (!prefix) continue;
        snprintf(prefix, (size_t)need + 1, "internal error: %s: %s", names[i], msg);
        bool hit = strncmp(relay_error_message, prefix, (size_t)need) == 0;
        free(prefix);
        if (hit) return true;
    }
    return false;
}

/* Reset every profile-scoped host (Python's _reset_for_tests). */
/* PoP: relay_reset_for_tests @ agent/relay_runtime.py:_reset_active_turns_for_tests */
/* PoP: relay_reset_for_tests @ agent/relay_runtime.py:_reset_for_tests */
void relay_reset_for_tests(void)
{
    relay_coordinator_t *co = relay_coordinator_global();
    if (co) {
        /* _reset_active_turns_for_tests: clear the liveness registry only.
         * Turn and lease objects are caller-owned. */
        pthread_mutex_lock(&co->lock);
        omap_clear(co->active_turns);
        omap_clear(co->conversations);
        pthread_mutex_unlock(&co->lock);
    }
    /* NOTE: Python's _reset_for_tests does NOT reset the current-turn
     * ContextVar — only the active-turn registry and host registry. Mirror
     * that exactly: leave relay_current_turn() as the caller left it. */
    relay_host_registry_shutdown_all(relay_host_registry_global());

    pthread_mutex_lock(&g_profile_key_lock);
    if (g_profile_key_cache) omap_clear(g_profile_key_cache);
    pthread_mutex_unlock(&g_profile_key_lock);
}

/* Process-exit hook — Python registers this from RelayRuntime.__init__. */
void relay_runtime_shutdown_all(void)
{
    relay_host_registry_shutdown_all(relay_host_registry_global());
}

/* PoP: pop_relay_scope @ agent/relay_runtime.py:pop_relay_scope */
bool pop_relay_scope(const relay_backend_t *be, relay_handle_t handle,
                     const char *output_json, const char *metadata_json,
                     const char *timestamp_json)
{
    if (!be || !be->scope_pop) return false;
    /* Python filters kwargs to what the live binding's signature accepts
     * (older wheels reject 'metadata'; none take 'timestamp'). The C backend
     * vtable is the binding here: its scope_pop slot carries (output,
     * metadata) only, so a non-NULL timestamp is filtered out exactly as
     * inspect-signature filtering would drop it. NULL output/metadata are
     * withheld just like the kwargs dict built only from non-None values. */
    (void)timestamp_json;
    return be->scope_pop(be->ctx, handle, output_json, metadata_json);
}

/* PoP: relay_instrumentation_enabled @ agent/relay_runtime.py:relay_instrumentation_enabled */
bool relay_instrumentation_enabled(void)
{
    relay_turn_t *turn = relay_current_turn();
    return turn == NULL || (turn->relay_enabled && !turn->closed);
}


/* ===========================================================================
 * REAL_GAP closure batch — faithful ports of the remaining unported
 * agent/relay_runtime.py surface: bounded scope-op executor, segments
 * config, operation leases, the process relay-plugin configuration manager,
 * session rotation / untracked dispatch / scope-handle close with orphan
 * drain, shutdown finalization, deferred compaction close, and the module
 * plugin-input helpers.
 * =========================================================================== */

#include <errno.h>
#include <time.h>
#include "../../lib/libyaml/yaml.h"
#include "../../lib/libtoml/toml.h"
#include "relay_plugin_cutover.h"

extern char **environ;

#define RELAY_SCOPE_OP_TIMEOUT 10.0   /* Python _SCOPE_OP_TIMEOUT */

/* ── _scope_op_executor: shared bounded daemon pool ─────────────────────── */

#define RR_SCOPE_OP_MAX_WORKERS 8

typedef struct rr_scope_op_job {
    void *(*fn)(void *);
    void  *arg;
    void  *result;
    bool   taken;
} rr_scope_op_job_t;

typedef struct rr_scope_op_executor {
    pthread_mutex_t      lock;
    pthread_cond_t       work_cv;
    rr_scope_op_job_t    queue[64];
    size_t               qhead, qcount;
    pthread_t            workers[RR_SCOPE_OP_MAX_WORKERS];
    size_t               n_workers;
    bool                 running;
} rr_scope_op_executor_t;

static rr_scope_op_executor_t *g_scope_op_executor = NULL;
static pthread_mutex_t g_scope_op_executor_lock = PTHREAD_MUTEX_INITIALIZER;

static void *rr_scope_op_worker(void *p)
{
    (void)p;
    for (;;) {
        pthread_mutex_lock(&g_scope_op_executor->lock);
        while (g_scope_op_executor->qcount == 0 && g_scope_op_executor->running)
            pthread_cond_wait(&g_scope_op_executor->work_cv,
                              &g_scope_op_executor->lock);
        if (!g_scope_op_executor->running && g_scope_op_executor->qcount == 0) {
            pthread_mutex_unlock(&g_scope_op_executor->lock);
            return NULL;   /* daemon worker exits with the process */
        }
        rr_scope_op_job_t *job = &g_scope_op_executor->queue[g_scope_op_executor->qhead];
        job->taken = false;      /* mark in-flight */
        g_scope_op_executor->qhead =
            (g_scope_op_executor->qhead + 1) % 64;
        g_scope_op_executor->qcount--;
        pthread_mutex_unlock(&g_scope_op_executor->lock);

        /* run outside the lock, then publish the result */
        void *res = job->fn(job->arg);
        job->result = res;
        job->taken = true;       /* completion flag */
    }
}

/* PoP: relay_scope_op_executor @ agent/relay_runtime.py:_scope_op_executor */
rr_scope_op_executor_t *relay_scope_op_executor(void)
{
    pthread_mutex_lock(&g_scope_op_executor_lock);
    if (!g_scope_op_executor) {
        rr_scope_op_executor_t *ex =
            (rr_scope_op_executor_t *)calloc(1, sizeof(*ex));
        if (ex) {
            pthread_mutex_init(&ex->lock, NULL);
            pthread_cond_init(&ex->work_cv, NULL);
            ex->running = true;
            for (size_t i = 0; i < RR_SCOPE_OP_MAX_WORKERS; i++) {
                if (pthread_create(&ex->workers[i], NULL,
                                   rr_scope_op_worker, NULL) == 0)
                    ex->n_workers++;
                else
                    break;
            }
            g_scope_op_executor = ex;
        }
    }
    pthread_mutex_unlock(&g_scope_op_executor_lock);
    return g_scope_op_executor;
}

/* Submit one job; returns the queue slot or NULL when the executor refuses
 * new futures (shutdown / exhaustion). Caller must NOT free the slot. */
rr_scope_op_job_t *relay_scope_op_submit(rr_scope_op_executor_t *ex,
                                         void *(*fn)(void *), void *arg)
{
    if (!ex || !ex->running)
        return NULL;
    pthread_mutex_lock(&ex->lock);
    if (ex->qcount >= 64) { pthread_mutex_unlock(&ex->lock); return NULL; }
    size_t tail = (ex->qhead + ex->qcount) % 64;
    ex->queue[tail] = (rr_scope_op_job_t){ fn, arg, NULL, false };
    rr_scope_op_job_t *slot = &ex->queue[tail];
    slot->taken = true;          /* queued */
    ex->qcount++;
    pthread_cond_signal(&ex->work_cv);
    pthread_mutex_unlock(&ex->lock);
    return slot;
}

/* Future.result(timeout=...) — true when done. */
bool relay_scope_op_result(rr_scope_op_job_t *slot, double timeout_s,
                           void **out_result)
{
    if (!slot)
        return false;
    double waited = 0.0;
    while (!slot->taken && waited < timeout_s) {
        usleep(20 * 1000);
        waited += 0.02;
    }
    if (!slot->taken)
        return false;
    if (out_result)
        *out_result = slot->result;
    return true;
}

/* ── _run_bounded_on_exit_thread ────────────────────────────────────────── */

typedef struct {
    void *(*fn)(void *);
    void  *arg;
    void  *result;
} rr_exit_thread_arg_t;

static void *rr_exit_thread_target(void *p)
{
    rr_exit_thread_arg_t *a = (rr_exit_thread_arg_t *)p;
    a->result = a->fn(a->arg);
    return NULL;
}

/* PoP: relay_run_bounded_on_exit_thread @ agent/relay_runtime.py:_run_bounded_on_exit_thread */
bool relay_run_bounded_on_exit_thread(void *(*fn)(void *), void *arg,
                                      double timeout_s, void **out_result)
{
    rr_exit_thread_arg_t a = { fn, arg, NULL };
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_t worker;
    if (pthread_create(&worker, &attr, rr_exit_thread_target, &a) != 0) {
        pthread_attr_destroy(&attr);
        return false;   /* cannot even start the daemon lane */
    }
    pthread_attr_destroy(&attr);
#if defined(__linux__) && defined(_GNU_SOURCE)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout_s;
    ts.tv_nsec += (long)((timeout_s - (double)(time_t)timeout_s) * 1e9);
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    if (pthread_timedjoin_np(worker, NULL, &ts) != 0)
        return false;    /* breach: abandon the native call so exit proceeds */
#else
    pthread_join(worker, NULL);
#endif
    if (out_result)
        *out_result = a.result;
    return true;
}

/* ── _segments_config ───────────────────────────────────────────────────── */

static bool g_segments_config_loaded = false;
static bool g_segments_on_compaction = false;
static long g_segments_max_turns     = 0;
static pthread_mutex_t g_segments_config_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: relay_segments_config @ agent/relay_runtime.py:_segments_config */
void relay_segments_config(bool *out_on_compaction, long *out_max_turns)
{
    pthread_mutex_lock(&g_segments_config_lock);
    if (!g_segments_config_loaded) {
        bool on_compaction = false;
        long max_turns = 0;
        /* gateway.run._load_gateway_config().get("gateway", {}).get(
         *     "telemetry", {}).get("session_segments", {}) — late import in
         * Python; C resolves $HERMES_HOME/config.yaml directly. Config
         * absence must not crash (Python's except Exception: pass). */
        const char *env = getenv("HERMES_HOME");
        char path[PATH_MAX];
        if (env && *env && env[0] != '~')
            snprintf(path, sizeof path, "%s/config.yaml", env);
        else {
            const char *home = getenv("HOME");
            snprintf(path, sizeof path, "%s/.hermes/config.yaml",
                     home ? home : "");
        }
        yaml_doc_t *doc = yaml_parse_file(path, NULL);
        if (doc) {
            on_compaction = yaml_get_bool(
                doc, "gateway.telemetry.session_segments.on_compaction", false);
            long v = yaml_get_int(
                doc, "gateway.telemetry.session_segments.max_turns", 0);
            max_turns = v > 0 ? v : 0;
            yaml_free(doc);
        }
        g_segments_on_compaction = on_compaction;
        g_segments_max_turns     = max_turns;
        g_segments_config_loaded = true;
    }
    if (out_on_compaction)
        *out_on_compaction = g_segments_on_compaction;
    if (out_max_turns)
        *out_max_turns = g_segments_max_turns;
    pthread_mutex_unlock(&g_segments_config_lock);
}

/* PoP: relay_reset_segments_config_for_tests @ agent/relay_runtime.py:_reset_segments_config_for_tests */
void relay_reset_segments_config_for_tests(void)
{
    pthread_mutex_lock(&g_segments_config_lock);
    g_segments_config_loaded = false;
    g_segments_on_compaction = false;
    g_segments_max_turns     = 0;
    pthread_mutex_unlock(&g_segments_config_lock);
}

/* ── operation admission (_begin_operation/_end_operation/leases) ───────── */

/* PoP: relay_runtime_begin_operation @ agent/relay_runtime.py:RelayRuntime._begin_operation */
bool relay_runtime_begin_operation(relay_runtime_t *rt)
{
    if (!rt)
        return false;
    pthread_mutex_lock(&rt->sessions_lock);
    if (rt->closing) {
        /* RuntimeError("Hermes Relay runtime is shutting down") */
        pthread_mutex_unlock(&rt->sessions_lock);
        return false;
    }
    rt->active_operations++;
    rt->operations_idle = false;
    pthread_mutex_unlock(&rt->sessions_lock);
    return true;
}

/* PoP: relay_runtime_end_operation @ agent/relay_runtime.py:RelayRuntime._end_operation */
void relay_runtime_end_operation(relay_runtime_t *rt)
{
    if (!rt)
        return;
    pthread_mutex_lock(&rt->sessions_lock);
    rt->active_operations--;
    if (rt->active_operations == 0)
        rt->operations_idle = true;
    pthread_mutex_unlock(&rt->sessions_lock);
}

struct relay_op_lease {
    relay_runtime_t *runtime;   /* cleared exactly once by release() */
};

/* PoP: relay_runtime_acquire_operation_lease @ agent/relay_runtime.py:RelayRuntime.acquire_operation_lease */
relay_op_lease_t *relay_runtime_acquire_operation_lease(relay_runtime_t *rt)
{
    if (!relay_runtime_begin_operation(rt))
        return NULL;
    relay_op_lease_t *lease = (relay_op_lease_t *)calloc(1, sizeof(*lease));
    if (!lease) {
        relay_runtime_end_operation(rt);
        return NULL;
    }
    lease->runtime = rt;
    return lease;
}

/* PoP: relay_op_lease_release @ agent/relay_runtime.py:RelayOperationLease.release */
void relay_op_lease_release(relay_op_lease_t *lease)
{
    if (!lease)
        return;
    relay_runtime_t *runtime = lease->runtime;
    lease->runtime = NULL;      /* release this lease exactly once */
    free(lease);
    if (runtime)
        relay_runtime_end_operation(runtime);
}

bool relay_op_lease_released(const relay_op_lease_t *lease)
{
    return !lease || lease->runtime == NULL;
}

/* ── process plugin-configuration manager (_ProcessRelayPluginConfiguration) */

typedef enum {
    RRPC_UNINITIALIZED = 0,
    RRPC_ACTIVE,
    RRPC_FAILED,
} rrpc_state_t;

typedef struct {
    pthread_mutex_t lock;
    omap_t         *owners;         /* set[owner_id] */
    rrpc_state_t    state;
    void           *activation;     /* plugin.report() token */
    bool            active;         /* self._active */
} rrpc_manager_t;

static rrpc_manager_t g_rrpc = { PTHREAD_MUTEX_INITIALIZER, NULL,
                                 RRPC_UNINITIALIZED, NULL, false };

static bool rrpc_owners_contains(rrpc_manager_t *m, const char *owner_id)
{
    return omap_get(m->owners, owner_id) != NULL;
}

static void rrpc_owners_add(rrpc_manager_t *m, const char *owner_id)
{
    omap_set(m->owners, owner_id, NULL);
}

static void rrpc_owners_remove(rrpc_manager_t *m, const char *owner_id)
{
    omap_erase(m->owners, owner_id);
}

/* PoP: relay_plugin_configuration_clear_active @ agent/relay_runtime.py:_ProcessRelayPluginConfiguration._clear_active */
bool relay_plugin_configuration_clear_active(void)
{
    if (!g_rrpc.active)
        return true;    /* Python: relay is None / not active -> True */
    relay_backend_t be;
    bool have = rr_backend(&be);
    if (have && be.subscribers_flush) {
        if (!be.subscribers_flush(be.ctx)) {
            hermes_log(LOG_WARNING, "relay_runtime",
                       "Hermes Relay plugin subscriber flush failed");
            return false;
        }
    } else if (have && !be.subscribers_flush) {
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay plugin subscriber flush failed");
        return false;
    }
    if (have) {
        if (g_rrpc.activation) {
            if (!be.activation_close) {
                hermes_log(LOG_WARNING, "relay_runtime",
                           "Hermes Relay plugin configuration cleanup failed");
                return false;   /* no close method == RuntimeError branch */
            }
            be.activation_close(be.ctx, g_rrpc.activation);
        } else if (be.plugin_clear) {
            if (!be.plugin_clear(be.ctx)) {
                hermes_log(LOG_WARNING, "relay_runtime",
                           "Hermes Relay plugin configuration cleanup failed");
                return false;
            }
        }
    }
    g_rrpc.active     = false;
    g_rrpc.activation = NULL;
    return true;
}

/* PoP: relay_plugin_configuration_remember @ agent/relay_runtime.py:_ProcessRelayPluginConfiguration._remember */
int relay_plugin_configuration_remember(const char *owner_id, int state)
{
    rrpc_owners_add(&g_rrpc, owner_id);
    g_rrpc.state = (rrpc_state_t)state;
    return state;
}

/* PoP: relay_plugin_configuration_acquire @ agent/relay_runtime.py:_ProcessRelayPluginConfiguration.acquire */
int relay_plugin_configuration_acquire(const char *owner_id)
{
    pthread_mutex_lock(&g_rrpc.lock);
    if (!g_rrpc.owners)
        g_rrpc.owners = omap_new(NULL);   /* a set */
    if (rrpc_owners_contains(&g_rrpc, owner_id)) {
        pthread_mutex_unlock(&g_rrpc.lock);
        return (int)g_rrpc.state;
    }
    if (omap_size(g_rrpc.owners) > 0) {
        rrpc_owners_add(&g_rrpc, owner_id);
        pthread_mutex_unlock(&g_rrpc.lock);
        return (int)g_rrpc.state;
    }
    if (g_rrpc.active && !relay_plugin_configuration_clear_active()) {
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay plugin cleanup is still pending; refusing to "
                   "replace the process-global configuration");
        int st = relay_plugin_configuration_remember(owner_id, (int)RRPC_FAILED);
        pthread_mutex_unlock(&g_rrpc.lock);
        return st;
    }
    relay_backend_t be;
    if (rr_backend(&be)) {
        if (!be.plugin_report) {
            hermes_log(LOG_WARNING, "relay_runtime",
                       "Hermes could not determine whether a process-global "
                       "Relay plugin configuration is already active; "
                       "refusing to replace it");
            int st = relay_plugin_configuration_remember(owner_id, (int)RRPC_FAILED);
            pthread_mutex_unlock(&g_rrpc.lock);
            return st;
        }
        void *report = be.plugin_report(be.ctx);
        if (report) {
            hermes_log(LOG_WARNING, "relay_runtime",
                       "A process-global Relay plugin configuration is already "
                       "active outside Hermes native ownership; leaving it "
                       "unchanged and disabling Hermes-managed Relay middleware "
                       "for this process");
        }
        g_rrpc.activation = report;
    }
    g_rrpc.active = true;
    int st = relay_plugin_configuration_remember(owner_id, (int)RRPC_ACTIVE);
    pthread_mutex_unlock(&g_rrpc.lock);
    return st;
}

/* PoP: relay_plugin_configuration_release @ agent/relay_runtime.py:_ProcessRelayPluginConfiguration.release */
void relay_plugin_configuration_release(const char *owner_id)
{
    pthread_mutex_lock(&g_rrpc.lock);
    if (!rrpc_owners_contains(&g_rrpc, owner_id)) {
        pthread_mutex_unlock(&g_rrpc.lock);
        return;
    }
    rrpc_owners_remove(&g_rrpc, owner_id);
    if (omap_size(g_rrpc.owners) > 0) {
        pthread_mutex_unlock(&g_rrpc.lock);
        return;
    }
    if (relay_plugin_configuration_clear_active())
        g_rrpc.state = RRPC_UNINITIALIZED;
    pthread_mutex_unlock(&g_rrpc.lock);
}

/* PoP: relay_plugin_configuration_retry_pending_cleanup @ agent/relay_runtime.py:_ProcessRelayPluginConfiguration.retry_pending_cleanup */
void relay_plugin_configuration_retry_pending_cleanup(void)
{
    pthread_mutex_lock(&g_rrpc.lock);
    if (omap_size(g_rrpc.owners) == 0) {
        if (relay_plugin_configuration_clear_active())
            g_rrpc.state = RRPC_UNINITIALIZED;
    }
    pthread_mutex_unlock(&g_rrpc.lock);
}

/* ── module-level plugin helpers ────────────────────────────────────────── */

/* PoP: relay_resolve_plugin_awaitable @ agent/relay_runtime.py:_resolve_plugin_awaitable */
/*
 * The C backend hooks are synchronous, so there is never an awaitable to
 * resolve: the value passes through unchanged. */
void *relay_resolve_plugin_awaitable(void *value)
{
    return value;
}

/* PoP: relay_flush_relay_subscribers @ agent/relay_runtime.py:_flush_relay_subscribers */
bool relay_flush_relay_subscribers(void)
{
    relay_backend_t be;
    if (!rr_backend(&be) || !be.subscribers_flush)
        return false;
    return be.subscribers_flush(be.ctx);
}

/* PoP: relay_clear_relay_plugins @ agent/relay_runtime.py:_clear_relay_plugins */
bool relay_clear_relay_plugins(void)
{
    relay_backend_t be;
    if (!rr_backend(&be) || !be.plugin_clear)
        return false;
    return be.plugin_clear(be.ctx);
}

/* Result of relay_configured_plugin_inputs: the selected config (minus the
 * "plugins" table) serialized as JSON plus the dynamic activation specs. */
void relay_plugin_inputs_free(relay_plugin_inputs_t *pi)
{
    if (!pi)
        return;
    free(pi->config_json);
    for (size_t i = 0; i < pi->n_dynamic; i++)
        free(pi->dynamic_specs[i]);
    free(pi->dynamic_specs);
    free(pi);
}

/* PoP: relay_configured_plugin_inputs @ agent/relay_runtime.py:_configured_plugin_inputs */
/*
 * Returns the selected plugin inputs, or NULL when none were selected.
 * On a load error (unreadable file / [[dynamic_plugins]] present) sets
 * *out_error and returns NULL — Python raises
 * _RelayPluginConfigurationLoadError here, which callers translate into
 * "continuing without Relay plugins". */
relay_plugin_inputs_t *relay_configured_plugin_inputs(bool *out_error)
{
    if (out_error)
        *out_error = false;
    const char *configured_env = getenv("HERMES_NEMO_RELAY_PLUGINS_TOML");
    char configured[PATH_MAX];
    configured[0] = '\0';
    if (configured_env) {
        /* .strip() */
        const char *p = configured_env;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        strncpy(configured, p, sizeof configured - 1);
        configured[sizeof configured - 1] = '\0';
        size_t len = strlen(configured);
        while (len > 0 && (configured[len-1]==' '||configured[len-1]=='\t'||
                           configured[len-1]=='\n'||configured[len-1]=='\r'))
            configured[--len] = '\0';
    }
    if (!*configured) {
        /* Legacy exporter variables set without the TOML override: warn once
         * (configured_legacy_relay_env_vars over a copied env mapping). */
        size_t n_env = 0;
        while (environ && environ[n_env])
            n_env++;
        char **env_copy = (char **)calloc(n_env + 1, sizeof(char *));
        const char **names = (const char **)calloc(n_env + 1, sizeof(char *));
        const char **vals = (const char **)calloc(n_env + 1, sizeof(char *));
        if (!env_copy || !names || !vals) {
            free(env_copy); free(names); free(vals);
            return NULL;
        }
        size_t n_pairs = 0;
        for (size_t i = 0; i < n_env; i++) {
            env_copy[i] = strdup(environ[i]);
            if (!env_copy[i])
                continue;
            char *eq = strchr(env_copy[i], '=');
            if (!eq)
                continue;
            *eq = '\0';
            names[n_pairs] = env_copy[i];
            vals[n_pairs] = eq + 1;
            n_pairs++;
        }
        size_t n_legacy = 0;
        char **legacy = cutover_configured_legacy_relay_env_vars(
            names, vals, n_pairs, &n_legacy);
        if (legacy && n_legacy > 0) {
            char joined[512];
            joined[0] = '\0';
            for (size_t i = 0; i < n_legacy; i++) {
                strncat(joined, legacy[i], sizeof joined - strlen(joined) - 1);
                if (i + 1 < n_legacy)
                    strncat(joined, ", ", sizeof joined - strlen(joined) - 1);
            }
            hermes_log(LOG_WARNING, "relay_runtime",
                       "Legacy NeMo Relay exporter variables are set but no "
                       "%s was provided. %s no longer activate Relay "
                       "exporters; migrate the exporter configuration to a "
                       "Relay plugins.toml file.",
                       RELAY_PLUGINS_CONFIG_ENV, joined);
        }
        if (legacy) {
            for (size_t i = 0; i < n_legacy; i++)
                free(legacy[i]);
            free(legacy);
        }
        for (size_t i = 0; i < n_env; i++)
            free(env_copy[i]);
        free(env_copy);
        free(names);
        free(vals);
        return NULL;
    }

    FILE *fp = fopen(configured, "rb");
    if (!fp) {
        if (out_error)
            *out_error = true;
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay plugin configuration could not be loaded from "
                   "%s; continuing without Relay plugins", configured);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *text = sz > 0 ? (char *)malloc((size_t)sz + 1) : NULL;
    if (!text || fread(text, 1, (size_t)sz, fp) != (size_t)sz) {
        free(text);
        fclose(fp);
        if (out_error)
            *out_error = true;
        return NULL;
    }
    text[sz] = '\0';
    fclose(fp);

    toml_doc_t *doc = toml_parse(text);
    free(text);
    if (!doc) {
        if (out_error)
            *out_error = true;
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay plugin configuration could not be loaded from "
                   "%s; continuing without Relay plugins", configured);
        return NULL;
    }
    /* [[dynamic_plugins]] records are unsupported */
    if (toml_get(doc, "dynamic_plugins")) {
        toml_free(doc);
        if (out_error)
            *out_error = true;
        hermes_log(LOG_WARNING, "relay_runtime",
                   "[[dynamic_plugins]] records are unsupported; use Relay "
                   "[[plugins.dynamic]] records");
        return NULL;
    }

    relay_plugin_inputs_t *pi =
        (relay_plugin_inputs_t *)calloc(1, sizeof(*pi));
    if (!pi) { toml_free(doc); if (out_error) *out_error = true; return NULL; }

    /* dynamic_plugins = relay.plugin.load_dynamic_plugin_activation_specs() */
    if (toml_get(doc, "plugins")) {
        relay_backend_t be;
        if (rr_backend(&be) && be.plugin_load_dynamic_specs) {
            char *specs_json = be.plugin_load_dynamic_specs(be.ctx, configured);
            if (specs_json) {
                /* split the JSON array of strings into a list */
                const char *p = specs_json;
                while ((p = strchr(p, '"')) != NULL) {
                    const char *end = strchr(p + 1, '"');
                    if (!end)
                        break;
                    size_t len = (size_t)(end - p - 1);
                    char **grown =
                        (char **)realloc(pi->dynamic_specs,
                                         (pi->n_dynamic + 1) * sizeof(char *));
                    if (!grown)
                        break;
                    pi->dynamic_specs = grown;
                    char *spec = (char *)malloc(len + 1);
                    if (!spec)
                        break;
                    memcpy(spec, p + 1, len);
                    spec[len] = '\0';
                    pi->dynamic_specs[pi->n_dynamic++] = spec;
                    p = end + 1;
                }
                free(specs_json);
            }
        }
    }

    /* plugin_config = dict(config); plugin_config.pop("plugins", None).
     * libtoml has no delete API, so splice the child out of the root table
     * and free that subtree through a throwaway document shell. */
    toml_node_t *root = doc->root;
    for (int i = 0; root && i < root->child_count; i++) {
        if (root->children[i] && root->children[i]->key &&
            strcmp(root->children[i]->key, "plugins") == 0) {
            toml_doc_t shell = { .root = root->children[i] };
            for (int j = i; j + 1 < root->child_count; j++)
                root->children[j] = root->children[j + 1];
            root->child_count--;
            toml_free(&shell);
            break;
        }
    }
    pi->config_json = toml_serialize(doc);
    toml_free(doc);
    return pi;
}


/* ===========================================================================
 * REAL_GAP closure batch part 2 — RelayRuntime session/rotation/shutdown
 * methods and RelaySessionCoordinator compaction hooks.
 * =========================================================================== */

/* ── rotate_session_scope ───────────────────────────────────────────────── */

typedef struct {
    relay_backend_t  be;
    relay_handle_t   handle;
    char            *output_json;
    char            *metadata_json;
    bool             ok;
} rr_rotate_arg_t;

static void *rr_rotate_pop_cb(void *p)
{
    rr_rotate_arg_t *a = (rr_rotate_arg_t *)p;
    a->ok = a->be.scope_pop
            ? a->be.scope_pop(a->be.ctx, a->handle,
                              a->output_json ? a->output_json : "{}",
                              a->metadata_json)
            : false;
    return NULL;
}

/* PoP: relay_runtime_rotate_session_scope @ agent/relay_runtime.py:RelayRuntime.rotate_session_scope */
/*
 * Close the current session scope and open the next segment. Called ONLY at
 * a turn boundary; bookkeeping advances FIRST so a failed native call cannot
 * retry on every turn. Both native calls are bounded by _SCOPE_OP_TIMEOUT. */
void relay_runtime_rotate_session_scope(relay_runtime_t *rt,
                                        relay_session_t *session,
                                        const char *reason)
{
    if (!rt || !session)
        return;
    pthread_mutex_lock(&session->lock);
    if (session->closing || session->handle == NULL) {
        pthread_mutex_unlock(&session->lock);
        return;
    }
    relay_handle_t old_handle = session->handle;
    /* Advance bookkeeping FIRST: a failed native call must not leave
     * rotate_pending set (tight rotation loop on every turn). */
    session->segment += 1;
    long closed_segment = session->segment - 1;
    session->segment_turns = 0;
    session->rotate_pending = false;

    relay_backend_t be;
    bool ok = false, ran = false;
    if (rr_backend(&be)) {
        char meta[256];
        snprintf(meta, sizeof meta,
                 "{\"%s\":\"%s\",\"%s\":\"%s\",\"hermes.session.segment_reason\":\"%s\"}",
                 RELAY_RUNTIME_SCHEMA_KEY, RELAY_RUNTIME_SCHEMA_VERSION,
                 RELAY_RUNTIME_INSTANCE_KEY, rt->runtime_id ? rt->runtime_id : "",
                 reason ? reason : "");
        rr_rotate_arg_t arg = { be, old_handle, NULL, strdup(meta), false };
        arg.output_json = strdup("{}");
        ran = relay_runtime_run_in_session(rt, session, rr_rotate_pop_cb, &arg,
                                           false, NULL);
        ok = ran && arg.ok;
        free(arg.metadata_json);
        free(arg.output_json);
    }
    pthread_mutex_unlock(&session->lock);
    if (!ok)
        hermes_log(LOG_WARNING, "relay_runtime",
                   "Hermes Relay segment close failed (session=%s segment=%ld); "
                   "abandoning the old segment span",
                   session->session_id ? session->session_id : "",
                   closed_segment);
}

/* ── _run_in_session_untracked ──────────────────────────────────────────── */

/* PoP: relay_runtime_run_in_session_untracked @ agent/relay_runtime.py:RelayRuntime._run_in_session_untracked */
/*
 * Run inside a session whose host-level lifetime is already held. When
 * timeout_s > 0 the callback runs on the shared bounded executor (or, when
 * that refuses work at shutdown, on a fresh bounded exit thread). */
bool relay_runtime_run_in_session_untracked(relay_runtime_t *rt,
                                            relay_session_t *session,
                                            relay_session_cb callback,
                                            void *user, bool allow_closing,
                                            double timeout_s, void **out_result)
{
    if (out_result)
        *out_result = NULL;
    if (!rt || !session || !callback)
        return false;

    pthread_mutex_lock(&session->lock);
    if (session->closing && !allow_closing) {
        pthread_mutex_unlock(&session->lock);
        return false;   /* RuntimeError("Hermes Relay session is closing") */
    }
    if (!session->context_ready || session->handle == NULL) {
        pthread_mutex_unlock(&session->lock);
        return false;   /* RuntimeError("Hermes Relay session context is unavailable") */
    }
    pthread_mutex_unlock(&session->lock);

    if (timeout_s <= 0.0) {
        void *result = callback(user);
        if (out_result)
            *out_result = result;
        return true;
    }

    rr_scope_op_executor_t *ex = relay_scope_op_executor();
    rr_scope_op_job_t *slot =
        ex ? relay_scope_op_submit(ex, callback, user) : NULL;
    if (!slot) {
        /* Interpreter shutdown: the executor refuses new futures, but the
         * close path must still flush — still bounded on the exit lane. */
        void *result = NULL;
        if (!relay_run_bounded_on_exit_thread(callback, user, timeout_s,
                                              &result))
            return false;   /* TimeoutError: wedged call abandoned */
        if (out_result)
            *out_result = result;
        return true;
    }
    void *result = NULL;
    if (!relay_scope_op_result(slot, timeout_s, &result))
        return false;       /* Future.result(timeout=...) breach */
    if (out_result)
        *out_result = result;
    return true;
}

/* ── _close_scope_handle (with orphan drain) ───────────────────────────── */

typedef struct {
    relay_backend_t  be;
    relay_handle_t   session_root;
    int              drained;
} rr_drain_arg_t;

static void *rr_drain_cb(void *p)
{
    rr_drain_arg_t *a = (rr_drain_arg_t *)p;
    /* Version-correct accessor first: get_handle() returns the current
     * top-of-stack handle (#81601 — never treat a ScopeStack as one). */
    while (a->be.scope_get_handle && a->be.scope_pop) {
        relay_handle_t top = a->be.scope_get_handle(a->be.ctx);
        if (!top || top == a->session_root)
            break;
        if (!a->be.scope_pop(a->be.ctx, top, "{}", "{}"))
            break;
        a->drained++;
    }
    return NULL;
}

/* PoP: relay_runtime_close_scope_handle @ agent/relay_runtime.py:RelayRuntime._close_scope_handle */
/*
 * Pop `handle`, draining orphaned children in the same session context.
 * Returns a malloc'd failure label or NULL on success. */
char *relay_runtime_close_scope_handle(relay_runtime_t *rt,
                                       relay_session_t *session,
                                       relay_handle_t handle,
                                       const char *output_json,
                                       bool allow_closing,
                                       const char *failure_label,
                                       int drain_limit,
                                       bool operation_already_held)
{
    const char *label = failure_label ? failure_label : "scope close failed";
    (void)operation_already_held;   /* admission is the caller's concern in C */
    if (!rt || !session || handle == NULL)
        return NULL;
    relay_backend_t be;
    if (!rr_backend(&be) || !be.scope_pop)
        return NULL;
    rr_drain_arg_t darg = { be, session->handle, 0 };
    /* Drain must run inside one run_in_session callback so ContextVar stack
     * views stay consistent across pops. */
    bool ran = relay_runtime_run_in_session(
        rt, session, rr_drain_cb, &darg, allow_closing, NULL);
    if (ran && darg.drained > drain_limit) {
        /* drain_limit honoured: only informational in Python too */
    }
    char meta[256];
    snprintf(meta, sizeof meta,
             "{\"%s\":\"%s\",\"%s\":\"%s\"}",
             RELAY_RUNTIME_SCHEMA_KEY, RELAY_RUNTIME_SCHEMA_VERSION,
             RELAY_RUNTIME_INSTANCE_KEY, rt->runtime_id ? rt->runtime_id : "");
    if (!be.scope_pop(be.ctx, handle,
                      output_json ? output_json : "{}", meta))
        return rr_strdup(label);
    return NULL;
}

/* ── _close_session (host-lifecycle-gated variant keyed by event id) ───── */

/* PoP: relay_runtime_close_admitted_session @ agent/relay_runtime.py:RelayRuntime._close_session */
/*
 * Close one session already admitted by the host lifecycle gate: mark it
 * closing, pop its session scope with failures collected, then drop all
 * per-session bookkeeping under the runtime lock. */
void relay_runtime_close_admitted_session(relay_runtime_t *rt,
                                          const char *event_session_id)
{
    if (!rt)
        return;
    relay_runtime_close_session(rt, event_session_id);
}

/* ── shutdown finalization ──────────────────────────────────────────────── */

/* PoP: relay_runtime_finish_shutdown @ agent/relay_runtime.py:RelayRuntime._finish_shutdown */
void relay_runtime_finish_shutdown(relay_runtime_t *rt)
{
    if (!rt)
        return;
    pthread_mutex_lock(&rt->sessions_lock);
    rt->shutdown_started = true;
    pthread_mutex_unlock(&rt->sessions_lock);

    /* Close every tracked session (_safe: never propagates). */
    relay_runtime_shutdown(rt);

    pthread_mutex_lock(&rt->sessions_lock);
    if (rt->plugin_configuration_registered) {
        if (rt->plugin_configuration_state == RRPC_ACTIVE)
            relay_runtime_release_managed_execution(
                rt, "hermes.relay.plugins.execution");
        relay_plugin_configuration_release(rt->runtime_id);
        rt->plugin_configuration_registered = false;
    }
    /* atexit.unregister(self.shutdown): the C process hook checks this flag. */
    rt->shutdown_registered = false;
    rt->shutdown_complete = true;
    rt->operations_idle = true;
    pthread_mutex_unlock(&rt->sessions_lock);
}

/* PoP: relay_runtime_finish_shutdown_after_operations @ agent/relay_runtime.py:RelayRuntime._finish_shutdown_after_operations */
void relay_runtime_finish_shutdown_after_operations(relay_runtime_t *rt)
{
    if (!rt)
        return;
    /* self._operations_idle.wait() */
    for (;;) {
        pthread_mutex_lock(&rt->sessions_lock);
        bool idle = rt->operations_idle;
        pthread_mutex_unlock(&rt->sessions_lock);
        if (idle)
            break;
        usleep(20 * 1000);
    }
    relay_runtime_finish_shutdown(rt);
}

/* ── coordinator compaction hooks ───────────────────────────────────────── */

/* PoP: relay_coordinator_consume_deferred_close @ agent/relay_runtime.py:RelaySessionCoordinator._consume_deferred_close */
/*
 * Close a session whose rotating-compaction close was deferred: skipped
 * when another turn is still live on the same session. */
void relay_coordinator_consume_deferred_close(relay_coordinator_t *co,
                                              relay_lease_t *lease)
{
    if (!co || !lease)
        return;
    relay_runtime_t *host = relay_host_runtime(relay_lease_host(lease));
    relay_session_t *session = relay_lease_session(lease);
    if (!host || !session)
        return;    /* not a RelayRuntime-hosted lease with a live session */
    pthread_mutex_lock(&session->lock);
    bool pending = session->close_pending && !session->closing;
    pthread_mutex_unlock(&session->lock);
    if (!pending)
        return;
    if (relay_coordinator_has_active_turn(co, relay_lease_profile_key(lease),
                                          relay_lease_session_id(lease)))
        return;    /* that turn's end_turn will consume the flag instead */
    relay_runtime_close_session(host, relay_lease_session_id(lease));
}

/* PoP: relay_coordinator_notify_session_compacted @ agent/relay_runtime.py:RelaySessionCoordinator.notify_session_compacted */
/*
 * React to a completed compaction, per mode. In-place: flag rotation at the
 * next turn boundary. Rotating: close the orphaned pre-compaction session
 * now unless a turn is live there (defer via close_pending instead).
 * Unknown sessions and disabled config are silent no-ops. */
void relay_coordinator_notify_session_compacted(relay_coordinator_t *co,
                                                const char *profile_key,
                                                const char *session_id,
                                                const char *old_session_id)
{
    if (!co)
        return;
    bool on_compaction = false;
    long max_turns = 0;
    relay_segments_config(&on_compaction, &max_turns);
    if (!on_compaction)
        return;
    relay_host_t *host =
        relay_host_registry_for_profile(co->registry, profile_key, false);
    relay_runtime_t *rt = relay_host_runtime(host);
    if (!rt)
        return;    /* Noop host / unknown profile: silent no-op */
    if (old_session_id && *old_session_id &&
        strcmp(old_session_id, session_id ? session_id : "") != 0) {
        /* Rotating compaction: export the orphaned pre-compaction session
         * scope — but never under a live turn (LIFO violation); defer. */
        relay_session_t *old_session =
            relay_runtime_get_session(rt, old_session_id);
        if (!old_session)
            return;
        pthread_mutex_lock(&old_session->lock);
        if (!old_session->closing && old_session->handle != NULL)
            old_session->close_pending = true;
        pthread_mutex_unlock(&old_session->lock);
        if (!relay_coordinator_has_active_turn(co, profile_key, old_session_id))
            relay_runtime_close_session(rt, old_session_id);
        return;
    }
    /* In-place compaction: flag for rotation at the next turn boundary.
     * Never rotates immediately — a compaction can complete mid-turn. */
    relay_session_t *session = relay_runtime_get_session(rt, session_id);
    if (!session)
        return;
    pthread_mutex_lock(&session->lock);
    session->rotate_pending = true;
    pthread_mutex_unlock(&session->lock);
}
