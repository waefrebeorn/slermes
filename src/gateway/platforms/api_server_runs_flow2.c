/*
 * api_server_runs_flow2.c — FLOW port of a second UNCLAIMED remainder of
 * gateway/platforms/api_server.py (the aiohttp OpenAI-compatible API server
 * adapter). The first remainder lives in api_server_flow2.c; the run-lifecycle
 * FLOW lives in api_server_runs_flow.c. Neither claims the 14 defs below, so
 * each carries its own PoP annotation against the DEFINING module.
 *
 * Ports the DECISION SEQUENCES of 14 functions:
 *   request/helpers — listen_address, _content_has_visible_payload,
 *     _session_row, _normalize_chat_content, _room_grant_delegate,
 *     _run_route_delegate
 *   HTTP handlers  — _handle_skills, _handle_toolsets
 *   store/runtime  — _ensure_store, _store_beside, _current_response_store
 *   run auth/task  — _check_run_auth, _request_owns_run, _track_background_task
 *
 * Modeling contract (shared with tests/oracle_api_server_runs_flow2.py):
 *  - Self-contained: the harness #includes this .c; EVERY symbol is static and
 *    asr2_*-prefixed; only libjson is linked.
 *  - Universal seam: json_t *(*call)(void*, const char*, const json_t*). name
 *    identifies the live collaborator; args is a BORROWED object copied into
 *    the ordered trace; the result is a NEW json_t. NULL == None; a
 *    {"__raise__":kind,"message":m} object is a raised exception. The port owns
 *    args (asr2_call frees them) and the result.
 *  - Seam names -> live calls:
 *      getenv                     -> os.getenv(name, default)
 *      _coerce_port               -> gateway.platforms._shared.coerce_port
 *      getattr                    -> getattr(session/entry, key)
 *      py_str                     -> str(value) (non-string fallback)
 *      room_grant_call            -> delegated api_server_room_grants handler
 *      run_route_call             -> delegated api_server_runs route handler
 *      find_all_skills/sort_skills -> tools.skills_tool in-function imports
 *      web.json_response          -> aiohttp response
 *      _error_response            -> claimed module error envelope
 *      load_config/_get_platform_tools/get_nous_subscription_features/
 *      _get_effective_configurable_toolsets/resolve_toolset/_toolset_has_keys
 *                                 -> hermes_cli + toolsets import chain
 *      selected_session_db/get_hermes_home/_open_and_cache_session_db/
 *      adapter_persistence/ResponseStore -> store construction chain
 *      runs._check_run_auth/runs._request_owns_run -> _api_runs delegates
 *      bg.add/bg.add_done_callback -> background task set registration
 *  - PURE claimed leaves transcribed inline with NO PoP re-claim:
 *    _cap_text, _cap_list, _strip/_lower/_text_part membership helpers.
 *  - logger is a SILENT side effect on both sides (no trace event).
 *  - State mirror in["state"] (harness-owned copy): gateway_runner,
 *    _session_db, _response_store_home, _response_store, _response_stores,
 *    _background_tasks. JSON-array _background_tasks projects the Python set.
 *  - Exception kinds: `except Exception` / suppress(Exception) swallow the
 *    Exception family; SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError
 *    (BaseException family) PASS THROUGH. The full builtin hierarchy is the
 *    asr2_exc_parent table below; the oracle builds it from the live classes.
 *  - FAIL-CLOSED: a missing/exhausted seam script is returned by the harness as
 *    {"__raise__":"RuntimeError","message":"no script for X"} and traced; the
 *    port propagates it exactly like a scripted raise.
 *  - A propagated raise is emitted as the return value (a raise spec object),
 *    matching the oracle's live-exception -> spec mapping.
 *  - FIXTURE DOMAIN: integral JSON numbers read as ints on both sides; str()
 *    positions avoid non-integral floats; set state is projected sorted;
 *    non-string dict "type"/"text" values are excluded.
 *
 * PoP annotations (one per ported def) appear at each definition below.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "libjson/json.h"

#define ASR2_DEFAULT_HOST "127.0.0.1"
#define ASR2_DEFAULT_PORT 8642
#define ASR2_MAX_NORM_LEN 65536
#define ASR2_MAX_CONTENT_LIST 1000

static const char *ASR2_TEXT_PART_TYPES[] = {"text", "input_text", "output_text", NULL};
static const char *ASR2_IMAGE_PART_TYPES[] = {"image_url", "input_image", NULL};
static const char *ASR2_SESSION_KEYS[] = {
    "source", "user_id", "model", "title", "started_at", "ended_at", "end_reason",
    "message_count", "tool_call_count", "input_tokens", "output_tokens",
    "cache_read_tokens", "cache_write_tokens", "reasoning_tokens",
    "estimated_cost_usd", "actual_cost_usd", "api_call_count", "parent_session_id",
    "pinned", "archived", "hidden", "system_prompt", NULL};

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct asr2_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
} asr2_env;

/* ── JSON type / truth helpers ────────────────────────────────────────────── */
static bool asr2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool asr2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool asr2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool asr2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static const char *asr2_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *asr2_sval(const json_t *v) { const char *s = asr2_str(v); return s ? s : ""; }
static bool asr2_nonnull(const json_t *v) { return v && v->type != JSON_NULL; }
static bool asr2_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static void asr2_free(json_t *v) { if (v) json_free(v); }

