/*
 * port_agent_relay_runtime.h — C11 port of agent/relay_runtime.py.
 *
 * Profile-scoped NeMo Relay runtimes owned by the Hermes agent core: session
 * scope stacks, the turn/conversation coordinator, and the profile host
 * registry. Hermes core calls into this at the LLM and tool boundaries to
 * parent observability scopes to the right session and turn.
 *
 * Python -> C mapping (the whole design in one table):
 *   contextvars.ContextVar  -> pthread TLS (the turn context follows the
 *                              thread that runs the turn, which is how the C
 *                              agent loop dispatches work).
 *   dict[str, X]            -> omap_t (insertion-ordered, lib/libomap).
 *   threading.RLock         -> pthread_mutex_t with PTHREAD_MUTEX_RECURSIVE.
 *   importlib nemo_relay    -> relay_backend_t vtable, injected by whoever
 *                              owns the wire. Absent backend => the runtime
 *                              degrades exactly like Python's NoopRelayRuntime.
 *   atexit.register         -> relay_runtime_shutdown_all at process exit.
 *
 * Opaque structs only; callers touch state through the accessors below.
 */

#ifndef PORT_AGENT_RELAY_RUNTIME_H
#define PORT_AGENT_RELAY_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

/* Callback form of Python's `run_in_session(session, callback, *args)`. */
typedef void *(*relay_session_cb)(void *user);

/* ── Contract constants (mirror the Python module's globals) ──────────── */

#define RELAY_SESSION_SCOPE          "hermes.session"
#define RELAY_TURN_SCOPE             "hermes.turn"
#define RELAY_LOGICAL_LLM_SCOPE      "hermes.logical_llm_call"
#define RELAY_RUNTIME_SCHEMA_KEY     "hermes.relay.schema_version"
#define RELAY_RUNTIME_SCHEMA_VERSION "hermes.relay.runtime.v1"
#define RELAY_RUNTIME_INSTANCE_KEY   "hermes.relay.runtime_instance"

/* Relay scope kinds (nemo_relay.ScopeType). */
typedef enum {
    RELAY_SCOPE_AGENT = 0,
    RELAY_SCOPE_FUNCTION,
    RELAY_SCOPE_LLM,
    RELAY_SCOPE_TOOL
} relay_scope_type_t;

/* ── Opaque handles ───────────────────────────────────────────────────── */

typedef struct relay_session       relay_session_t;   /* RelaySession */
typedef struct relay_runtime       relay_runtime_t;   /* RelayRuntime */
typedef struct relay_host          relay_host_t;      /* RelayRuntime | NoopRelayRuntime */
typedef struct relay_host_registry relay_host_registry_t;
typedef struct relay_lease         relay_lease_t;     /* ConversationLease */
typedef struct relay_turn          relay_turn_t;      /* RelayTurnContext */
typedef struct relay_coordinator   relay_coordinator_t;

/* Opaque scope handle returned by the backend (nemo_relay ScopeHandle). */
typedef void *relay_handle_t;

/* ── Backend vtable (the injected `nemo_relay` binding) ───────────────── */

/* Python reaches Relay through `importlib.import_module("nemo_relay")`. C has
 * no import system, so the binding is injected as a vtable. Every hook may be
 * NULL; a NULL hook fails the operation the same way a missing wheel does in
 * Python, which is what drives the Noop degradation path. */
