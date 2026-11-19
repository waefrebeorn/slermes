/* t_port_tail_revive4.c — harness for the tail group-4 revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (NULL -> null; {"__raise__": ...} -> exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the mutated input object minus "io"
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * Each port .c carries its own uniquely-prefixed seam io type; T4_DISP
 * generates a type-correct dispatcher per file and T4_WRAP a case wrapper.
 *
 * Gate (explicit objs; the harness #includes the port sources, so only
 * libjson is linked):
 *   bash tests/run_one_oracle.sh tail_revive4 oracle_tail_revive4.py "" "lib/libjson/json.o"
 */
#include "agent/transports/codex_gaps.c"
#include "hermes_cli/update_restart_recovery_gaps.c"
#include "hermes_cli/local_runtime/gguf_gaps.c"
#include "tools/tool_search_gaps.c"
#include "gateway/stream_consumer_gaps.c"
#include "agent/review_idle_queue_gaps.c"
#include "agent/prompt_caching_gaps.c"
#include "tools/terminal_tool_gaps.c"
#include "hermes_cli/local_runtime/load_progress_gaps.c"
#include "hermes_cli/nous_auth_keepalive_gaps.c"
#include "agent/codex_headers_gaps.c"
#include "agent/lsp/lsp_client_gaps.c"
#include "agent/tool_dispatch_helpers_gaps.c"
#include "tools/clarify_gateway_gaps.c"
#include "tools/skills_ast_audit.c"
#include "agent/tool_executor_gaps.c"
#include "tools/vision_tools_gaps.c"
#include "agent/checkpoints/checkpoint_manager_gaps.c"
#include "hermes_cli/update_abort_recovery_gaps.c"
#include "gateway/runtime_footer.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} t4_script_t;

static json_t *t4_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *t4_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return t4_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *t4_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message);
    return e;
}

/* Script lookup shared by every per-file dispatcher. */
static json_t *t4_dispatch_impl(void *ctx, const char *name, json_t *args) {
    (void)args;
    t4_script_t *sc = (t4_script_t *)ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return t4_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return t4_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return t4_entry_result(json_get(entry, idx));
    }
    return t4_entry_result(entry);
}

/* One type-correct dispatcher per port io type. */
#define T4_DISP(P) \
    static json_t *P##_disp(P##_io_t *io, const char *name, json_t *args) { \
        return t4_dispatch_impl(io->ctx, name, args); \
    }

T4_DISP(cx)
T4_DISP(ur)
T4_DISP(sc)
T4_DISP(rq)
T4_DISP(pc)
T4_DISP(tt)
T4_DISP(nk)
T4_DISP(lc)
T4_DISP(te)
T4_DISP(vt)
T4_DISP(cm)
T4_DISP(ua)
T4_DISP(rft)

/* One wrapper per case; pure ports take void* io and ignore it. */
#define T4_WRAP(NAME, P, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        P##_io_t io; \
        io.call = P##_disp; \
        io.trace = trace; \
        io.ctx = sc; \
        return FN(&io, work); \
    }
#define T4_WRAP_PURE(NAME, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        (void)trace; (void)sc; \
        return FN(NULL, work); \
    }