/* ── raise specs + FULL builtin exception hierarchy ──────────────────────── */
static bool asr2_is_raise(const json_t *r) {
    return asr2_is_obj(r) && asr2_is_str(json_obj_get(r, "__raise__"));
}
static const char *asr2_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *asr2_msg(const json_t *r) { return json_get_str(r, "message", ""); }
static json_t *asr2_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
/* child -> parent; mirrors Python's builtin BaseException tree (plus the
 * asyncio.CancelledError leaf and the custom ProviderAuthResolutionError). */
static const struct { const char *c, *p; } ASR2_EXC_PARENT[] = {
    {"Exception","BaseException"},{"SystemExit","BaseException"},
    {"KeyboardInterrupt","BaseException"},{"GeneratorExit","BaseException"},
    {"CancelledError","BaseException"},{"BaseExceptionGroup","BaseException"},
    {"StopIteration","Exception"},{"StopAsyncIteration","Exception"},
    {"ArithmeticError","Exception"},{"FloatingPointError","ArithmeticError"},
    {"OverflowError","ArithmeticError"},{"ZeroDivisionError","ArithmeticError"},
    {"AssertionError","Exception"},{"AttributeError","Exception"},
    {"BufferError","Exception"},{"EOFError","Exception"},
    {"ImportError","Exception"},{"ModuleNotFoundError","ImportError"},
    {"LookupError","Exception"},{"IndexError","LookupError"},{"KeyError","LookupError"},
    {"MemoryError","Exception"},{"NameError","Exception"},{"UnboundLocalError","NameError"},
    {"OSError","Exception"},{"BlockingIOError","OSError"},{"ChildProcessError","OSError"},
    {"ConnectionError","OSError"},{"BrokenPipeError","ConnectionError"},
    {"ConnectionAbortedError","ConnectionError"},{"ConnectionRefusedError","ConnectionError"},
    {"ConnectionResetError","ConnectionError"},{"FileExistsError","OSError"},
    {"FileNotFoundError","OSError"},{"InterruptedError","OSError"},
    {"IsADirectoryError","OSError"},{"NotADirectoryError","OSError"},
    {"PermissionError","OSError"},{"ProcessLookupError","OSError"},{"TimeoutError","OSError"},
    {"ReferenceError","Exception"},{"RuntimeError","Exception"},
    {"NotImplementedError","RuntimeError"},{"RecursionError","RuntimeError"},
    {"_ProviderAuthResolutionError","RuntimeError"},
    {"SyntaxError","Exception"},{"IndentationError","SyntaxError"},{"TabError","IndentationError"},
    {"SystemError","Exception"},{"TypeError","Exception"},{"ValueError","Exception"},
    {"UnicodeError","ValueError"},{"UnicodeDecodeError","UnicodeError"},
    {"UnicodeEncodeError","UnicodeError"},{"UnicodeTranslateError","UnicodeError"},
    {"Warning","Exception"},{"UserWarning","Warning"},{"DeprecationWarning","Warning"},
    {"PendingDeprecationWarning","Warning"},{"SyntaxWarning","Warning"},
    {"RuntimeWarning","Warning"},{"FutureWarning","Warning"},{"ImportWarning","Warning"},
    {"UnicodeWarning","Warning"},{"BytesWarning","Warning"},{"ResourceWarning","Warning"},
    {"EncodingWarning","Warning"},{"ExceptionGroup","Exception"},
};
static const char *asr2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(ASR2_EXC_PARENT)/sizeof(ASR2_EXC_PARENT[0]); i++)
        if (strcmp(ASR2_EXC_PARENT[i].c, kind) == 0) return ASR2_EXC_PARENT[i].p;
    return NULL;
}
static bool asr2_exc_is(const char *kind, const char *ancestor) {
    const char *k = kind;
    while (k) { if (strcmp(k, ancestor) == 0) return true; k = asr2_exc_parent(k); }
    return false;
}
static bool asr2_is_exception(const char *kind) { return asr2_exc_is(kind, "Exception"); }
/* true when a raise must PROPAGATE out of an `except Exception` site
 * (BaseException family); false for an Exception-family raise (caller frees). */
static bool asr2_propagates(json_t *r) {
    return asr2_is_raise(r) && !asr2_is_exception(asr2_kind(r));
}