typedef struct relay_backend {
    void *ctx;   /* backend-owned state, passed back to every hook */

    /* scope.push(name, type, handle=parent, data=, input={}, metadata=) ->
     * handle. Returns NULL on failure. `metadata` is a JSON object string. */
    relay_handle_t (*scope_push)(void *ctx, const char *name,
                                 relay_scope_type_t type,
                                 relay_handle_t parent,
                                 const char *data_json,
                                 const char *metadata_json);

    /* scope.pop(handle, output=, metadata=). Returns false on failure. */
    bool (*scope_pop)(void *ctx, relay_handle_t handle,
                      const char *output_json, const char *metadata_json);

    /* scope.event(name, handle=, data=, metadata=). Returns false on failure. */
    bool (*scope_event)(void *ctx, const char *name, relay_handle_t handle,
                        const char *data_json, const char *metadata_json);

    /* subscribers.flush(). Returns false on failure. */
    bool (*subscribers_flush)(void *ctx);

    /* tools.request_intercepts(tool_name, args_json) -> rewritten args JSON
     * (malloc'd, caller frees) or NULL to leave the arguments unchanged.
     * NULL hook == Python's "not callable" branch. */
    char *(*tools_request_intercepts)(void *ctx, const char *tool_name,
                                      const char *args_json);

    /* llm.execute(name, relay_request_json, invoke_callback, invoke_user,
     *   handle=parent, metadata_json, model_name, codec, response_codec) ->
     * returns managed JSON result via out_result (malloc'd, caller frees).
     * `invoke_callback` is called on the session thread with `invoke_user`;
     * it must return a malloc'd JSON string (the result of the provider callback).
     * NULL hook => execute() degrades to direct callback invocation (no Relay). */
    bool (*llm_execute)(void *ctx, const char *name,
                        const char *relay_request_json,
                        relay_session_cb invoke_callback, void *invoke_user,
                        relay_handle_t parent,
                        const char *metadata_json,
                        const char *model_name,
                        const char *codec, const char *response_codec,
                        char **out_result);

    /* llm.stream_execute(name, relay_request_json, provider_stream_cb,
     *   provider_stream_user, observe_chunk_cb, observe_chunk_user,
     *   finalizer_cb, finalizer_user,
     *   handle=parent, metadata_json, model_name, codec, response_codec) ->
     * returns a stream handle (opaque, backend-owned) via out_handle.
     * Each callback is relay_session_cb-style (void* user -> void* ret);
     *   provider_stream_cb runs as an async generator (yields JSON strings,
     *   NULL-terminated **out_chunk for synchronous pull).
     * NULL hook => stream() degrades to direct stream-factory invocation. */
    bool (*llm_stream_execute)(void *ctx, const char *name,
                               const char *relay_request_json,
                               relay_session_cb provider_stream_cb,
                               void *provider_stream_user,
                               relay_session_cb observe_chunk_cb,
                               void *observe_chunk_user,
                               relay_session_cb finalizer_cb,
                               void *finalizer_user,
                               relay_handle_t parent,
                               const char *metadata_json,
                               const char *model_name,
                               const char *codec, const char *response_codec,
                               void **out_handle);

    /* llm_stream_next(handle, out_chunk_json) -> true if a chunk was produced,
     * false on end-of-stream or error. Caller frees *out_chunk_json. */
    bool (*llm_stream_next)(void *ctx, void *handle, char **out_chunk_json);

    /* llm_stream_close(handle) — release backend stream resources. */
    void (*llm_stream_close)(void *ctx, void *handle);
} relay_backend_t;

/* Install the process-wide backend used by every runtime created afterwards
 * (the C analogue of `_load_nemo_relay`). Passing NULL clears it, which makes
 * subsequent host creation yield reduced-capability hosts. */
/* PoP: relay_runtime_set_backend @ agent/relay_runtime.py:_load_nemo_relay */
void relay_runtime_set_backend(const relay_backend_t *backend);

/* True when a backend is installed. */
bool relay_runtime_backend_available(void);

/* ── RelaySession accessors ───────────────────────────────────────────── */

const char     *relay_session_id(const relay_session_t *session);
const char     *relay_session_parent_id(const relay_session_t *session);
relay_handle_t  relay_session_handle(const relay_session_t *session);
bool            relay_session_closing(const relay_session_t *session);

/* ── RelayRuntime ─────────────────────────────────────────────────────── */

/* PoP: relay_runtime_new @ agent/relay_runtime.py:__init__ */
relay_runtime_t *relay_runtime_new(const char *profile_key);
void             relay_runtime_free(relay_runtime_t *rt);

const char *relay_runtime_profile_key(const relay_runtime_t *rt);
const char *relay_runtime_id(const relay_runtime_t *rt);

/* PoP: relay_runtime_retain_managed_execution @ agent/relay_runtime.py:retain_managed_execution */
bool relay_runtime_retain_managed_execution(relay_runtime_t *rt, const char *consumer);
/* PoP: relay_runtime_release_managed_execution @ agent/relay_runtime.py:release_managed_execution */
void relay_runtime_release_managed_execution(relay_runtime_t *rt, const char *consumer);
/* PoP: relay_runtime_managed_execution_enabled @ agent/relay_runtime.py:managed_execution_enabled */
bool relay_runtime_managed_execution_enabled(relay_runtime_t *rt);

/* PoP: relay_runtime_ensure_session @ agent/relay_runtime.py:ensure_session */
relay_session_t *relay_runtime_ensure_session(relay_runtime_t *rt,
                                              const char *session_id,
                                              const char *data_json,
                                              const char *metadata_json);
