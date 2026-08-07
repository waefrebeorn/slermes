/*
 * port_agent_relay_llm.c — C11 port of agent/relay_llm.py.
 *
 * The relay-managed LLM execution surface: execute / stream_current /
 * ManagedLlmStream / AnthropicStreamAccumulator plus the pure request-shaping
 * helpers. This is the cohesive port of ONE Python module — the correct
 * boundary; splitting it would fragment a single upstream concern.
 *
 * Python -> C mapping (per port_agent_relay_runtime.h):
 *   SimpleNamespace   -> relay_llm_namespace_t (shallow key/value mirror;
 *                        nested values stay json_t*, which C callers navigate
 *                        with json_obj_get).
 *   asyncio           -> relay_runtime_run_in_session_async (worker thread +
 *                        join); _run_awaitable is identity (C has no
 *                        awaitables).
 *   codec classes     -> codec name strings; the C port has no codec library,
 *                        so a codec round-trip baseline degrades to NULL
 *                        exactly like Python's codec-failure path.
 */

#define _GNU_SOURCE

#include "port_agent_relay_llm.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hermes_logger.h"

/* ── module constants ─────────────────────────────────────────────────── */

#define RELAY_PROVIDER_EXTENSION_KEYS_N 2
static const char *const RELAY_PROVIDER_EXTENSION_KEYS[RELAY_PROVIDER_EXTENSION_KEYS_N] = {
    "reasoning_content", "reasoning_details"
};

#define RELAY_INTERNAL_PROVIDER_HEADERS_N 2
static const char *const RELAY_INTERNAL_PROVIDER_HEADERS[RELAY_INTERNAL_PROVIDER_HEADERS_N] = {
    "x-dynamo-parent-session-id", "x-dynamo-session-id"
};

/* codec name table indexed by api_mode string comparison (mirrors Python's
 * api_mode -> codec mapping). */
static const char *const CODE_CODECS[] = {
    "OpenAIChatCodec", "AnthropicMessagesCodec", "OpenAIResponsesCodec"
};
static const char *const CODE_MODES[] = {
    "chat_completions", "anthropic_messages", "codex_responses"
};
#define CODE_MODES_N 3

/* ── small helpers ────────────────────────────────────────────────────── */

static char *rl_strdup(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _jsonable ────────────────────────────────────────────────────────── */

/* PoP: _jsonable @ agent/relay_llm.py:_jsonable */
json_t *relay_llm_jsonable(const void *value)
{
    if (!value) return json_null();
    const json_t *v = (const json_t *)value;
    return json_copy(v);
}

/* ── _namespace ───────────────────────────────────────────────────────── */

/* PoP: _namespace @ agent/relay_llm.py:_namespace */
relay_llm_namespace_t *relay_llm_namespace(const json_t *value)
{
    if (!value || value->type != JSON_OBJECT) return NULL;
    size_t n = value->c.count;
    relay_llm_namespace_t *ns = (relay_llm_namespace_t *)calloc(1, sizeof(*ns));
    if (!ns) return NULL;
    ns->keys = (char **)calloc(n ? n : 1, sizeof(char *));
    ns->vals = (void **)calloc(n ? n : 1, sizeof(void *));
    if (!ns->keys || !ns->vals) {
        free(ns->keys); free(ns->vals); free(ns);
        return NULL;
    }
    ns->count = n;
    for (size_t i = 0; i < n; i++) {
        ns->keys[i] = rl_strdup(value->c.keys[i]);
        ns->vals[i] = json_copy(value->c.items[i]);
    }
    return ns;
}

void relay_llm_namespace_free(relay_llm_namespace_t *ns)
{
    if (!ns) return;
    for (size_t i = 0; i < ns->count; i++) {
        free(ns->keys[i]);
        json_free((json_t *)ns->vals[i]);
    }
    free(ns->keys);
    free(ns->vals);
    free(ns);
}

const void *relay_llm_namespace_get(const relay_llm_namespace_t *ns, const char *key)
{
    if (!ns || !key) return NULL;
    for (size_t i = 0; i < ns->count; i++)
        if (strcmp(ns->keys[i], key) == 0)
            return ns->vals[i];
    return NULL;
}

const char *relay_llm_namespace_get_str(const relay_llm_namespace_t *ns, const char *key, const char *def)
{
    const json_t *v = (const json_t *)relay_llm_namespace_get(ns, key);
    if (v && v->type == JSON_STRING) return v->str_val;
    return def;
}

/* ── _json_equal ──────────────────────────────────────────────────────── */

/* Recursive equality matching Python's json.dumps(sort_keys=True) comparison:
 * object key order is irrelevant, array order matters, scalars compare
 * directly. */
static bool rl_json_equal_rec(const json_t *a, const json_t *b)
{
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
    case JSON_NULL: return true;
    case JSON_BOOL: return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!rl_json_equal_rec(a->c.items[i], b->c.items[i]))
                return false;
        return true;
    case JSON_OBJECT: {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const char *key = a->c.keys[i];
            json_t *bv = json_obj_get(b, key);
            if (!bv || !rl_json_equal_rec(a->c.items[i], bv))
                return false;
        }
        return true;
    }
    default: return false;
    }
}

/* PoP: _json_equal @ agent/relay_llm.py:_json_equal */
bool relay_llm_json_equal(const void *left, const void *right)
{
    return rl_json_equal_rec((const json_t *)left, (const json_t *)right);
}

/* ── _is_cancellation ─────────────────────────────────────────────────── */

/* PoP: _is_cancellation @ agent/relay_llm.py:_is_cancellation */
bool relay_llm_is_cancellation(const char *error_kind)
{
    if (!error_kind) return false;
    return strcmp(error_kind, "CancelledError") == 0 ||
           strcmp(error_kind, "InterruptedError") == 0 ||
           strcmp(error_kind, "KeyboardInterrupt") == 0;
}

/* PoP: _has_running_event_loop @ agent/relay_llm.py:_has_running_event_loop */
bool relay_llm_has_running_event_loop(void)
{
    /* The C analogue of a thread with a running asyncio loop is a thread
     * inside an async session callback (the runtime's worker thread). */
    return relay_runtime_in_async_worker();
}

/* PoP: _run_awaitable @ agent/relay_llm.py:_run_awaitable */
void *relay_llm_run_awaitable(void *value)
{
    /* C has no awaitables: the runtime's run_in_session_async already runs
     * the operation on a worker thread and joins, which IS the awaited
     * result. */
    return value;
}

/* ── _codec ───────────────────────────────────────────────────────────── */

/* PoP: _codec @ agent/relay_llm.py:_codec */
char *relay_llm_codec(const json_t *metadata)
{
    /* Python: codecs = getattr(relay, "codecs", None); None -> None. The C
     * analogue of a missing codec registry is an absent backend. */
    if (!relay_runtime_backend_available()) return NULL;
    const char *api_mode = metadata ? json_get_str(metadata, "api_mode", "") : "";
    for (int i = 0; i < CODE_MODES_N; i++)
        if (strcmp(api_mode, CODE_MODES[i]) == 0)
            return rl_strdup(CODE_CODECS[i]);
    return NULL;
}

/* ── _provider_request_body ───────────────────────────────────────────── */

/* PoP: _provider_request_body @ agent/relay_llm.py:_provider_request_body */
json_t *relay_llm_provider_request_body(const json_t *content, const json_t *metadata)
{
    json_t *body = json_copy(content);
    if (!body || body->type != JSON_OBJECT) return body;
    const char *api_mode = metadata ? json_get_str(metadata, "api_mode", "") : "";
    if (strcmp(api_mode, "codex_responses") != 0) return body;

    json_t *tools = json_obj_get(body, "tools");
    if (!tools || tools->type != JSON_ARRAY) return body;

    json_t *out = json_array();
    if (!out) return body;
    size_t n = tools->c.count;
    for (size_t i = 0; i < n; i++) {
        json_t *tool = tools->c.items[i];
        if (tool && tool->type == JSON_OBJECT &&
            json_get_str(tool, "type", "") && strcmp(json_get_str(tool, "type", ""), "function") == 0) {
            json_t *fn = json_obj_get(tool, "function");
            if (fn && fn->type == JSON_OBJECT) {
                /* {"type": "function", **dict(tool["function"])} */
                json_t *flat = json_object();
                json_set(flat, "type", json_string("function"));
                for (size_t k = 0; k < fn->c.count; k++)
                    json_set(flat, fn->c.keys[k], json_copy(fn->c.items[k]));
                json_append(out, flat);
                continue;
            }
        }
        json_append(out, json_copy(tool));
    }
    json_set(body, "tools", out);
    return body;
}