/* ── universal seam call (args owned/freed by the port) ───────────────────── */
static json_t *asr2_call(asr2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *asr2_seam0(asr2_env *e, const char *name) { return asr2_call(e, name, json_object()); }

/* ── state mirror helpers ─────────────────────────────────────────────────── */
static json_t *asr2_st(const asr2_env *e, const char *k) { return json_obj_get(e->state, k); }
static bool asr2_st_truth(asr2_env *e, const char *k) { return asr2_truth(asr2_st(e, k)); }
static bool asr2_st_nonnull(asr2_env *e, const char *k) { return asr2_nonnull(asr2_st(e, k)); }
static const char *asr2_st_str(asr2_env *e, const char *k, const char *def) {
    const json_t *v = asr2_st(e, k); return asr2_is_str(v) ? asr2_str(v) : def;
}

/* ── small pure helpers (transcribed claimed leaves, no PoP) ──────────────── */
static char *asr2_strip(const char *s) {
    size_t n = strlen(s); size_t a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\f'||s[a]=='\v')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\f'||s[b-1]=='\v')) b--;
    char *out = (char *)malloc((b - a) + 1);
    memcpy(out, s + a, b - a); out[b - a] = '\0'; return out;
}
static void asr2_lower_inplace(char *s) {
    for (char *p = s; *p; ++p) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
}
static bool asr2_in_set(const char *v, const char *const *set) {
    for (size_t i = 0; set[i]; i++) if (strcmp(v, set[i]) == 0) return true;
    return false;
}
static json_t *asr2_cap_text(const json_t *v) {
    if (!asr2_is_str(v)) return json_null();
    size_t n = json_len(v);
    if (n > ASR2_MAX_NORM_LEN) {
        char *buf = (char *)malloc(ASR2_MAX_NORM_LEN + 1);
        memcpy(buf, v->str_val, ASR2_MAX_NORM_LEN); buf[ASR2_MAX_NORM_LEN] = '\0';
        json_t *r = json_string(buf); free(buf); return r;
    }
    return json_copy(v);
}
static json_t *asr2_cap_list(const json_t *v) {
    json_t *r = json_array();
    if (!asr2_is_arr(v)) return r;
    size_t n = v->c.count; if (n > ASR2_MAX_CONTENT_LIST) n = ASR2_MAX_CONTENT_LIST;
    for (size_t i = 0; i < n; i++) json_append(r, json_copy(v->c.items[i]));
    return r;
}

/* ════════════════════════════ ported defs ═══════════════════════════════════ */

/* PoP: asr2_listen_address @ gateway/platforms/api_server.py:listen_address */
static json_t *asr2_listen_address(asr2_env *e, const json_t *in) {
    json_t *out = NULL;
    const json_t *extra = json_obj_get(in, "extra");
    /* host = extra.get("host", os.getenv("API_SERVER_HOST", DEFAULT_HOST)) */
    json_t *a = json_object();
    json_set(a, "name", json_string("API_SERVER_HOST"));
    json_set(a, "default", json_string(ASR2_DEFAULT_HOST));
    json_t *env_host = asr2_call(e, "getenv", a);
    if (asr2_is_raise(env_host)) { out = env_host; goto done; }
    const char *host = ASR2_DEFAULT_HOST;
    const json_t *eh = json_obj_get(extra, "host");
    if (asr2_nonnull(eh) && asr2_is_str(eh)) host = asr2_str(eh);
    else if (asr2_nonnull(env_host) && asr2_is_str(env_host)) host = asr2_str(env_host);
    /* raw_port = extra.get("port"); if None: os.getenv("API_SERVER_PORT", str(DEFAULT_PORT)) */
    const json_t *rp = json_obj_get(extra, "port");
    json_t *port_val = NULL;
    if (asr2_nonnull(rp)) port_val = json_copy(rp);
    else {
        json_t *b = json_object();
        json_set(b, "name", json_string("API_SERVER_PORT"));
        json_set(b, "default", json_string("8642"));  /* live: str(DEFAULT_PORT) */
        json_t *env_port = asr2_call(e, "getenv", b);
        if (asr2_is_raise(env_port)) { asr2_free(env_host); out = env_port; goto done; }
        port_val = env_port;
    }
    /* _coerce_port(raw_port, DEFAULT_PORT) */
    json_t *c = json_object();
    json_set(c, "value", port_val ? json_copy(port_val) : json_null());
    json_set(c, "default", json_number(ASR2_DEFAULT_PORT));
    json_t *port = asr2_call(e, "_coerce_port", c);
    if (asr2_is_raise(port)) { asr2_free(env_host); out = port; goto done; }
    out = json_array();
    json_append(out, json_string(host));
    json_append(out, port ? json_copy(port) : json_null());
    asr2_free(env_host); asr2_free(port); asr2_free(port_val);
done:
    return out;
}

