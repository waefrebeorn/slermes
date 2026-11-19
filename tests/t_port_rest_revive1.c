/* t_port_rest_revive1.c — harness for the deep-stub rest group-1 revival.
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
 * Each port .c carries its own uniquely-prefixed seam io type; R1_DISP
 * generates a type-correct dispatcher per file and R1_WRAP a case wrapper.
 * Pure ports (skills_guard, archive_safe, file_operations warning, proxy
 * base name/display_name) take void* io and ignore it.
 *
 * Gate (explicit objs; the harness #includes the port sources — all are
 * libjson-only gap files — so only libjson is linked):
 *   bash tests/run_one_oracle.sh rest_revive1 oracle_rest_revive1.py "" "lib/libjson/json.o"
 */
#include "agent/bedrock_adapter_gaps.c"
#include "gateway/outbound/send_message_tool_gaps.c"
#include "tools/skills_guard_gaps.c"
#include "gateway/session_gaps.c"
#include "hermes_cli/archive_safe_gaps.c"
#include "hermes_cli/_install_repair_gaps.c"
#include "gateway/scale_to_zero_gaps3.c"
#include "hermes_cli/gateway_windows_gaps.c"
#include "tools/file_operations_gaps.c"
#include "hermes_cli/proxy/adapters/proxy_adapters_base_ports.c"
#include "hermes_cli/cli_output_gaps3.c"
#include "tools/desktop_ui_gaps3.c"
#include "agent/vertex_adapter_gaps3.c"
#include "tools/file_state_gaps3.c"
#include "gateway/shutdown_watchdog_gaps3.c"
#include "tools/terminal_scope_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} r1_script_t;

static json_t *r1_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *r1_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return r1_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *r1_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message);
    return e;
}

/* Script lookup shared by every per-file dispatcher. */
static json_t *r1_dispatch_impl(void *ctx, const char *name, json_t *args) {
    (void)args;
    r1_script_t *sc = (r1_script_t *)ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return r1_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return r1_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return r1_entry_result(json_get(entry, idx));
    }
    return r1_entry_result(entry);
}

/* One type-correct dispatcher per port io type. */
#define R1_DISP(P) \
    static json_t *P##_disp(P##_io_t *io, const char *name, json_t *args) { \
        return r1_dispatch_impl(io->ctx, name, args); \
    }

R1_DISP(ba)
R1_DISP(sm)
R1_DISP(sp)
R1_DISP(ir)
R1_DISP(sz)
R1_DISP(gw)
R1_DISP(co)
R1_DISP(du)
R1_DISP(va)
R1_DISP(fs)
R1_DISP(sw)
R1_DISP(ts)

/* One wrapper per case; pure ports take void* io and ignore it. */
#define R1_WRAP(NAME, P, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        P##_io_t io; \
        io.call = P##_disp; \
        io.trace = trace; \
        io.ctx = sc; \
        return FN(&io, work); \
    }
#define R1_WRAP_PURE(NAME, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        (void)trace; (void)sc; \
        return FN(NULL, work); \
    }