/* ── _relay_request_body ──────────────────────────────────────────────── */

/* PoP: _relay_request_body @ agent/relay_llm.py:_relay_request_body */
json_t *relay_llm_relay_request_body(const json_t *request, const json_t *metadata)
{
    json_t *body = json_copy(request);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return json_object();
    }
    const char *api_mode = metadata ? json_get_str(metadata, "api_mode", "") : "";
    if (strcmp(api_mode, "codex_responses") == 0) {
        json_t *tools = json_obj_get(body, "tools");
        if (tools && tools->type == JSON_NULL) {
            /* tools=None -> pop (the Responses codec expects absent, not null). */
            json_obj_del(body, "tools");
        } else if (tools && tools->type == JSON_ARRAY) {
            json_t *out = json_array();
            size_t n = tools->c.count;
            for (size_t i = 0; i < n; i++) {
                json_t *tool = tools->c.items[i];
                if (tool && tool->type == JSON_OBJECT &&
                    strcmp(json_get_str(tool, "type", ""), "function") == 0 &&
                    !json_obj_get(tool, "function")) {
                    /* {"type":"function","function":{k:v for k != "type"}} */
                    json_t *fn = json_object();
                    for (size_t k = 0; k < tool->c.count; k++) {
                        if (strcmp(tool->c.keys[k], "type") == 0) continue;
                        json_set(fn, tool->c.keys[k], json_copy(tool->c.items[k]));
                    }
                    json_t *wrapped = json_object();
                    json_set(wrapped, "type", json_string("function"));
                    json_set(wrapped, "function", fn);
                    json_append(out, wrapped);
                } else {
                    json_append(out, json_copy(tool));
                }
            }
            json_set(body, "tools", out);
        }
    } else if (strcmp(api_mode, "chat_completions") == 0) {
        json_t *tools = json_obj_get(body, "tools");
        if (tools && tools->type == JSON_ARRAY) {
            json_t *out = json_array();
            size_t n = tools->c.count;
            for (size_t i = 0; i < n; i++) {
                json_t *tool = tools->c.items[i];
                if (tool && tool->type == JSON_OBJECT &&
                    json_obj_get(tool, "function") &&
                    !json_obj_get(tool, "type")) {
                    /* {"type": "function", **tool} */
                    json_t *flat = json_object();
                    json_set(flat, "type", json_string("function"));
                    for (size_t k = 0; k < tool->c.count; k++)
                        json_set(flat, tool->c.keys[k], json_copy(tool->c.items[k]));
                    json_append(out, flat);
                } else {
                    json_append(out, json_copy(tool));
                }
            }
            json_set(body, "tools", out);
        }
    }
    return body;
}

/* ── _restore_provider_message_extensions ─────────────────────────────── */

/* PoP: _restore_provider_message_extensions @ agent/relay_llm.py:_restore_provider_message_extensions */
json_t *relay_llm_restore_provider_message_extensions(const json_t *original,
                                                      json_t *final,
                                                      const json_t *baseline,
                                                      const json_t *intercepted)
{
    if (!original || !final || !baseline || !intercepted) return final;
    json_t *om = json_obj_get(original, "messages");
    json_t *fm = json_obj_get(final, "messages");
    json_t *bm = json_obj_get(baseline, "messages");
    json_t *im = json_obj_get(intercepted, "messages");
    if (!om || !fm || !bm || !im) return final;
    if (om->type != JSON_ARRAY || fm->type != JSON_ARRAY ||
        bm->type != JSON_ARRAY || im->type != JSON_ARRAY)
        return final;
    size_t n = om->c.count;
    if (fm->c.count != n || bm->c.count != n || im->c.count != n)
        return final;
    for (size_t i = 0; i < n; i++) {
        json_t *o = om->c.items[i];
        json_t *f = fm->c.items[i];
        json_t *b = bm->c.items[i];
        json_t *ic = im->c.items[i];
        if (!o || !f || !b || !ic) continue;
        if (o->type != JSON_OBJECT || f->type != JSON_OBJECT ||
            b->type != JSON_OBJECT || ic->type != JSON_OBJECT)
            continue;
        for (int k = 0; k < RELAY_PROVIDER_EXTENSION_KEYS_N; k++) {
            const char *key = RELAY_PROVIDER_EXTENSION_KEYS[k];
            if (json_obj_get(o, key) &&
                !json_obj_get(b, key) &&
                !json_obj_get(ic, key) &&
                !json_obj_get(f, key)) {
                json_set(f, key, json_copy(json_obj_get(o, key)));
            }
        }
    }
    return final;
}

/* ── _provider_request ────────────────────────────────────────────────── */

/* PoP: _provider_request @ agent/relay_llm.py:_provider_request */
json_t *relay_llm_provider_request(const json_t *original,
                                   const json_t *next_request,
                                   const json_t *relay_request_body,
                                   const json_t *codec_baseline_body,
                                   const json_t *metadata)
{
    /* content = getattr(request, "content", request); C requests are json
     * objects: when the object carries a "content" member (the relay LLM
     * request shape), that member is the content; otherwise the request IS
     * its content. A non-dict content falls back to the relay body, mirroring
     * the isinstance check. */
    const json_t *content = NULL;
    if (next_request && next_request->type == JSON_OBJECT) {
        json_t *c = json_obj_get(next_request, "content");
        content = (c && c->type == JSON_OBJECT) ? c : next_request;
    } else {
        content = relay_request_body;
    }
    if (!content || content->type != JSON_OBJECT)
        content = relay_request_body;

    json_t *final = json_copy(original);
    if (!final) return NULL;

    if (!codec_baseline_body ||
        relay_llm_json_equal(content, relay_request_body)) {
        /* Plain copy path. */
    } else {
        const json_t *baseline = codec_baseline_body;
        json_t *intercepted = relay_llm_provider_request_body(content, metadata);
        if (intercepted) {
            /* Union of baseline and intercepted keys; overlay only changed
             * values (typed codecs may not represent provider fields). */
            for (size_t i = 0; i < baseline->c.count; i++) {
                const char *key = baseline->c.keys[i];
                if (!json_obj_get(intercepted, key))
                    json_obj_del(final, key);
            }
            for (size_t i = 0; i < intercepted->c.count; i++) {
                const char *key = intercepted->c.keys[i];
                json_t *bv = json_obj_get(baseline, key);
                json_t *iv = intercepted->c.items[i];
                if (!bv || !relay_llm_json_equal(iv, bv))
                    json_set(final, key, json_copy(iv));
            }
            relay_llm_restore_provider_message_extensions(original, final,
                                                          baseline, intercepted);
            json_free(intercepted);
        }
    }

    /* headers = getattr(request, "headers", None); C requests may carry a
     * "headers" member; internal relay headers are dropped, the rest merge
     * into extra_headers. */
    json_t *headers = NULL;
    if (next_request && next_request->type == JSON_OBJECT)
        headers = json_obj_get(next_request, "headers");
    if (headers && headers->type == JSON_OBJECT) {
        json_t *filtered = json_object();
        for (size_t i = 0; i < headers->c.count; i++) {
            const char *key = headers->c.keys[i];
            char lower[256];
            size_t n = strlen(key);
            if (n >= sizeof lower) n = sizeof lower - 1;
            for (size_t j = 0; j < n; j++)
                lower[j] = (char)tolower((unsigned char)key[j]);
            lower[n] = '\0';
            bool internal = false;
            for (int j = 0; j < RELAY_INTERNAL_PROVIDER_HEADERS_N; j++)
                if (strcmp(lower, RELAY_INTERNAL_PROVIDER_HEADERS[j]) == 0) { internal = true; break; }
            if (internal) continue;
            json_set(filtered, key, json_copy(headers->c.items[i]));
        }
        if (filtered->c.count > 0) {
            json_t *extra = json_obj_get(final, "extra_headers");
            json_t *merged = json_object();
            if (extra && extra->type == JSON_OBJECT)
                for (size_t i = 0; i < extra->c.count; i++)
                    json_set(merged, extra->c.keys[i], json_copy(extra->c.items[i]));
            for (size_t i = 0; i < filtered->c.count; i++)
                json_set(merged, filtered->c.keys[i], json_copy(filtered->c.items[i]));
            json_set(final, "extra_headers", merged);
        }
        json_free(filtered);
    }
    return final;
}