/* PoP: asr2_content_has_visible_payload @ gateway/platforms/api_server.py:_content_has_visible_payload */
static json_t *asr2_content_has_visible_payload(asr2_env *e, const json_t *in) {
    (void)e;
    const json_t *content = json_obj_get(in, "content");
    bool visible = false;
    if (asr2_is_str(content)) {
        char *s = asr2_strip(asr2_sval(content));
        visible = s[0] != '\0'; free(s);
    } else if (asr2_is_arr(content)) {
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *part = content->c.items[i];
            if (!asr2_is_obj(part)) continue;
            const json_t *tv = json_obj_get(part, "type");
            char *pt = asr2_strip(asr2_sval(tv)); asr2_lower_inplace(pt);
            bool img = asr2_in_set(pt, ASR2_IMAGE_PART_TYPES);
            bool txt = asr2_in_set(pt, ASR2_TEXT_PART_TYPES);
            bool has_text = false;
            if (txt) {
                char *tt = asr2_strip(asr2_sval(json_obj_get(part, "text")));
                has_text = tt[0] != '\0'; free(tt);
            }
            free(pt);
            if (img || has_text) { visible = true; break; }
        }
    }
    return json_bool(visible);
}

/* PoP: asr2_session_row @ gateway/platforms/api_server.py:_session_row */
static json_t *asr2_session_row(asr2_env *e, const json_t *in) {
    json_t *out = NULL;
    const json_t *entry = json_obj_get(in, "entry");
    bool has_entry = asr2_is_obj(entry);
    json_t *row = json_object();
    for (size_t i = 0; ASR2_SESSION_KEYS[i]; i++) {
        json_t *a = json_object();
        json_set(a, "obj", json_string("session"));
        json_set(a, "key", json_string(ASR2_SESSION_KEYS[i]));
        json_t *v = asr2_call(e, "getattr", a);
        if (asr2_is_raise(v)) { asr2_free(row); out = v; goto done; }
        json_set(row, ASR2_SESSION_KEYS[i], v ? json_copy(v) : json_null());
        asr2_free(v);
    }
    /* | {"id": session.session_id, "model_config": dict(session.model_config)} */
    for (int k = 0; k < 2; k++) {
        json_t *a = json_object();
        json_set(a, "obj", json_string("session"));
        json_set(a, "key", json_string(k == 0 ? "session_id" : "model_config"));
        json_t *v = asr2_call(e, "getattr", a);
        if (asr2_is_raise(v)) { asr2_free(row); out = v; goto done; }
        json_set(row, k == 0 ? "id" : "model_config", v ? json_copy(v) : json_null());
        asr2_free(v);
    }
    if (has_entry) {
        json_t *a = json_object();
        json_set(a, "obj", json_string("entry"));
        json_set(a, "key", json_string("last_active"));
        json_t *v = asr2_call(e, "getattr", a);
        if (asr2_is_raise(v)) { asr2_free(row); out = v; goto done; }
        json_set(row, "last_active", v ? json_copy(v) : json_null()); asr2_free(v);
        a = json_object();
        json_set(a, "obj", json_string("entry"));
        json_set(a, "key", json_string("preview"));
        v = asr2_call(e, "getattr", a);
        if (asr2_is_raise(v)) { asr2_free(row); out = v; goto done; }
        json_set(row, "preview", v ? json_copy(v) : json_null()); asr2_free(v);
        a = json_object();
        json_set(a, "obj", json_string("entry"));
        json_set(a, "key", json_string("lineage_root_id"));
        v = asr2_call(e, "getattr", a);
        if (asr2_is_raise(v)) { asr2_free(row); out = v; goto done; }
        if (asr2_nonnull(v)) {
            /* live reads entry.lineage_root_id TWICE: once in the `is not None`
             * test, once for the value written into the row. Both are seams. */
            json_t *a2 = json_object();
            json_set(a2, "obj", json_string("entry"));
            json_set(a2, "key", json_string("lineage_root_id"));
            json_t *v2 = asr2_call(e, "getattr", a2);
            if (asr2_is_raise(v2)) { asr2_free(v); asr2_free(row); out = v2; goto done; }
            json_set(row, "_lineage_root_id", json_copy(v2));
            asr2_free(v2);
        }
        asr2_free(v);
    }
    out = row;
done:
    return out;
}

/* internal recursive helper for _normalize_chat_content (no PoP re-claim) */
static json_t *asr2_normalize(json_t *e_state, json_t *(*call)(void*,const char*,const json_t*),
                              void *ud, const json_t *content, int max_depth, int depth,
                              json_t **raise_out);

/* PoP: asr2_normalize_chat_content @ gateway/platforms/api_server.py:_normalize_chat_content */
static json_t *asr2_normalize_chat_content(asr2_env *e, const json_t *in) {
    json_t *ro = NULL;
    return asr2_normalize(e->state, e->call, e->ud, json_obj_get(in, "content"),
                          10, 0, &ro);
}

