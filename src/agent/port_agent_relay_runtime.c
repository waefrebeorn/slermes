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

static void *rr_async_trampoline(void *p)
{
    rr_async_arg_t *a = (rr_async_arg_t *)p;
    a->result = a->callback(a->user);
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
bool relay_noop_available(void) { return false; }

/* PoP: relay_noop_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_noop_apply_tool_request_intercepts(const char *session_id,
                                               const char *tool_name,
                                               const char *args_json)
{
    (void)session_id; (void)tool_name;      /* Python: `del session_id, tool_name` */
    return args_json ? rr_strdup(args_json) : NULL;
}

/* PoP: relay_noop_retain_managed_execution @ agent/relay_runtime.py:retain_managed_execution */
void relay_noop_retain_managed_execution(const char *consumer) { (void)consumer; }
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
        if (!set) {
            set = omap_new(NULL);
            if (set && !omap_set(co->active_turns, key, set)) { omap_free(set); set = NULL; }
        }
        if (set) omap_set(set, tk, turn);
        turn->active_registered = set != NULL;
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