/* ── _codec_round_trip_request_body ───────────────────────────────────── */

/* PoP: _codec_round_trip_request_body @ agent/relay_llm.py:_codec_round_trip_request_body */
json_t *relay_llm_codec_round_trip_request_body(const json_t *backend_codec,
                                                const json_t *relay_request,
                                                const json_t *relay_request_body,
                                                const json_t *metadata)
{
    (void)backend_codec;
    (void)relay_request;
    /* Python: codec is None -> the body itself is the baseline. When a codec
     * exists, the decode/encode round trip yields the codec-only shape — or
     * NULL when the round trip fails. The C port has no codec library, so a
     * real codec name means the baseline cannot be computed: return NULL
     * (Python's codec-failure path), which makes _provider_request treat the
     * request as unchanged. */
    char *codec = relay_llm_codec(metadata);
    if (!codec) {
        json_t *baseline = relay_llm_provider_request_body(relay_request_body, metadata);
        return baseline;
    }
    free(codec);
    hermes_log(LOG_WARNING, "relay_llm",
               "NeMo Relay request codec baseline unavailable in the C port; ignoring request rewrites");
    return NULL;
}

/* ── _logical_parent ──────────────────────────────────────────────────── */

typedef struct {
    relay_backend_t  be;
    const char      *name;
    relay_scope_type_t type;
    relay_handle_t   parent;
    const char      *data_json;
    const char      *metadata_json;
    relay_handle_t   handle;
} rl_push_arg_t;

static void *rl_push_cb(void *p)
{
    rl_push_arg_t *a = (rl_push_arg_t *)p;
    a->handle = a->be.scope_push
                ? a->be.scope_push(a->be.ctx, a->name, a->type, a->parent,
                                   a->data_json, a->metadata_json)
                : NULL;
    return NULL;
}

/* PoP: _logical_parent @ agent/relay_llm.py:_logical_parent */
bool relay_llm_logical_parent(relay_runtime_t *runtime,
                              relay_session_t *session,
                              relay_handle_t parent,
                              const json_t *metadata,
                              relay_turn_t **out_turn,
                              relay_handle_t *out_handle,
                              char **out_request_id)
{
    if (!runtime || !session || !out_turn || !out_handle || !out_request_id)
        return false;
    *out_turn = NULL; *out_handle = NULL; *out_request_id = NULL;

    relay_turn_t *turn = relay_active_turn(relay_session_id(session));
    const char *request_id = metadata ? json_get_str(metadata, "api_request_id", "") : "";
    if (!turn || !request_id[0]) return false;

    /* turn.lease.host is not runtime -> None */
    relay_lease_t *lease = relay_turn_lease(turn);
    relay_host_t *host = lease ? relay_lease_host(lease) : NULL;
    if (!host || relay_host_runtime(host) != runtime) return false;

    if (relay_turn_closed(turn)) return false;

    relay_handle_t handle = relay_turn_get_logical_call(turn, request_id);
    if (!handle) {
        /* scope.push(LOGICAL_LLM_SCOPE, Function, handle=parent, input={},
         * metadata={schema, instance, hermes.call_role}) inside the session. */
        relay_backend_t be;
        if (!relay_runtime_backend_snapshot(&be) || !be.scope_push) return false;
        const char *call_role = metadata ? json_get_str(metadata, "call_role", "primary") : "primary";
        char meta[1024];
        snprintf(meta, sizeof meta,
                 "{\"%s\":\"%s\",\"%s\":\"%s\",\"hermes.call_role\":\"%s\"}",
                 RELAY_RUNTIME_SCHEMA_KEY, RELAY_RUNTIME_SCHEMA_VERSION,
                 RELAY_RUNTIME_INSTANCE_KEY, relay_runtime_id(runtime),
                 call_role ? call_role : "primary");
        rl_push_arg_t arg = { be, RELAY_LOGICAL_LLM_SCOPE, RELAY_SCOPE_FUNCTION,
                              parent, "{}", meta, NULL };
        void *res = NULL;
        if (!relay_runtime_run_in_session(runtime, session, rl_push_cb, &arg,
                                          false, &res))
            return false;
        handle = arg.handle;
        if (!handle) return false;
        relay_turn_add_logical_call(turn, request_id, handle);
    }
    *out_turn = turn;
    *out_handle = handle;
    *out_request_id = rl_strdup(request_id);
    return true;
}

/* ── _complete_logical ────────────────────────────────────────────────── */

typedef struct {
    relay_backend_t be;
    relay_handle_t  handle;
    const char     *output_json;
    const char     *metadata_json;
    bool            ok;
} rl_pop_arg_t;

static void *rl_pop_cb(void *p)
{
    rl_pop_arg_t *a = (rl_pop_arg_t *)p;
    a->ok = a->be.scope_pop
            ? a->be.scope_pop(a->be.ctx, a->handle, a->output_json, a->metadata_json)
            : false;
    return NULL;
}

/* PoP: _complete_logical @ agent/relay_llm.py:_complete_logical */
void relay_llm_complete_logical(relay_turn_t *turn,
                                relay_handle_t handle,
                                const char *request_id,
                                const char *outcome)
{
    if (!turn || !handle || !request_id) return;
    relay_lease_t *lease = relay_turn_lease(turn);
    relay_host_t *host = lease ? relay_lease_host(lease) : NULL;
    relay_runtime_t *rt = host ? relay_host_runtime(host) : NULL;
    /* Python: not isinstance(lease.host, RelayRuntime) -> return */
    if (!rt) return;
    /* Python: with logical_llm_lock: if logical_llm_calls.get(id) is not
     * handle -> return */
    if (relay_turn_get_logical_call(turn, request_id) != handle) return;

    relay_session_t *session = lease ? relay_lease_session(lease) : NULL;
    if (!session) return;

    char output[512];
    snprintf(output, sizeof output, "{\"outcome\":\"%s\"}",
             outcome && outcome[0] ? outcome : "success");
    char meta[1024];
    snprintf(meta, sizeof meta,
             "{\"%s\":\"%s\",\"%s\":\"%s\"}",
             RELAY_RUNTIME_SCHEMA_KEY, RELAY_RUNTIME_SCHEMA_VERSION,
             RELAY_RUNTIME_INSTANCE_KEY, relay_runtime_id(rt));

    relay_backend_t be;
    if (!relay_runtime_backend_snapshot(&be) || !be.scope_pop) return;
    rl_pop_arg_t arg = { be, handle, output, meta, false };
    void *res = NULL;
    if (!relay_runtime_run_in_session(rt, session, rl_pop_cb, &arg, false, &res) ||
        !arg.ok) {
        /* Python logs a warning and retains the handle for turn finalization;
         * the provider result is authoritative. */
        hermes_log(LOG_WARNING, "relay_llm",
                   "Hermes Relay logical LLM finalization failed; retaining handle");
        return;
    }
    if (relay_turn_get_logical_call(turn, request_id) == handle)
        relay_turn_remove_logical_call(turn, request_id);
}

/* ── _recover_successful_callback ─────────────────────────────────────── */