static json_t *asr2_normalize(json_t *state, json_t *(*call)(void*,const char*,const json_t*),
                              void *ud, const json_t *content, int max_depth, int depth,
                              json_t **raise_out) {
    (void)state;
    asr2_env tmp = { NULL, call, ud };
    if (depth > max_depth || !asr2_nonnull(content)) return json_string("");
    if (asr2_is_str(content)) return asr2_cap_text(content);
    if (asr2_is_arr(content)) {
        json_t *capped = asr2_cap_list(content);
        json_t *parts = json_array();
        double total = 0.0;
        for (size_t i = 0; i < json_len(capped); i++) {
            const json_t *item = json_get(capped, i);
            json_t *part = json_string("");
            if (asr2_is_str(item)) { asr2_free(part); part = json_copy(item); }
            else if (asr2_is_obj(item)) {
                const json_t *tv = json_obj_get(item, "type");
                char *pt = asr2_strip(asr2_sval(tv)); asr2_lower_inplace(pt);
                if (asr2_in_set(pt, ASR2_TEXT_PART_TYPES)) {
                    const json_t *text = json_obj_get(item, "text");
                    if (asr2_truth(text)) {
                        /* FIXTURE DOMAIN: text is a non-empty str, so the live
                         * `with suppress(Exception): part = str(text)` is the
                         * identity. Non-str text is excluded (see header). */
                        if (asr2_is_str(text)) { asr2_free(part); part = json_copy(text); }
                    }
                }
                free(pt);
            } else if (asr2_is_arr(item)) {
                asr2_free(part);
                part = asr2_normalize(state, call, ud, item, max_depth, depth + 1, raise_out);
                if (*raise_out) { asr2_free(capped); asr2_free(parts); return NULL; }
                if (!part) part = json_string("");
            }
            if (asr2_truth(part)) {
                json_t *cp = asr2_cap_text(part);
                asr2_free(part); part = cp;
                json_append(parts, json_copy(part));
                total += (double)json_len(part);
            }
            asr2_free(part);
            if (total >= ASR2_MAX_NORM_LEN) break;
        }
        /* "\n".join(parts) — build directly, then cap. */
        size_t jl = 0; char *buf = NULL;
        for (size_t i = 0; i < json_len(parts); i++) {
            const char *s = asr2_sval(json_get(parts, i));
            size_t sl = strlen(s);
            buf = (char *)realloc(buf, jl + sl + 2);
            if (i) buf[jl++] = '\n';
            memcpy(buf + jl, s, sl); jl += sl; buf[jl] = '\0';
        }
        if (!buf) { buf = (char *)malloc(1); buf[0] = '\0'; }
        json_t *joined = json_string(buf);
        free(buf); asr2_free(capped);
        json_t *result = asr2_cap_text(joined);
        asr2_free(joined); asr2_free(parts);
        return result;
    }
    /* fallback: str(content) inside try/except Exception */
    {
        json_t *a = json_object();
        json_set(a, "value", json_copy(content));
        json_t *s = asr2_call(&tmp, "py_str", a);
        if (asr2_is_raise(s)) {
            if (asr2_propagates(s)) { *raise_out = s; return NULL; }
            asr2_free(s); return json_string("");
        }
        json_t *r = asr2_cap_text(s); asr2_free(s); return r;
    }
}

/* PoP: asr2_room_grant_delegate @ gateway/platforms/api_server.py:_room_grant_delegate */
static json_t *asr2_room_grant_delegate(asr2_env *e, const json_t *in) {
    json_t *a = json_object();
    json_set(a, "name", json_copy(json_obj_get(in, "name")));
    return asr2_call(e, "room_grant_call", a);
}

/* PoP: asr2_run_route_delegate @ gateway/platforms/api_server.py:_run_route_delegate */
static json_t *asr2_run_route_delegate(asr2_env *e, const json_t *in) {
    json_t *a = json_object();
    json_set(a, "name", json_copy(json_obj_get(in, "name")));
    return asr2_call(e, "run_route_call", a);
}

/* helper: emit a web.json_response seam call */
static json_t *asr2_json_response(asr2_env *e, const json_t *body) {
    json_t *a = json_object();
    json_set(a, "body", json_copy(body));
    return asr2_call(e, "web.json_response", a);
}
/* helper: _error_response(message,status,err_type=...) seam */
static json_t *asr2_error_response(asr2_env *e, const char *message, int status, const char *err_type) {
    json_t *a = json_object();
    json_set(a, "message", json_string(message));
    json_set(a, "status", json_number(status));
    json_set(a, "err_type", err_type ? json_string(err_type) : json_null());
    return asr2_call(e, "_error_response", a);
}