/* PoP: relay_runtime_register_subagent @ agent/relay_runtime.py:register_subagent */
relay_session_t *relay_runtime_register_subagent(relay_runtime_t *rt,
                                                 const char *parent_session_id,
                                                 const char *child_session_id,
                                                 const char *metadata_json);
/* PoP: relay_runtime_unregister_subagent @ agent/relay_runtime.py:unregister_subagent */
void relay_runtime_unregister_subagent(relay_runtime_t *rt, const char *child_session_id);

/* PoP: relay_runtime_get_session @ agent/relay_runtime.py:get_session */
relay_session_t *relay_runtime_get_session(relay_runtime_t *rt, const char *session_id);
/* PoP: relay_runtime_get_session_handle @ agent/relay_runtime.py:get_session_handle */
relay_handle_t   relay_runtime_get_session_handle(relay_runtime_t *rt, const char *session_id);

/* PoP: relay_runtime_run_in_session @ agent/relay_runtime.py:run_in_session */
bool relay_runtime_run_in_session(relay_runtime_t *rt, relay_session_t *session,
                                  relay_session_cb callback, void *user,
                                  bool allow_closing, void **out_result);
/* Async form: Python awaits inside the session's saved context; C runs the
 * callback on a dedicated thread that inherits the session context and joins.
 * PoP: relay_runtime_run_in_session_async @ agent/relay_runtime.py:run_in_session_async */
bool relay_runtime_run_in_session_async(relay_runtime_t *rt, relay_session_t *session,
                                        relay_session_cb callback, void *user,
                                        bool allow_closing, void **out_result);

/* PoP: relay_runtime_emit_mark @ agent/relay_runtime.py:emit_mark */
bool relay_runtime_emit_mark(relay_runtime_t *rt, const char *name,
                             const char *session_id,
                             const char *data_json, const char *metadata_json);

/* Returns a malloc'd rewritten-args JSON string, or a copy of `args_json`
 * when no rewrite applies. NULL only when `args_json` is NULL.
 * PoP: relay_runtime_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_runtime_apply_tool_request_intercepts(relay_runtime_t *rt,
                                                  const char *session_id,
                                                  const char *tool_name,
                                                  const char *args_json);

/* PoP: relay_runtime_close_session @ agent/relay_runtime.py:close_session */
void relay_runtime_close_session(relay_runtime_t *rt, const char *session_id);
/* PoP: relay_runtime_shutdown @ agent/relay_runtime.py:shutdown */
void relay_runtime_shutdown(relay_runtime_t *rt);

/* ── NoopRelayRuntime (explicit reduced-capability host) ──────────────── */

/* PoP: relay_noop_available @ agent/relay_runtime.py:available */
bool relay_noop_available(void);
/* PoP: relay_noop_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_noop_apply_tool_request_intercepts(const char *session_id,
                                               const char *tool_name,
                                               const char *args_json);
/* PoP: relay_noop_retain_managed_execution @ agent/relay_runtime.py:retain_managed_execution */
void relay_noop_retain_managed_execution(const char *consumer);
/* PoP: relay_noop_release_managed_execution @ agent/relay_runtime.py:release_managed_execution */
void relay_noop_release_managed_execution(const char *consumer);
/* PoP: relay_noop_managed_execution_enabled @ agent/relay_runtime.py:managed_execution_enabled */
bool relay_noop_managed_execution_enabled(void);
/* PoP: relay_noop_shutdown @ agent/relay_runtime.py:shutdown */
void relay_noop_shutdown(void);

/* ── RelayHost (RelayRuntime | NoopRelayRuntime) ──────────────────────── */

/* NULL when the host is the reduced-capability variant — the C spelling of
 * Python's `isinstance(host, RelayRuntime)` test. */
relay_runtime_t *relay_host_runtime(relay_host_t *host);
bool             relay_host_available(const relay_host_t *host);
const char      *relay_host_profile_key(const relay_host_t *host);
const char      *relay_host_reason(const relay_host_t *host);
void             relay_host_shutdown(relay_host_t *host);

/* ── RelayHostRegistry ────────────────────────────────────────────────── */

/* PoP: relay_host_registry_new @ agent/relay_runtime.py:__init__ */
relay_host_registry_t *relay_host_registry_new(void);
void                   relay_host_registry_free(relay_host_registry_t *reg);