/* PoP: _recover_successful_callback @ agent/relay_llm.py:_recover_successful_callback */
bool relay_llm_recover_successful_callback(json_t **raw_response,
                                           const char *relay_error_kind,
                                           const char *relay_error_message,
                                           const char *callback_error_kind,
                                           const char *callback_error_message,
                                           relay_turn_t *logical_turn,
                                           relay_handle_t logical_handle,
                                           const char *logical_request_id,
                                           bool defer_logical_completion)
{
    (void)relay_error_message;
    (void)callback_error_message;
    if (!raw_response || !*raw_response) return false;
    if (!relay_error_kind) return false;          /* relay_error is None */
    if (callback_error_kind) return false;        /* callback_error is not None */
    if (!json_obj_get(*raw_response, "value")) return false;

    hermes_log(LOG_WARNING, "relay_llm",
               "NeMo Relay LLM post-processing failed after provider success; returning the provider response");
    if (!defer_logical_completion)
        relay_llm_complete_logical(logical_turn, logical_handle,
                                   logical_request_id, "success");
    return true;
}

/* ── complete_logical_call ────────────────────────────────────────────── */

/* PoP: complete_logical_call @ agent/relay_llm.py:complete_logical_call */
void relay_llm_complete_logical_call(const char *api_request_id, const char *outcome)
{
    if (!api_request_id || !api_request_id[0]) return;
    relay_turn_t *turn = relay_current_turn();
    if (!turn) return;
    relay_handle_t handle = relay_turn_get_logical_call(turn, api_request_id);
    if (handle)
        relay_llm_complete_logical(turn, handle, api_request_id, outcome);
}

/* ── execute ──────────────────────────────────────────────────────────── */

typedef struct {
    relay_llm_provider_callback callback;
    void                       *cb_user;
    json_t                     *original;
    json_t                     *relay_request_body;
    json_t                     *codec_baseline_body;
    json_t                     *metadata;
    json_t                     *raw_response;   /* {"value":..., "json":...} */
    const char                 *callback_error_kind;
} rl_invoke_arg_t;

/* The invoke callback the backend calls on the session thread. Returns a
 * malloc'd JSON string (the jsonable provider result). */
static void *rl_invoke_cb(void *p)
{
    rl_invoke_arg_t *a = (rl_invoke_arg_t *)p;
    /* Build the final provider request from the backend's next_request. The
     * C backend hands us the relay body (no typed codec), so content == the
     * relay body, mirroring Python when the codec baseline is unavailable. */
    json_t *final_request = relay_llm_provider_request(
        a->original, a->relay_request_body,
        a->relay_request_body, a->codec_baseline_body, a->metadata);
    json_t *raw = final_request ? a->callback(a->cb_user) : NULL;
    json_free(final_request);
    if (!raw) return NULL;
    json_t *jraw = relay_llm_jsonable(raw);
    /* raw_response["value"] = raw; raw_response["json"] = jraw */
    json_t *wr = a->raw_response ? json_obj_get(a->raw_response, "value") : NULL;
    if (wr) json_free(wr);
    json_set(a->raw_response, "value", json_copy(raw));
    json_set(a->raw_response, "json", json_copy(jraw));
    char *out = json_serialize(jraw);
    json_free(jraw);
    json_free(raw);
    return out;
}

typedef struct {
    relay_backend_t be;
    const char     *name;
    const char     *relay_request_json;
    relay_session_cb invoke_cb;
    void           *invoke_user;
    relay_handle_t  parent;
    const char     *metadata_json;
    const char     *model_name;
    const char     *codec;
    const char     *response_codec;
    char           *out_result;
    bool            ok;
} rl_exec_arg_t;

static void *rl_exec_cb(void *p)
{
    rl_exec_arg_t *a = (rl_exec_arg_t *)p;
    a->ok = a->be.llm_execute
            ? a->be.llm_execute(a->be.ctx, a->name, a->relay_request_json,
                                a->invoke_cb, a->invoke_user, a->parent,
                                a->metadata_json, a->model_name,
                                a->codec, a->response_codec, &a->out_result)
            : false;
    return NULL;
}

static json_t *rl_execute_impl(const json_t *request_json,
                               relay_llm_provider_callback callback, void *cb_user,
                               const char *session_id,
                               const char *name, const char *model_name,
                               const char *metadata_json,
                               bool defer_logical_completion)
{
    relay_runtime_t *runtime = NULL;
    relay_session_t *session = NULL;
    relay_handle_t   parent = NULL;
    if (!relay_resolve_execution_context(session_id, &runtime, &session, &parent) ||
        !runtime || !session || !relay_runtime_managed_execution_enabled(runtime)) {
        /* Python: runtime/session None or not managed -> callback(request) */
        if (callback) return callback(cb_user);
        return NULL;
    }

    json_t *metadata = metadata_json ? json_parse(metadata_json, NULL) : NULL;

    relay_turn_t *logical_turn = NULL;
    relay_handle_t logical_handle = NULL;
    char *logical_request_id = NULL;
    if (relay_llm_logical_parent(runtime, session, parent, metadata,
                                 &logical_turn, &logical_handle, &logical_request_id)) {
        parent = logical_handle;
    }

    json_t *relay_request_body = relay_llm_relay_request_body(request_json, metadata);
    json_t *codec_baseline = relay_llm_codec_round_trip_request_body(
        NULL, relay_request_body, relay_request_body, metadata);

    json_t *raw_response = json_object();
    rl_invoke_arg_t invoke_arg = {
        callback, cb_user,
        (json_t *)request_json, relay_request_body, codec_baseline, metadata,
        raw_response, NULL
    };

    relay_backend_t be;
    bool have_be = relay_runtime_backend_snapshot(&be);
    char *relay_request_json = json_serialize(relay_request_body);
    char *meta_str = json_serialize(relay_llm_jsonable(metadata ? metadata : json_null()));
    char *codec_name = relay_llm_codec(metadata);

    rl_exec_arg_t exec_arg = {
        be, name, relay_request_json, rl_invoke_cb, &invoke_arg,
        parent, meta_str, model_name, codec_name, codec_name, NULL, false
    };

    void *res = NULL;
    bool ran = have_be && be.llm_execute &&
               relay_runtime_run_in_session_async(runtime, session, rl_exec_cb,
                                                  &exec_arg, false, &res);
    if (!ran || !exec_arg.ok) {
        /* Managed-execution failure: distinguish callback errors from relay
         * post-processing failures. */
        const char *cb_kind = invoke_arg.callback_error_kind;
        if (cb_kind && relay_is_relay_wrapped_callback_error(
                "RuntimeError", exec_arg.out_result ? exec_arg.out_result : "",
                cb_kind, "callback failed")) {
            /* Python raises the callback error. C surfaces it as NULL with a
             * log; the caller's own callback owns the failure state. */
            hermes_log(LOG_WARNING, "relay_llm",
                       "Relay wrapped callback error: %s", cb_kind);
            free(exec_arg.out_result);
            free(relay_request_json); free(meta_str); free(codec_name);
            json_free(raw_response); json_free(relay_request_body);
            json_free(codec_baseline); json_free(metadata); free(logical_request_id);
            return NULL;
        }
        /* Recover a successful provider callback after relay post-processing
         * failure. */
        if (relay_llm_recover_successful_callback(
                &raw_response, "RuntimeError",
                exec_arg.out_result ? exec_arg.out_result : "",
                cb_kind, NULL,
                logical_turn, logical_handle, logical_request_id,
                defer_logical_completion)) {
            json_t *value = json_obj_get(raw_response, "value");
            json_t *out = value ? json_copy(value) : NULL;
            free(exec_arg.out_result);
            free(relay_request_json); free(meta_str); free(codec_name);
            json_free(raw_response); json_free(relay_request_body);
            json_free(codec_baseline); json_free(metadata); free(logical_request_id);
            return out;
        }
        free(exec_arg.out_result);
        free(relay_request_json); free(meta_str); free(codec_name);
        json_free(raw_response); json_free(relay_request_body);
        json_free(codec_baseline); json_free(metadata); free(logical_request_id);
        return NULL;
    }

    if (!defer_logical_completion && logical_handle)
        relay_llm_complete_logical(logical_turn, logical_handle,
                                   logical_request_id, "success");

    json_t *managed = exec_arg.out_result ? json_parse(exec_arg.out_result, NULL) : NULL;
    free(exec_arg.out_result);

    json_t *value = json_obj_get(raw_response, "value");
    json_t *jval = json_obj_get(raw_response, "json");
    json_t *out = NULL;
    if (value && jval && managed && relay_llm_json_equal(managed, jval)) {
        out = json_copy(value);   /* Python returns the raw provider value */
    } else if (managed) {
        out = json_copy(managed); /* Python returns _namespace(managed); C
                                     consumers read the json directly */
    }

    free(relay_request_json); free(meta_str); free(codec_name);
    json_free(raw_response); json_free(relay_request_body);
    json_free(codec_baseline); json_free(managed); json_free(metadata);
    free(logical_request_id);
    return out;
}