/* PoP: asr2_handle_skills @ gateway/platforms/api_server.py:APIServerAdapter._handle_skills */
static json_t *asr2_handle_skills(asr2_env *e, const json_t *in) {
    (void)in;
    json_t *out = NULL;
    json_t *fa = json_object();
    json_set(fa, "skip_disabled", json_bool(false));
    json_set(fa, "include_editorial", json_bool(true));
    json_t *found = asr2_call(e, "find_all_skills", fa);
    if (asr2_is_raise(found)) {
        if (asr2_propagates(found)) { out = found; goto done; }
        asr2_free(found);
        out = asr2_error_response(e, "Failed to enumerate skills", 500, "server_error");
        goto done;
    }
    json_t *a = json_object();
    json_set(a, "a0", json_copy(found));
    json_t *skills = asr2_call(e, "sort_skills", a);
    asr2_free(found);
    if (asr2_is_raise(skills)) {
        if (asr2_propagates(skills)) { out = skills; goto done; }
        asr2_free(skills);
        out = asr2_error_response(e, "Failed to enumerate skills", 500, "server_error");
        goto done;
    }
    json_t *body = json_object();
    json_set(body, "object", json_string("list"));
    json_set(body, "data", skills ? json_copy(skills) : json_null());
    out = asr2_json_response(e, body);
    asr2_free(skills); asr2_free(body);
done:
    return out;
}

/* Python `needle in container`: list/tuple membership, str SUBSTRING, dict KEY.
 * The seam script for _get_platform_tools is a per-call queue, so a bare
 * ["alpha"] delivers the STRING "alpha" on the first call — live then does
 * `"alpha" in "alpha"` (True) while array-only json_len() would say 0. */
static bool asr2_contains(const json_t *c, const char *needle) {
    if (!c || !needle) return false;
    if (c->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(c); i++)
            if (strcmp(asr2_sval(json_get(c, i)), needle) == 0) return true;
        return false;
    }
    if (c->type == JSON_STRING) return c->str_val && strstr(c->str_val, needle) != NULL;
    if (c->type == JSON_OBJECT) return json_has(c, needle);
    return false;
}

/* PoP: asr2_handle_toolsets @ gateway/platforms/api_server.py:APIServerAdapter._handle_toolsets */
static json_t *asr2_handle_toolsets(asr2_env *e, const json_t *in) {
    (void)in;
    json_t *out = NULL;
    json_t *cfg = asr2_seam0(e, "load_config");
    if (asr2_is_raise(cfg)) {
        if (asr2_propagates(cfg)) { out = cfg; goto done; }
        asr2_free(cfg); goto outer_except;
    }
    json_t *a = json_object();
    json_set(a, "a0", json_copy(cfg));
    json_set(a, "a1", json_string("api_server"));
    json_set(a, "include_default_mcp_servers", json_bool(false));
    json_t *enabled = asr2_call(e, "_get_platform_tools", a);
    if (asr2_is_raise(enabled)) {
        if (asr2_propagates(enabled)) { asr2_free(cfg); out = enabled; goto done; }
        asr2_free(enabled); asr2_free(cfg); goto outer_except;
    }
    json_t *fa = json_object(); json_set(fa, "a0", json_copy(cfg));
    json_t *feat = asr2_call(e, "get_nous_subscription_features", fa);
    if (asr2_is_raise(feat)) {
        if (asr2_propagates(feat)) { asr2_free(cfg); asr2_free(enabled); out = feat; goto done; }
        asr2_free(feat); asr2_free(cfg); asr2_free(enabled); goto outer_except;
    }
    json_t *tuples = asr2_seam0(e, "_get_effective_configurable_toolsets");
    if (asr2_is_raise(tuples)) {
        if (asr2_propagates(tuples)) { asr2_free(cfg); asr2_free(enabled); asr2_free(feat); out = tuples; goto done; }
        asr2_free(tuples); asr2_free(cfg); asr2_free(enabled); asr2_free(feat); goto outer_except;
    }
    json_t *data = json_array();
    for (size_t i = 0; tuples && i < json_len(tuples); i++) {
        const json_t *tup = json_get(tuples, i);
        const char *name = asr2_sval(json_get(tup, 0));
        const char *label = asr2_sval(json_get(tup, 1));
        const char *desc = asr2_sval(json_get(tup, 2));
        json_t *ra = json_object(); json_set(ra, "a0", json_string(name));
        json_t *toolset = asr2_call(e, "resolve_toolset", ra);
        json_t *tools = json_array();
        if (asr2_is_raise(toolset)) {
            if (asr2_propagates(toolset)) {
                asr2_free(cfg); asr2_free(enabled); asr2_free(feat); asr2_free(tuples);
                asr2_free(data); asr2_free(tools); out = toolset; goto done;
            }
            asr2_free(toolset);   /* inner `except Exception: tools = []` */
        } else {
            for (size_t j = 0; j < json_len(toolset); j++) {
                const json_t *tv = json_get(toolset, j);
                bool seen = false;
                for (size_t k = 0; k < json_len(tools); k++)
                    if (strcmp(asr2_sval(json_get(tools, k)), asr2_sval(tv)) == 0) { seen = true; break; }
                if (!seen) json_append(tools, json_copy(tv));
            }
            for (size_t x = 0; x < json_len(tools); x++)
                for (size_t y = x + 1; y < json_len(tools); y++)
                    if (strcmp(asr2_sval(json_get(tools, x)), asr2_sval(json_get(tools, y))) > 0) {
                        json_t *tmp = tools->c.items[x]; tools->c.items[x] = tools->c.items[y]; tools->c.items[y] = tmp;
                    }
            asr2_free(toolset);
        }
        bool is_enabled = asr2_contains(enabled, name);
        json_t *ca = json_object();
        json_set(ca, "a0", json_string(name));
        json_set(ca, "a1", json_copy(cfg));
        json_set(ca, "features", json_copy(feat));
        json_t *configured = asr2_call(e, "_toolset_has_keys", ca);
        if (asr2_is_raise(configured)) {
            if (asr2_propagates(configured)) {
                asr2_free(cfg); asr2_free(enabled); asr2_free(feat); asr2_free(tuples);
                asr2_free(data); asr2_free(tools); out = configured; goto done;
            }
            asr2_free(configured);
            asr2_free(cfg); asr2_free(enabled); asr2_free(feat); asr2_free(tuples);
            asr2_free(data); asr2_free(tools); goto outer_except;
        }
        json_t *d = json_object();
        json_set(d, "name", json_string(name));
        json_set(d, "label", json_string(label));
        json_set(d, "description", json_string(desc));
        json_set(d, "enabled", json_bool(is_enabled));
        json_set(d, "configured", json_bool(asr2_truth(configured)));
        json_set(d, "tools", json_copy(tools));
        json_append(data, d);
        asr2_free(configured); asr2_free(tools);
    }
    json_t *body = json_object();
    json_set(body, "object", json_string("list"));
    json_set(body, "platform", json_string("api_server"));
    json_set(body, "data", json_copy(data));
    out = asr2_json_response(e, body);
    asr2_free(data); asr2_free(body);
    asr2_free(cfg); asr2_free(enabled); asr2_free(feat); asr2_free(tuples);
    goto done;
outer_except:
    out = asr2_error_response(e, "Failed to enumerate toolsets", 500, "server_error");
done:
    return out;
}