/* PoP: relay_host_registry_for_profile @ agent/relay_runtime.py:for_profile */
relay_host_t *relay_host_registry_for_profile(relay_host_registry_t *reg,
                                              const char *profile_key, bool create);
/* PoP: relay_host_registry_shutdown_profile @ agent/relay_runtime.py:shutdown_profile */
void relay_host_registry_shutdown_profile(relay_host_registry_t *reg, const char *profile_key);
/* PoP: relay_host_registry_shutdown_all @ agent/relay_runtime.py:shutdown_all */
void relay_host_registry_shutdown_all(relay_host_registry_t *reg);

/* The module-level HOST_REGISTRY singleton. */
relay_host_registry_t *relay_host_registry_global(void);

/* ── ConversationLease / RelayTurnContext accessors ───────────────────── */

const char      *relay_lease_profile_key(const relay_lease_t *lease);
const char      *relay_lease_session_id(const relay_lease_t *lease);
const char      *relay_lease_platform(const relay_lease_t *lease);
const char      *relay_lease_parent_session_id(const relay_lease_t *lease);
relay_host_t    *relay_lease_host(relay_lease_t *lease);
relay_session_t *relay_lease_session(relay_lease_t *lease);
bool             relay_lease_released(const relay_lease_t *lease);

const char     *relay_turn_id(const relay_turn_t *turn);
const char     *relay_turn_task_id(const relay_turn_t *turn);
relay_handle_t  relay_turn_handle(const relay_turn_t *turn);
bool            relay_turn_closed(const relay_turn_t *turn);
relay_lease_t  *relay_turn_lease(relay_turn_t *turn);

/* Logical LLM child scopes tracked on a turn (turn.logical_llm_calls). */
bool relay_turn_add_logical_call(relay_turn_t *turn, const char *request_id,
                                 relay_handle_t handle);
relay_handle_t relay_turn_get_logical_call(const relay_turn_t *turn,
                                           const char *request_id);
void relay_turn_remove_logical_call(relay_turn_t *turn, const char *request_id);
size_t relay_turn_logical_call_count(relay_turn_t *turn);

/* ── RelaySessionCoordinator ──────────────────────────────────────────── */

typedef void (*relay_session_initializer_fn)(relay_runtime_t *host,
                                             const char *profile_key,
                                             const char *session_id,
                                             const char *platform,
                                             const char *parent_session_id,
                                             const char *model,
                                             void *user);

/* PoP: relay_coordinator_new @ agent/relay_runtime.py:__init__ */
relay_coordinator_t *relay_coordinator_new(relay_host_registry_t *registry);
void                 relay_coordinator_free(relay_coordinator_t *co);

/* PoP: relay_coordinator_register_session_initializer @ agent/relay_runtime.py:register_session_initializer */
bool relay_coordinator_register_session_initializer(relay_coordinator_t *co,
                                                    const char *name,
                                                    relay_session_initializer_fn cb,
                                                    void *user);
/* PoP: relay_coordinator_unregister_session_initializer @ agent/relay_runtime.py:unregister_session_initializer */
void relay_coordinator_unregister_session_initializer(relay_coordinator_t *co,
                                                      const char *name);

/* PoP: relay_coordinator_acquire_conversation @ agent/relay_runtime.py:acquire_conversation */
relay_lease_t *relay_coordinator_acquire_conversation(relay_coordinator_t *co,
                                                      const char *profile_key,
                                                      const char *session_id,
                                                      const char *platform,
                                                      const char *parent_session_id,
                                                      const char *model);
/* PoP: relay_coordinator_begin_turn @ agent/relay_runtime.py:begin_turn */
relay_turn_t *relay_coordinator_begin_turn(relay_coordinator_t *co, relay_lease_t *lease,
                                           const char *turn_id, const char *task_id);
/* PoP: relay_coordinator_end_turn @ agent/relay_runtime.py:end_turn */
void relay_coordinator_end_turn(relay_coordinator_t *co, relay_turn_t *turn,
                                const char *outcome);
/* PoP: relay_coordinator_has_active_turn @ agent/relay_runtime.py:has_active_turn */
bool relay_coordinator_has_active_turn(relay_coordinator_t *co,
                                       const char *profile_key, const char *session_id);
/* PoP: relay_coordinator_finish_logical_calls @ agent/relay_runtime.py:finish_logical_calls */
void relay_coordinator_finish_logical_calls(relay_coordinator_t *co, relay_turn_t *turn,
                                            const char *outcome);