/* PoP: execute @ agent/relay_llm.py:execute */
json_t *relay_llm_execute(const json_t *request_json,
                          relay_llm_provider_callback callback, void *cb_user,
                          const char *session_id,
                          const char *name, const char *model_name,
                          const char *metadata_json,
                          bool defer_logical_completion)
{
    return rl_execute_impl(request_json, callback, cb_user, session_id,
                           name, model_name, metadata_json,
                           defer_logical_completion);
}

/* PoP: execute_async @ agent/relay_llm.py:execute_async */
json_t *relay_llm_execute_async(const json_t *request_json,
                                relay_llm_provider_callback callback, void *cb_user,
                                const char *session_id,
                                const char *name, const char *model_name,
                                const char *metadata_json,
                                bool defer_logical_completion)
{
    /* run_in_session_async already runs the callback on a worker thread and
     * joins — the faithful equivalent of awaiting it. */
    return rl_execute_impl(request_json, callback, cb_user, session_id,
                           name, model_name, metadata_json,
                           defer_logical_completion);
}

/* PoP: execute_current @ agent/relay_llm.py:execute_current */
json_t *relay_llm_execute_current(const json_t *request_json,
                                  relay_llm_provider_callback callback, void *cb_user,
                                  const char *name, const char *model_name,
                                  const char *metadata_json,
                                  bool defer_logical_completion)
{
    relay_turn_t *turn = relay_active_turn(NULL);
    if (!turn) {
        if (callback) return callback(cb_user);
        return NULL;
    }
    relay_lease_t *lease = relay_turn_lease(turn);
    const char *session_id = lease ? relay_lease_session_id(lease) : NULL;
    return rl_execute_impl(request_json, callback, cb_user, session_id,
                           name, model_name, metadata_json,
                           defer_logical_completion);
}

/* PoP: execute_current_async @ agent/relay_llm.py:execute_current_async */
json_t *relay_llm_execute_current_async(const json_t *request_json,
                                        relay_llm_provider_callback callback, void *cb_user,
                                        const char *name, const char *model_name,
                                        const char *metadata_json,
                                        bool defer_logical_completion)
{
    return relay_llm_execute_current(request_json, callback, cb_user,
                                     name, model_name, metadata_json,
                                     defer_logical_completion);
}

/* ── ManagedLlmStream ─────────────────────────────────────────────────── */

struct relay_llm_managed_stream {
    bool closed;
    bool defer_logical_completion;
    bool provider_completed;
    bool relay_observes_chunks;
    bool output_modified;

    json_t *final_response;      /* completed provider response, if any */

    /* logical call */
    relay_turn_t    *logical_turn;
    relay_handle_t   logical_handle;
    char            *logical_request_id;

    /* callbacks */
    void *(*stream_factory)(void *user);
    void  *sf_user;
    void (*finalizer)(void *user);
    void  *fin_user;
    void (*on_stream_created)(void *user, void *raw_stream);
    void  *osc_user;
    void (*on_chunk)(void *user, const json_t *chunk);
    void  *oc_user;
    void *(*chunk_adapter)(const json_t *chunk);
    bool (*accept_chunk)(const json_t *chunk);
    bool (*completed_response_predicate)(const json_t *raw_stream);

    /* request state */
    json_t *request;
    json_t *relay_request_body;
    json_t *codec_baseline_body;
    json_t *metadata;

    /* direct (non-managed) mode: the raw factory result iterated as chunks */
    json_t *raw_stream_resource;
    size_t  direct_index;

    /* pending provider chunks (encoded json_t*), Python's _raw_chunks */
    json_t **raw_chunks;
    size_t   raw_chunks_n;
    size_t   raw_chunks_cap;
    size_t   raw_chunks_pos;

    /* backend stream handle (managed mode) */
    relay_backend_t be;
    void           *backend_handle;
};

static void rl_raw_chunks_append(relay_llm_managed_stream_t *s, json_t *encoded)
{
    if (s->raw_chunks_n == s->raw_chunks_cap) {
        size_t cap = s->raw_chunks_cap ? s->raw_chunks_cap * 2 : 8;
        json_t **n = (json_t **)realloc(s->raw_chunks, cap * sizeof(json_t *));
        if (!n) { json_free(encoded); return; }
        s->raw_chunks = n;
        s->raw_chunks_cap = cap;
    }
    s->raw_chunks[s->raw_chunks_n++] = encoded;
}