R1_WRAP(resolve_bedrock_bearer_token, ba, resolve_bedrock_bearer_token)
R1_WRAP(cache_point_rejection_placement, ba, cache_point_rejection_placement)
R1_WRAP(strip_cache_points, ba, strip_cache_points)
R1_WRAP(recover_from_cache_point_rejection, ba, recover_from_cache_point_rejection)
R1_WRAP(prepare_send_message_platforms, sm, prepare_send_message_platforms)
R1_WRAP(bounded_send_error, sm, bounded_send_error)
R1_WRAP_PURE(shell_write_re, shell_write_re)
R1_WRAP_PURE(prose_modify_re, prose_modify_re)
R1_WRAP_PURE(content_contract_re, content_contract_re)
R1_WRAP(named_profile_for_key, sp, named_profile_for_key)
R1_WRAP(profile_home_for_key, sp, profile_home_for_key)
R1_WRAP(owner_key_for_session_id, sp, owner_key_for_session_id)
R1_WRAP(get_transcript_drain_lock, sp, get_transcript_drain_lock)
R1_WRAP_PURE(normalize_archive_parts, normalize_archive_parts)
R1_WRAP(normalize_windows_path, ir, normalize_windows_path)
R1_WRAP(windows_user_path_entries, ir, windows_user_path_entries)
R1_WRAP(read_user_path_raw, ir, read_user_path_raw)
R1_WRAP(dashboard_client_heartbeat_path, sz, dashboard_client_heartbeat_path)
R1_WRAP(touch_dashboard_client_heartbeat, sz, touch_dashboard_client_heartbeat)
R1_WRAP(dashboard_client_last_seen, sz, dashboard_client_last_seen)
R1_WRAP(start_attestation_path, gw, start_attestation_path)
R1_WRAP_PURE(macos_protected_search_warning, macos_protected_search_warning)
R1_WRAP_PURE(pxb_name, pxb_name)
R1_WRAP_PURE(pxb_display_name, pxb_display_name)
R1_WRAP(line_input, co, line_input)
R1_WRAP(user_enabled, du, user_enabled)
R1_WRAP(has_explicit_vertex_config, va, has_explicit_vertex_config)
R1_WRAP(forget_task, fs, forget_task)
R1_WRAP(get_loop_tick_socket_path, sw, get_loop_tick_socket_path)
R1_WRAP(get_terminal_scope, ts, get_terminal_scope)

typedef json_t *(*r1_fn_t)(json_t *, void *, json_t *);
typedef struct { const char *func; r1_fn_t fn; } r1_case_t;

static const r1_case_t R1_CASES[] = {
    { "resolve_bedrock_bearer_token", wrap_resolve_bedrock_bearer_token },
    { "cache_point_rejection_placement", wrap_cache_point_rejection_placement },
    { "strip_cache_points", wrap_strip_cache_points },
    { "recover_from_cache_point_rejection", wrap_recover_from_cache_point_rejection },
    { "prepare_send_message_platforms", wrap_prepare_send_message_platforms },
    { "bounded_send_error", wrap_bounded_send_error },
    { "shell_write_re", wrap_shell_write_re },
    { "prose_modify_re", wrap_prose_modify_re },
    { "content_contract_re", wrap_content_contract_re },
    { "named_profile_for_key", wrap_named_profile_for_key },
    { "profile_home_for_key", wrap_profile_home_for_key },
    { "owner_key_for_session_id", wrap_owner_key_for_session_id },
    { "get_transcript_drain_lock", wrap_get_transcript_drain_lock },
    { "normalize_archive_parts", wrap_normalize_archive_parts },
    { "normalize_windows_path", wrap_normalize_windows_path },
    { "windows_user_path_entries", wrap_windows_user_path_entries },
    { "read_user_path_raw", wrap_read_user_path_raw },
    { "dashboard_client_heartbeat_path", wrap_dashboard_client_heartbeat_path },
    { "touch_dashboard_client_heartbeat", wrap_touch_dashboard_client_heartbeat },
    { "dashboard_client_last_seen", wrap_dashboard_client_last_seen },
    { "start_attestation_path", wrap_start_attestation_path },
    { "macos_protected_search_warning", wrap_macos_protected_search_warning },
    { "pxb_name", wrap_pxb_name },
    { "pxb_display_name", wrap_pxb_display_name },
    { "line_input", wrap_line_input },
    { "user_enabled", wrap_user_enabled },
    { "has_explicit_vertex_config", wrap_has_explicit_vertex_config },
    { "forget_task", wrap_forget_task },
    { "get_loop_tick_socket_path", wrap_get_loop_tick_socket_path },
    { "get_terminal_scope", wrap_get_terminal_scope },
};
#define R1_CASE_COUNT (sizeof(R1_CASES) / sizeof(R1_CASES[0]))

static char *r1_read_line(FILE *f) {
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
    while ((line = r1_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const r1_case_t *tc = NULL;
        for (size_t i = 0; i < R1_CASE_COUNT; i++) {
            if (strcmp(R1_CASES[i].func, func) == 0) { tc = &R1_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            r1_script_t sc;
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