/* PoP: asr2_ensure_store @ gateway/platforms/api_server.py:APIServerAdapter._ensure_store */
static json_t *asr2_ensure_store(asr2_env *e, const json_t *in) {
    (void)in;
    json_t *out = NULL;
    if (asr2_st_nonnull(e, "gateway_runner")) {
        json_t *home = asr2_seam0(e, "get_hermes_home");
        if (asr2_is_raise(home)) { out = home; goto done; }
        json_t *a = json_object();
        json_set(a, "a0", json_string("<adapter>"));
        json_set(a, "a1", home ? json_copy(home) : json_null());
        out = asr2_call(e, "selected_session_db", a);
        asr2_free(home); goto done;
    }
    if (asr2_st_nonnull(e, "_session_db")) { out = json_copy(asr2_st(e, "_session_db")); goto done; }
    json_t *home = asr2_seam0(e, "get_hermes_home");
    if (asr2_is_raise(home)) {
        if (asr2_propagates(home)) { out = home; goto done; }
        asr2_free(home); out = json_null(); goto done;
    }
    json_t *a = json_object(); json_set(a, "a0", home ? json_copy(home) : json_null());
    json_t *db = asr2_call(e, "_open_and_cache_session_db", a);
    asr2_free(home);
    if (asr2_is_raise(db)) {
        if (asr2_propagates(db)) { out = db; goto done; }
        asr2_free(db); out = json_null(); goto done;
    }
    out = db;
done:
    return out;
}

/* PoP: asr2_store_beside @ gateway/platforms/api_server.py:APIServerAdapter._store_beside */
static json_t *asr2_store_beside(asr2_env *e, const json_t *in) {
    const json_t *db = json_obj_get(in, "db");
    if (!asr2_nonnull(db)) return json_null();
    json_t *a = json_object(); json_set(a, "a0", json_string("<adapter>"));
    return asr2_call(e, "adapter_persistence", a);
}