/* PoP: __init__ @ agent/relay_llm.py:ManagedLlmStream.__init__ */
relay_llm_managed_stream_t *relay_llm_managed_stream_new(
    const json_t *request,
    void *(*stream_factory)(void *user), void *sf_user,
    const char *session_id, const char *name, const char *model_name,
    void (*finalizer)(void *user), void *fin_user,
    void (*on_stream_created)(void *user, void *raw_stream), void *osc_user,
    void (*on_chunk)(void *user, const json_t *chunk), void *oc_user,
                    void *(*chunk_adapter)(const json_t *chunk),
    bool (*accept_chunk)(const json_t *chunk),
    bool (*completed_response_predicate)(const json_t *raw_stream),
    const char *metadata_json,
    bool defer_logical_completion)
{
    (void)name; (void)model_name; (void)session_id;
    relay_llm_managed_stream_t *s = (relay_llm_managed_stream_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->stream_factory = stream_factory;
    s->sf_user = sf_user;
    s->finalizer = finalizer;
    s->fin_user = fin_user;
    s->on_stream_created = on_stream_created;
    s->osc_user = osc_user;
    s->on_chunk = on_chunk;
    s->oc_user = oc_user;
    s->chunk_adapter = chunk_adapter;
    s->accept_chunk = accept_chunk;
    s->completed_response_predicate = completed_response_predicate;
    s->defer_logical_completion = defer_logical_completion;
    s->request = json_copy(request);
    s->metadata = metadata_json ? json_parse(metadata_json, NULL) : NULL;
    s->raw_stream_resource = NULL;
    s->relay_observes_chunks = false;
    return s;
}

/* PoP: __iter__ @ agent/relay_llm.py:ManagedLlmStream.__iter__ */
relay_llm_managed_stream_t *relay_llm_managed_stream_iter(relay_llm_managed_stream_t *s)
{
    return s;
}

/* The provider-stream callback the backend drives on the session thread.
 * Runs the real stream factory against the provider request and captures the
 * raw stream / completed response on the managed stream. */
typedef struct {
    relay_llm_managed_stream_t *s;
    json_t *next_request;
} rl_provider_stream_arg_t;

static void *rl_provider_stream_cb(void *p)
{
    rl_provider_stream_arg_t *a = (rl_provider_stream_arg_t *)p;
    relay_llm_managed_stream_t *s = a->s;
    if (!s->stream_factory) return NULL;
    json_t *final_request = relay_llm_provider_request(
        s->request, a->next_request ? a->next_request : s->relay_request_body,
        s->relay_request_body, s->codec_baseline_body, s->metadata);
    void *raw = s->stream_factory(s->sf_user);
    json_free(final_request);
    if (!raw) return NULL;
    json_t *rawj = (json_t *)raw;
    if (s->completed_response_predicate && s->completed_response_predicate(rawj)) {
        s->final_response = json_copy(rawj);
        s->provider_completed = true;
        return NULL;
    }
    if (s->on_stream_created)
        s->on_stream_created(s->osc_user, raw);
    s->raw_stream_resource = json_copy(rawj);
    json_free(rawj);
    return NULL;
}

static void *rl_observe_chunk_cb(void *p)
{
    rl_provider_stream_arg_t *a = (rl_provider_stream_arg_t *)p;
    (void)a;
    return NULL;
}

static void *rl_finalizer_cb(void *p)
{
    rl_provider_stream_arg_t *a = (rl_provider_stream_arg_t *)p;
    relay_llm_managed_stream_t *s = a->s;
    if (s->final_response) return json_serialize(json_copy(s->final_response));
    if (s->finalizer) {
        s->finalizer(s->fin_user);
        return json_serialize(json_null());
    }
    return NULL;
}

/* Drive the backend stream: __next__ pulls one encoded chunk. */
static bool rl_stream_next_backend(relay_llm_managed_stream_t *s, json_t **out)
{
    if (s->closed) return false;
    if (!s->be.llm_stream_next) return false;
    char *chunk_json = NULL;
    if (!s->be.llm_stream_next(s->be.ctx, s->backend_handle, &chunk_json))
        return false;
    if (!chunk_json) return false;
    json_t *chunk = json_parse(chunk_json, NULL);
    free(chunk_json);
    if (!chunk) return false;
    *out = chunk;
    return true;
}

/* PoP: __next__ @ agent/relay_llm.py:ManagedLlmStream.__next__ */
bool relay_llm_managed_stream_next(relay_llm_managed_stream_t *s, json_t **out_chunk)
{
    if (!s || !out_chunk) return false;
    *out_chunk = NULL;
    if (s->closed) return false;

    if (s->final_response) {
        /* A completed response was captured: deliver it once, then end. */
        *out_chunk = json_copy(s->final_response);
        s->closed = true;
        return true;
    }

    json_t *chunk = NULL;
    bool from_backend = false;
    if (s->backend_handle) {
        from_backend = true;
        if (!rl_stream_next_backend(s, &chunk)) {
            /* End of managed stream: complete logical success, close. */
            if (!s->defer_logical_completion && s->logical_handle)
                relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                           s->logical_request_id, "success");
            relay_llm_managed_stream_shutdown(s, "cancelled");
            return false;
        }
    } else if (s->raw_stream_resource) {
        /* Direct mode: iterate the raw factory result as chunks. Python
         * iterates the provider's raw stream; the C raw result is a json
         * array of chunk objects (or a single response). */
        if (s->raw_stream_resource->type == JSON_ARRAY) {
            if (s->direct_index >= s->raw_stream_resource->c.count) {
                if (!s->defer_logical_completion && s->logical_handle)
                    relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                               s->logical_request_id, "success");
                relay_llm_managed_stream_shutdown(s, "cancelled");
                return false;
            }
            chunk = json_copy(s->raw_stream_resource->c.items[s->direct_index++]);
        } else {
            if (s->direct_index > 0) {
                if (!s->defer_logical_completion && s->logical_handle)
                    relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                               s->logical_request_id, "success");
                relay_llm_managed_stream_shutdown(s, "cancelled");
                return false;
            }
            s->direct_index++;
            chunk = json_copy(s->raw_stream_resource);
        }
    } else {
        /* Nothing to pull. */
        if (!s->defer_logical_completion && s->logical_handle)
            relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                       s->logical_request_id, "success");
        relay_llm_managed_stream_shutdown(s, "cancelled");
        return false;
    }

    /* Python: accept_chunk gate; relay-observed chunks skip the local hook. */
    if (s->accept_chunk && !s->accept_chunk(chunk)) {
        json_free(chunk);
        if (!s->defer_logical_completion && s->logical_handle)
            relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                       s->logical_request_id, "success");
        relay_llm_managed_stream_shutdown(s, "cancelled");
        return false;
    }

    /* Python matches the encoded chunk against _raw_chunks to return the raw
     * chunk object; in C the encoded json IS the chunk, and the match tracks
     * output_modified exactly like Python. */
    if (!s->relay_observes_chunks && s->on_chunk)
        s->on_chunk(s->oc_user, chunk);

    bool matched = false;
    for (size_t i = s->raw_chunks_pos; i < s->raw_chunks_n; i++) {
        if (relay_llm_json_equal(chunk, s->raw_chunks[i])) {
            if (i > s->raw_chunks_pos) s->output_modified = true;
            s->raw_chunks_pos = i + 1;
            matched = true;
            break;
        }
    }
    if (!matched)
        s->output_modified = true;

    if (from_backend && s->chunk_adapter && !matched) {
        json_t *adapted = (json_t *)s->chunk_adapter(chunk);
        json_free(chunk);
        if (adapted) { *out_chunk = adapted; return true; }
    }
    *out_chunk = chunk;
    return true;
}

/* PoP: _preserve_pending_provider_chunks @ agent/relay_llm.py:ManagedLlmStream._preserve_pending_provider_chunks */
void relay_llm_managed_stream_preserve_pending(relay_llm_managed_stream_t *s)
{
    if (!s) return;
    /* Switch the stream to its undelivered provider chunks. */
    json_t *pending = json_array();
    for (size_t i = s->raw_chunks_pos; i < s->raw_chunks_n; i++)
        json_append(pending, json_copy(s->raw_chunks[i]));
    for (size_t i = 0; i < s->raw_chunks_n; i++)
        json_free(s->raw_chunks[i]);
    free(s->raw_chunks);
    s->raw_chunks = NULL;
    s->raw_chunks_n = s->raw_chunks_cap = s->raw_chunks_pos = 0;

    if (s->backend_handle && s->be.llm_stream_close) {
        s->be.llm_stream_close(s->be.ctx, s->backend_handle);
        s->backend_handle = NULL;
    }
    json_free(s->raw_stream_resource);
    s->raw_stream_resource = pending;
    s->direct_index = 0;
    s->accept_chunk = NULL;
    if (!s->defer_logical_completion && s->logical_handle)
        relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                   s->logical_request_id, "success");
}

/* PoP: _close @ agent/relay_llm.py:ManagedLlmStream._close */
void relay_llm_managed_stream_shutdown(relay_llm_managed_stream_t *s,
                                       const char *logical_outcome)
{
    if (!s || s->closed) return;
    s->closed = true;
    if (s->backend_handle && s->be.llm_stream_close) {
        s->be.llm_stream_close(s->be.ctx, s->backend_handle);
        s->backend_handle = NULL;
    }
    json_free(s->raw_stream_resource);
    s->raw_stream_resource = NULL;
    if (!s->defer_logical_completion && s->logical_handle)
        relay_llm_complete_logical(s->logical_turn, s->logical_handle,
                                   s->logical_request_id, logical_outcome);
}

/* PoP: close @ agent/relay_llm.py:ManagedLlmStream.close */
void relay_llm_managed_stream_close(relay_llm_managed_stream_t *s)
{
    relay_llm_managed_stream_shutdown(s, "cancelled");
}

json_t *relay_llm_managed_stream_final_response(const relay_llm_managed_stream_t *s)
{
    return s ? s->final_response : NULL;
}

/* PoP: __del__ @ agent/relay_llm.py:ManagedLlmStream.__del__ */
void relay_llm_managed_stream_free(relay_llm_managed_stream_t *s)
{
    if (!s) return;
    relay_llm_managed_stream_shutdown(s, "cancelled");
    for (size_t i = 0; i < s->raw_chunks_n; i++)
        json_free(s->raw_chunks[i]);
    free(s->raw_chunks);
    json_free(s->final_response);
    json_free(s->request);
    json_free(s->relay_request_body);
    json_free(s->codec_baseline_body);
    json_free(s->metadata);
    free(s->logical_request_id);
    free(s);
}

/* PoP: stream @ agent/relay_llm.py:stream */

/* Argument bundle for the stream_execute backend hook, driven on the session
 * thread via run_in_session_async. */
typedef struct {
    relay_backend_t be;
    const char     *name;
    const char     *relay_request_json;
    relay_session_cb provider_stream_cb; void *provider_stream_user;
    relay_session_cb observe_chunk_cb;  void *observe_chunk_user;
    relay_session_cb finalizer_cb;      void *finalizer_user;
    relay_handle_t   parent;
    const char     *metadata_json;
    const char     *model_name;
    const char     *codec;
    const char     *response_codec;
    void           *handle;   /* out: stream handle */
} rl_stream_exec_arg_t;

typedef struct {
    rl_stream_exec_arg_t *arg;
    void *handle;
    bool ok;
} rl_stream_wrap_t;