T4_WRAP(is_post_tool_replay, cx, is_post_tool_replay)
T4_WRAP(is_opencode_responses_backend, cx, is_opencode_responses_backend)
T4_WRAP(is_azure_foundry_responses, cx, is_azure_foundry_responses)
T4_WRAP_PURE(native_compaction_active, native_compaction_active)
T4_WRAP(systemd_verified_active, ur, systemd_verified_active)
T4_WRAP(unit_main_pid, ur, unit_main_pid)
T4_WRAP(unit_is_active, ur, unit_is_active)
T4_WRAP(serve_unit_replaced, ur, serve_unit_replaced)
T4_WRAP_PURE(n_vocab, n_vocab)
T4_WRAP_PURE(n_head, n_head)
T4_WRAP_PURE(head_dim_k, head_dim_k)
T4_WRAP_PURE(head_dim_v, head_dim_v)
T4_WRAP_PURE(effective_defer_tools, effective_defer_tools)
T4_WRAP_PURE(schema_has_external_ref, schema_has_external_ref)
T4_WRAP(stream_is_message, sc, stream_is_message)
T4_WRAP_PURE(is_flood_error, is_flood_error)
T4_WRAP_PURE(defer_max_age_s, defer_max_age_s)
T4_WRAP(_quiet_for, rq, _quiet_for)
T4_WRAP(envelope_tool_part_cache_markers_supported, pc, envelope_tool_part_cache_markers_supported)
T4_WRAP_PURE(is_qwen_model, is_qwen_model)
T4_WRAP(is_container_backend, tt, is_container_backend)
T4_WRAP(in_delegated_child_context, tt, in_delegated_child_context)
T4_WRAP_PURE(composite_percent, composite_percent)
T4_WRAP(observed_lifetime_seconds, nk, observed_lifetime_seconds)
T4_WRAP_PURE(is_official_codex_base_url, is_official_codex_base_url)
T4_WRAP(connection_is_open, lc, connection_is_open)
T4_WRAP_PURE(detect_upstream_elision, detect_upstream_elision)
T4_WRAP_PURE(selection_attempt_tokens, selection_attempt_tokens)
T4_WRAP_PURE(_is_importlib, tools_skills_ast_audit_is_importlib)
T4_WRAP(_human_wait_seconds, te, _human_wait_seconds)
T4_WRAP(profile_rejects_tool_media, vt, profile_rejects_tool_media)
T4_WRAP(exceeds_size_cap, cm, exceeds_size_cap)
T4_WRAP(serve_unit_recovery_available, ua, serve_unit_recovery_available)
T4_WRAP(_env_cwd, rft, gateway_runtime_footer_env_cwd)

typedef json_t *(*t4_fn_t)(json_t *, void *, json_t *);
typedef struct { const char *func; t4_fn_t fn; } t4_case_t;

static const t4_case_t T4_CASES[] = {
    { "is_post_tool_replay", wrap_is_post_tool_replay },
    { "is_opencode_responses_backend", wrap_is_opencode_responses_backend },
    { "is_azure_foundry_responses", wrap_is_azure_foundry_responses },
    { "native_compaction_active", wrap_native_compaction_active },
    { "systemd_verified_active", wrap_systemd_verified_active },
    { "unit_main_pid", wrap_unit_main_pid },
    { "unit_is_active", wrap_unit_is_active },
    { "serve_unit_replaced", wrap_serve_unit_replaced },
    { "n_vocab", wrap_n_vocab },
    { "n_head", wrap_n_head },
    { "head_dim_k", wrap_head_dim_k },
    { "head_dim_v", wrap_head_dim_v },
    { "effective_defer_tools", wrap_effective_defer_tools },
    { "schema_has_external_ref", wrap_schema_has_external_ref },
    { "stream_is_message", wrap_stream_is_message },
    { "is_flood_error", wrap_is_flood_error },
    { "defer_max_age_s", wrap_defer_max_age_s },
    { "_quiet_for", wrap__quiet_for },
    { "envelope_tool_part_cache_markers_supported", wrap_envelope_tool_part_cache_markers_supported },
    { "is_qwen_model", wrap_is_qwen_model },
    { "is_container_backend", wrap_is_container_backend },
    { "in_delegated_child_context", wrap_in_delegated_child_context },
    { "composite_percent", wrap_composite_percent },
    { "observed_lifetime_seconds", wrap_observed_lifetime_seconds },
    { "is_official_codex_base_url", wrap_is_official_codex_base_url },
    { "connection_is_open", wrap_connection_is_open },
    { "detect_upstream_elision", wrap_detect_upstream_elision },
    { "selection_attempt_tokens", wrap_selection_attempt_tokens },
    { "_is_importlib", wrap__is_importlib },
    { "_human_wait_seconds", wrap__human_wait_seconds },
    { "profile_rejects_tool_media", wrap_profile_rejects_tool_media },
    { "exceeds_size_cap", wrap_exceeds_size_cap },
    { "serve_unit_recovery_available", wrap_serve_unit_recovery_available },
    { "_env_cwd", wrap__env_cwd },
};
#define T4_CASE_COUNT (sizeof(T4_CASES) / sizeof(T4_CASES[0]))

static char *t4_read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = t4_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const t4_case_t *tc = NULL;
        for (size_t i = 0; i < T4_CASE_COUNT; i++) {
            if (strcmp(T4_CASES[i].func, func) == 0) { tc = &T4_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            t4_script_t sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *trace = json_array();
            json_t *r = tc->fn(trace, &sc, work);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(trace);
            json_free(sc.cursors);
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}