/* PoP: relay_coordinator_release_conversation @ agent/relay_runtime.py:release_conversation */
void relay_coordinator_release_conversation(relay_lease_t *lease);
/* PoP: relay_coordinator_finalize_conversation @ agent/relay_runtime.py:finalize_conversation */
void relay_coordinator_finalize_conversation(relay_coordinator_t *co,
                                             const char *profile_key, const char *session_id);
/* PoP: relay_coordinator_shutdown_profile @ agent/relay_runtime.py:shutdown_profile */
void relay_coordinator_shutdown_profile(relay_coordinator_t *co, const char *profile_key);

/* Free a lease / turn once the caller is done (Python leaves this to the GC;
 * end_turn deliberately does NOT free the turn it was handed). */
void relay_lease_free(relay_lease_t *lease);
void relay_turn_free(relay_turn_t *turn);

/* The module-level SESSION_COORDINATOR singleton. */
relay_coordinator_t *relay_coordinator_global(void);

/* ── Module-level functions ───────────────────────────────────────────── */

/* PoP: relay_current_turn @ agent/relay_runtime.py:current_turn */
relay_turn_t *relay_current_turn(void);
/* PoP: relay_active_turn @ agent/relay_runtime.py:active_turn */
relay_turn_t *relay_active_turn(const char *session_id);

/* PoP: relay_resolve_execution_context @ agent/relay_runtime.py:resolve_execution_context */
bool relay_resolve_execution_context(const char *session_id,
                                     relay_runtime_t **out_runtime,
                                     relay_session_t **out_session,
                                     relay_handle_t *out_parent);

/* PoP: relay_emit_mark @ agent/relay_runtime.py:emit_mark */
bool relay_emit_mark(const char *name, const char *session_id,
                     const char *data_json, const char *metadata_json);
/* PoP: relay_apply_tool_request_intercepts @ agent/relay_runtime.py:apply_tool_request_intercepts */
char *relay_apply_tool_request_intercepts(const char *session_id, const char *tool_name,
                                          const char *args_json);
/* PoP: relay_ensure_session @ agent/relay_runtime.py:ensure_session */
relay_session_t *relay_ensure_session(const char *session_id);
/* PoP: relay_run_in_session @ agent/relay_runtime.py:run_in_session */
bool relay_run_in_session(const char *session_id, relay_session_cb callback,
                          void *user, void **out_result);
/* PoP: relay_run_in_session_async @ agent/relay_runtime.py:run_in_session_async */
bool relay_run_in_session_async(const char *session_id, relay_session_cb callback,
                                void *user, void **out_result);
/* PoP: relay_get_session_handle @ agent/relay_runtime.py:get_session_handle */
relay_handle_t relay_get_session_handle(const char *session_id);

/* Relay wraps a callback error as `RuntimeError("internal error: <Type>: <msg>")`.
 * The C port matches on that exact prefix so genuine policy errors are not
 * masked. `relay_error_kind` is the C stand-in for the exception class name.
 * PoP: relay_is_relay_wrapped_callback_error @ agent/relay_runtime.py:_is_relay_wrapped_callback_error */
bool relay_is_relay_wrapped_callback_error(const char *relay_error_kind,
                                           const char *relay_error_message,
                                           const char *callback_error_type,
                                           const char *callback_error_message);

/* PoP: relay_get_runtime @ agent/relay_runtime.py:get_runtime */
relay_runtime_t *relay_get_runtime(bool create, const char *profile_key);
/* PoP: relay_get_host @ agent/relay_runtime.py:get_host */
relay_host_t    *relay_get_host(bool create, const char *profile_key);

/* Canonical profile identity (resolved HERMES_HOME). Caller must NOT free —
 * the result is cached, mirroring Python's _PROFILE_KEY_CACHE.
 * PoP: relay_current_profile_key @ agent/relay_runtime.py:current_profile_key */
const char *relay_current_profile_key(void);

/* PoP: relay_session_id_of_event @ agent/relay_runtime.py:_session_id */
const char *relay_session_id_of_event(const char *session_id_or_null);

/* PoP: relay_reset_for_tests @ agent/relay_runtime.py:_reset_for_tests */
void relay_reset_for_tests(void);

/* Process-exit hook: closes every profile host (Python's atexit registration
 * inside RelayRuntime.__init__). */
void relay_runtime_shutdown_all(void);

#endif /* PORT_AGENT_RELAY_RUNTIME_H */