static void *rl_stream_exec_trampoline(void *p)
{
    rl_stream_wrap_t *w = (rl_stream_wrap_t *)p;
    rl_stream_exec_arg_t *a = w->arg;
    if (a->be.llm_stream_execute) {
        w->ok = a->be.llm_stream_execute(a->be.ctx, a->name,
                                         a->relay_request_json,
                                         a->provider_stream_cb, a->provider_stream_user,
                                         a->observe_chunk_cb, a->observe_chunk_user,
                                         a->finalizer_cb, a->finalizer_user,
                                         a->parent, a->metadata_json,
                                         a->model_name, a->codec, a->response_codec,
                                         &w->handle);
    }
    return NULL;
}

relay_llm_managed_stream_t *relay_llm_stream(
    const json_t *request,
    void *(*stream_factory)(void *user), void *sf_user,
    const char *session_id, const char *name, const char *model_name,
    void (*finalizer)(void *user), void *fin_user,
    void (*on_stream_created)(void *user, void *raw_stream), void *osc_user,
    void (*on_chunk)(void *user, const json_t *chunk), void *oc_user,
                    void *(*chunk_adapter)(const json_t *chunk),
    bool (*accept_chunk)(const json_t *chunk),
    bool (*completed_response_predicate)(const json_t *raw_stream),
    const char *metadata_json,
    bool defer_logical_completion)
{
    relay_llm_managed_stream_t *s = relay_llm_managed_stream_new(
        request, stream_factory, sf_user, session_id, name, model_name,
        finalizer, fin_user, on_stream_created, osc_user,
        on_chunk, oc_user, chunk_adapter, accept_chunk,
        completed_response_predicate, metadata_json, defer_logical_completion);
    if (!s) return NULL;

    relay_runtime_t *runtime = NULL;
    relay_session_t *session = NULL;
    relay_handle_t   parent = NULL;
    if (!relay_resolve_execution_context(session_id, &runtime, &session, &parent) ||
        !runtime || !session || !relay_runtime_managed_execution_enabled(runtime)) {
        /* Non-managed path: run the factory directly; a completed response is
         * captured, else the raw result is iterated as chunks. */
        if (!s->stream_factory) return s;
        void *raw = s->stream_factory(s->sf_user);
        json_t *rawj = (json_t *)raw;
        if (!rawj) return s;
        if (s->completed_response_predicate && s->completed_response_predicate(rawj)) {
            s->final_response = json_copy(rawj);
            json_free(rawj);
        } else {
            if (s->on_stream_created)
                s->on_stream_created(s->osc_user, raw);
            s->raw_stream_resource = json_copy(rawj);
            json_free(rawj);
        }
        return s;
    }

    /* Managed path: create the logical parent, then drive the backend's
     * stream_execute hook on the session thread. */
    if (relay_llm_logical_parent(runtime, session, parent, s->metadata,
                                 &s->logical_turn, &s->logical_handle,
                                 &s->logical_request_id)) {
        parent = s->logical_handle;
    }
    s->relay_request_body = relay_llm_relay_request_body(s->request, s->metadata);
    s->codec_baseline_body = relay_llm_codec_round_trip_request_body(
        NULL, s->relay_request_body, s->relay_request_body, s->metadata);

    relay_backend_t be;
    if (!relay_runtime_backend_snapshot(&be) || !be.llm_stream_execute) {
        /* Backend without a stream hook degrades to the direct path. */
        if (!s->stream_factory) return s;
        void *raw = s->stream_factory(s->sf_user);
        json_t *rawj = (json_t *)raw;
        if (!rawj) return s;
        if (s->completed_response_predicate && s->completed_response_predicate(rawj)) {
            s->final_response = json_copy(rawj);
            json_free(rawj);
        } else {
            if (s->on_stream_created)
                s->on_stream_created(s->osc_user, raw);
            s->raw_stream_resource = json_copy(rawj);
            json_free(rawj);
        }
        return s;
    }
    s->be = be;

    char *relay_request_json = json_serialize(s->relay_request_body);
    char *meta_str = json_serialize(relay_llm_jsonable(s->metadata ? s->metadata : json_null()));
    char *codec_name = relay_llm_codec(s->metadata);

    rl_provider_stream_arg_t ps_arg = { s, s->relay_request_body };
    void *backend_handle = NULL;

    /* stream_execute(name, relay_request, provider_stream_cb, ...) inside the
     * session; the backend stores the handle we pull chunks from. */
    bool ok = false;
    void *res = NULL;
    if (be.llm_stream_execute) {
        rl_stream_exec_arg_t arg = {
            be, name, relay_request_json,
            rl_provider_stream_cb, &ps_arg,
            rl_observe_chunk_cb, &ps_arg,
            rl_finalizer_cb, &ps_arg,
            parent, meta_str, model_name, codec_name, codec_name, NULL
        };
        rl_stream_wrap_t wrap = { &arg, NULL, false };
        ok = relay_runtime_run_in_session_async(runtime, session,
                                                rl_stream_exec_trampoline, &wrap,
                                                false, &res);
        backend_handle = wrap.handle;
    }
    free(relay_request_json); free(meta_str); free(codec_name);

    if (!ok || !backend_handle) {
        /* Post-processing failure after provider success: preserve chunks. */
        if (s->provider_completed) {
            relay_llm_managed_stream_preserve_pending(s);
            return s;
        }
        relay_llm_managed_stream_shutdown(s, "failed");
        return s;
    }
    s->backend_handle = backend_handle;
    s->relay_observes_chunks = true;
    return s;
}

/* PoP: stream_current @ agent/relay_llm.py:stream_current */
relay_llm_managed_stream_t *relay_llm_stream_current(
    const json_t *request_json,
    void *(*stream_factory)(void *user), void *sf_user,
    const char *name, const char *model_name,
    void (*finalizer)(void *user), void *fin_user,
    bool (*completed_response_predicate)(const json_t *raw_stream),
    const char *metadata_json,
    bool defer_logical_completion)
{
    relay_turn_t *turn = relay_active_turn(NULL);
    if (!turn || relay_llm_has_running_event_loop()) {
        /* Python: no turn or a running event loop -> raw factory result. */
        relay_llm_managed_stream_t *s = relay_llm_managed_stream_new(
            request_json, stream_factory, sf_user, NULL, name, model_name,
            finalizer, fin_user, NULL, NULL, NULL, NULL, NULL, NULL,
            completed_response_predicate, metadata_json, defer_logical_completion);
        if (!s) return NULL;
        void *raw = s->stream_factory ? s->stream_factory(s->sf_user) : NULL;
        json_t *rawj = (json_t *)raw;
        if (rawj && completed_response_predicate && completed_response_predicate(rawj)) {
            s->final_response = json_copy(rawj);
            json_free(rawj);
        } else if (rawj) {
            s->raw_stream_resource = json_copy(rawj);
            json_free(rawj);
        }
        return s;
    }
    relay_lease_t *lease = relay_turn_lease(turn);
    const char *session_id = lease ? relay_lease_session_id(lease) : NULL;
    return relay_llm_stream(request_json, stream_factory, sf_user,
                            session_id, name, model_name,
                            finalizer, fin_user,
                            NULL, NULL, NULL, NULL, NULL, NULL,
                            completed_response_predicate,
                            metadata_json, defer_logical_completion);
}

/* ── AnthropicStreamAccumulator ───────────────────────────────────────── */

struct anthropic_stream_accumulator {
    json_t *message;   /* assembled message fields (id/type/role/model/usage/stop_reason) */
    json_t *blocks;    /* index-keyed block objects ("0", "1", ...) */
};

/* PoP: __init__ @ agent/relay_llm.py:AnthropicStreamAccumulator.__init__ */
anthropic_stream_accumulator_t *anthropic_stream_accumulator_new(void)
{
    anthropic_stream_accumulator_t *acc =
        (anthropic_stream_accumulator_t *)calloc(1, sizeof(*acc));
    if (!acc) return NULL;
    acc->message = json_object();
    acc->blocks = json_object();
    if (!acc->message || !acc->blocks) {
        json_free(acc->message); json_free(acc->blocks); free(acc);
        return NULL;
    }
    return acc;
}