/* PoP: asr2_current_response_store @ gateway/platforms/api_server.py:APIServerAdapter._current_response_store */
static json_t *asr2_current_response_store(asr2_env *e, const json_t *in) {
    (void)in;
    json_t *out = NULL;
    json_t *home = asr2_seam0(e, "get_hermes_home");
    if (asr2_is_raise(home)) { out = home; goto done; }
    const char *home_s = asr2_sval(home);
    const char *current = asr2_st_str(e, "_response_store_home", "");
    if (strcmp(home_s, current) == 0) { out = json_copy(asr2_st(e, "_response_store")); goto done; }
    json_t *stores = asr2_st(e, "_response_stores");
    json_t *store = stores ? json_obj_get(stores, home_s) : NULL;
    bool store_owned = false;   /* only a fresh seam result is port-owned */
    if (!asr2_nonnull(store)) {
        json_t *a = json_object();
        char path[1024];
        snprintf(path, sizeof(path), "%s/response_store.db", home_s);
        json_set(a, "db_path", json_string(path));
        store = asr2_call(e, "ResponseStore", a);
        if (asr2_is_raise(store)) { asr2_free(home); out = store; goto done; }
        if (!stores) { stores = json_object(); json_set(e->state, "_response_stores", stores); }
        json_set(stores, home_s, json_copy(store));
        store_owned = true;
    }
    out = json_copy(store);
    /* On the cache hit `store` is BORROWED from state["_response_stores"];
     * freeing it corrupts the state tree (0xC0000374 at harness teardown). */
    if (store_owned) asr2_free(store);
    asr2_free(home);
done:
    return out;
}

/* PoP: asr2_check_run_auth @ gateway/platforms/api_server.py:APIServerAdapter._check_run_auth */
static json_t *asr2_check_run_auth(asr2_env *e, const json_t *in) {
    json_t *a = json_object();
    json_set(a, "a0", json_string("<adapter>"));
    json_set(a, "a1", json_copy(json_obj_get(in, "request")));
    json_set(a, "permission", json_copy(json_obj_get(in, "permission")));
    return asr2_call(e, "runs._check_run_auth", a);
}

/* PoP: asr2_request_owns_run @ gateway/platforms/api_server.py:APIServerAdapter._request_owns_run */
static json_t *asr2_request_owns_run(asr2_env *e, const json_t *in) {
    json_t *a = json_object();
    json_set(a, "a0", json_string("<adapter>"));
    json_set(a, "a1", json_copy(json_obj_get(in, "request")));
    json_set(a, "a2", json_copy(json_obj_get(in, "run_id")));
    return asr2_call(e, "runs._request_owns_run", a);
}

/* PoP: asr2_track_background_task @ gateway/platforms/api_server.py:APIServerAdapter._track_background_task */
static json_t *asr2_track_background_task(asr2_env *e, const json_t *in) {
    const json_t *task = json_obj_get(in, "task");
    bool tolerate = json_get_bool(in, "tolerate_missing", false);
    json_t *bg = asr2_st(e, "_background_tasks");
    if (!asr2_is_arr(bg)) { bg = json_array(); json_set(e->state, "_background_tasks", bg); }
    json_t *a = json_object(); json_set(a, "task", json_copy(task));
    json_t *add = asr2_call(e, "bg.add", a);
    if (asr2_is_raise(add)) {
        const char *kind = asr2_kind(add);
        /* live: tolerate_missing wraps BOTH calls in suppress(TypeError,
         * AttributeError); the normal path suppresses TypeError only. */
        bool suppressed = tolerate
            ? (asr2_exc_is(kind, "TypeError") || asr2_exc_is(kind, "AttributeError"))
            : asr2_exc_is(kind, "TypeError");
        if (!suppressed) return add;
        asr2_free(add);
        if (tolerate) return json_null();       /* return after the tolerate with-block */
    } else {
        asr2_free(add);
        json_append(bg, json_copy(task));
    }
    /* hasattr(task,"add_done_callback"): task doubles always have it. */
    json_t *b = json_object(); json_set(b, "task", json_copy(task));
    json_t *cb = asr2_call(e, "bg.add_done_callback", b);
    if (asr2_is_raise(cb)) {
        const char *ck = asr2_kind(cb);
        /* Outside the suppress on the normal path, a ValueError/anything else
         * from add_done_callback PROPAGATES; only tolerate_missing suppresses
         * TypeError/AttributeError here. */
        if (!tolerate || !(asr2_exc_is(ck, "TypeError") || asr2_exc_is(ck, "AttributeError")))
            return cb;
        asr2_free(cb);
    }
    return json_null();
}

/* ── dispatch table ───────────────────────────────────────────────────────── */
typedef json_t *(*asr2_fn)(asr2_env *, const json_t *);
static const struct { const char *name; asr2_fn fn; } asr2_table[] = {
    {"listen_address", asr2_listen_address},
    {"content_has_visible_payload", asr2_content_has_visible_payload},
    {"session_row", asr2_session_row},
    {"normalize_chat_content", asr2_normalize_chat_content},
    {"room_grant_delegate", asr2_room_grant_delegate},
    {"run_route_delegate", asr2_run_route_delegate},
    {"handle_skills", asr2_handle_skills},
    {"handle_toolsets", asr2_handle_toolsets},
    {"ensure_store", asr2_ensure_store},
    {"store_beside", asr2_store_beside},
    {"current_response_store", asr2_current_response_store},
    {"check_run_auth", asr2_check_run_auth},
    {"request_owns_run", asr2_request_owns_run},
    {"track_background_task", asr2_track_background_task},
};