void anthropic_stream_accumulator_free(anthropic_stream_accumulator_t *acc)
{
    if (!acc) return;
    json_free(acc->message);
    json_free(acc->blocks);
    free(acc);
}

static const char *MESSAGE_START_KEYS[] = { "id", "type", "role", "model", "usage" };
#define MESSAGE_START_KEYS_N 5
static const char *MESSAGE_DELTA_KEYS[] = { "stop_reason", "stop_sequence" };
#define MESSAGE_DELTA_KEYS_N 2

/* PoP: observe @ agent/relay_llm.py:AnthropicStreamAccumulator.observe */
void anthropic_stream_accumulator_observe(anthropic_stream_accumulator_t *acc,
                                          const json_t *event)
{
    if (!acc || !event) return;
    json_t *payload = relay_llm_jsonable(event);
    if (!payload || payload->type != JSON_OBJECT) { json_free(payload); return; }
    const char *event_type = json_get_str(payload, "type", "");

    if (strcmp(event_type, "message_start") == 0) {
        json_t *message = json_obj_get(payload, "message");
        if (message && message->type == JSON_OBJECT) {
            for (int i = 0; i < MESSAGE_START_KEYS_N; i++) {
                const char *key = MESSAGE_START_KEYS[i];
                json_t *v = json_obj_get(message, key);
                if (v) json_set(acc->message, key, json_copy(v));
            }
        }
    } else if (strcmp(event_type, "content_block_start") == 0) {
        json_t *index = json_obj_get(payload, "index");
        json_t *block = json_obj_get(payload, "content_block");
        if (index && index->type == JSON_NUMBER && block && block->type == JSON_OBJECT) {
            char key[32];
            snprintf(key, sizeof key, "%d", (int)index->num_val);
            json_set(acc->blocks, key, json_copy(block));
        }
    } else if (strcmp(event_type, "content_block_delta") == 0) {
        json_t *index = json_obj_get(payload, "index");
        json_t *delta = json_obj_get(payload, "delta");
        if (!(index && index->type == JSON_NUMBER && delta && delta->type == JSON_OBJECT)) {
            json_free(payload); return;
        }
        char key[32];
        snprintf(key, sizeof key, "%d", (int)index->num_val);
        json_t *block = json_obj_get(acc->blocks, key);
        if (!block) {
            block = json_object();
            json_set(acc->blocks, key, block);
        }
        const char *delta_type = json_get_str(delta, "type", "");
        if (strcmp(delta_type, "text_delta") == 0) {
            const char *cur = json_get_str(block, "text", "");
            const char *add = json_get_str(delta, "text", "");
            char *joined = NULL;
            if (asprintf(&joined, "%s%s", cur ? cur : "", add ? add : "") >= 0) {
                json_set(block, "text", json_string(joined));
                free(joined);
            }
        } else if (strcmp(delta_type, "thinking_delta") == 0) {
            const char *cur = json_get_str(block, "thinking", "");
            const char *add = json_get_str(delta, "thinking", "");
            char *joined = NULL;
            if (asprintf(&joined, "%s%s", cur ? cur : "", add ? add : "") >= 0) {
                json_set(block, "thinking", json_string(joined));
                free(joined);
            }
        } else if (strcmp(delta_type, "signature_delta") == 0) {
            const char *cur = json_get_str(block, "signature", "");
            const char *add = json_get_str(delta, "signature", "");
            char *joined = NULL;
            if (asprintf(&joined, "%s%s", cur ? cur : "", add ? add : "") >= 0) {
                json_set(block, "signature", json_string(joined));
                free(joined);
            }
        } else if (strcmp(delta_type, "input_json_delta") == 0) {
            const char *cur = json_get_str(block, "_partial_json", "");
            const char *add = json_get_str(delta, "partial_json", "");
            char *joined = NULL;
            if (asprintf(&joined, "%s%s", cur ? cur : "", add ? add : "") >= 0) {
                json_set(block, "_partial_json", json_string(joined));
                free(joined);
            }
        } else if (strcmp(delta_type, "citations_delta") == 0 &&
                   json_obj_get(delta, "citation")) {
            json_t *cites = json_obj_get(block, "citations");
            if (!cites || cites->type != JSON_ARRAY) {
                cites = json_array();
                json_set(block, "citations", cites);
            }
            json_append(cites, json_copy(json_obj_get(delta, "citation")));
        }
    } else if (strcmp(event_type, "message_delta") == 0) {
        json_t *delta = json_obj_get(payload, "delta");
        if (delta && delta->type == JSON_OBJECT) {
            for (int i = 0; i < MESSAGE_DELTA_KEYS_N; i++) {
                const char *key = MESSAGE_DELTA_KEYS[i];
                json_t *v = json_obj_get(delta, key);
                if (v) json_set(acc->message, key, json_copy(v));
            }
        }
        json_t *usage = json_obj_get(payload, "usage");
        if (usage) {
            json_t *current = json_obj_get(acc->message, "usage");
            if (current && current->type == JSON_OBJECT && usage->type == JSON_OBJECT) {
                json_t *merged = json_object();
                for (size_t i = 0; i < current->c.count; i++)
                    json_set(merged, current->c.keys[i], json_copy(current->c.items[i]));
                for (size_t i = 0; i < usage->c.count; i++)
                    json_set(merged, usage->c.keys[i], json_copy(usage->c.items[i]));
                json_set(acc->message, "usage", merged);
            } else {
                json_set(acc->message, "usage", json_copy(usage));
            }
        }
    }
    json_free(payload);
}

/* PoP: finalize @ agent/relay_llm.py:AnthropicStreamAccumulator.finalize */
json_t *anthropic_stream_accumulator_finalize(const anthropic_stream_accumulator_t *acc)
{
    if (!acc) return json_object();
    json_t *out = json_copy(acc->message);
    json_t *blocks = json_array();
    /* Sorted by numeric index. */
    size_t n = acc->blocks->c.count;
    size_t *order = (size_t *)malloc(n ? n : 1 * sizeof(size_t));
    if (order) {
        for (size_t i = 0; i < n; i++) order[i] = i;
        /* insertion-sort keys numerically */
        for (size_t i = 1; i < n; i++) {
            size_t j = i;
            while (j > 0) {
                long a = strtol(acc->blocks->c.keys[order[j - 1]], NULL, 10);
                long b = strtol(acc->blocks->c.keys[order[j]], NULL, 10);
                if (a <= b) break;
                size_t t = order[j - 1]; order[j - 1] = order[j]; order[j] = t;
                j--;
            }
        }
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_copy(acc->blocks->c.items[order[i]]);
            json_t *partial = json_obj_get(block, "_partial_json");
            if (partial && partial->type == JSON_STRING) {
                json_t *parsed = json_parse(partial->str_val, NULL);
                if (parsed)
                    json_set(block, "input", parsed);
                else
                    json_set(block, "input", json_copy(partial));
                json_obj_del(block, "_partial_json");
            }
            json_append(blocks, block);
        }
        free(order);
    }
    json_set(out, "content", blocks);
    return out;
}

/* PoP: response @ agent/relay_llm.py:AnthropicStreamAccumulator.response */
json_t *anthropic_stream_accumulator_response(const anthropic_stream_accumulator_t *acc,
                                              const json_t *base)
{
    if (!acc) return json_object();
    json_t *assembled = anthropic_stream_accumulator_finalize(acc);
    json_t *base_payload = base ? relay_llm_jsonable(base) : json_object();
    if (!base_payload || base_payload->type != JSON_OBJECT) {
        json_free(base_payload);
        base_payload = json_object();
    }
    json_t *content = json_obj_get(assembled, "content");
    if (content) json_obj_del(assembled, "content");
    json_t *merged = base_payload;
    for (size_t i = 0; i < assembled->c.count; i++)
        json_set(merged, assembled->c.keys[i], json_copy(assembled->c.items[i]));
    if ((content && content->c.count > 0) || !json_obj_get(merged, "content"))
        json_set(merged, "content", content ? json_copy(content) : json_array());
    json_free(assembled);
    return merged;
}
