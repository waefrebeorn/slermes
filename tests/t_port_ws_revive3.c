/* t_port_ws_revive3.c — harness for the web-server group-3 revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <cfn>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (null == None; {"__raise__": ...} == exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the mutated input object minus "io"
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * List-valued seam RETURNS must be wrapped {"__script__": true, "ret": [...]}
 * (a bare array is a per-call queue).
 *
 * The port keeps the original stub signatures json_t *fn(json_t *req);
 * the universal seam channel (g_w3_io) is installed per case below.
 * Seam ARGS are adopted by the seam (w3_seam records them in the trace);
 * the dispatcher never frees them.
 *
 * Gate (harness #includes the port source; only libjson is linked):
 *   bash tests/run_one_oracle.sh ws_revive3 oracle_ws_revive3.py "" "lib/libjson/json.o"
 */
#include "hermes_cli/ws_revive3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} w3_script_t;

static json_t *w3h_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *w3h_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return w3h_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *w3h_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message);
    return e;
}

/* Script dispatcher: args BORROWED here (w3_seam adopts them into the
 * trace event; the dispatcher must not free them). */
static json_t *w3h_disp(w3_io_t *io, const char *name, json_t *args) {
    (void)args;
    w3_script_t *sc = (w3_script_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return w3h_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return w3h_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return w3h_entry_result(json_get(entry, idx));
    }
    return w3h_entry_result(entry);
}

typedef json_t *(*w3h_fn_t)(json_t *);
typedef struct { const char *func; w3h_fn_t fn; } w3h_case_t;

static const w3h_case_t W3H_CASES[] = {
    { "ws_u_normalize_whatsapp_allowed_users", ws_u_normalize_whatsapp_allowed_users },
    { "ws_u_whatsapp_phone_from_identifier", ws_u_whatsapp_phone_from_identifier },
    { "ws_u_normalize_whatsapp_onboarding_mode", ws_u_normalize_whatsapp_onboarding_mode },
    { "ws_u_string_list", ws_u_string_list },
    { "ws_u_resolve_toolset_model_plugin", ws_u_resolve_toolset_model_plugin },
    { "ws_u_chat_image_extension", ws_u_chat_image_extension },
    { "ws_u_api_key_display", ws_u_api_key_display },
    { "ws_u_oauth_provider_disconnect_hint", ws_u_oauth_provider_disconnect_hint },
    { "ws_u_codex_device_code_start_error", ws_u_codex_device_code_start_error },
    { "ws_u_read_field", ws_u_read_field },
    { "ws_u_declared_field_is_set", ws_u_declared_field_is_set },
    { "ws_u_serialize_field_value", ws_u_serialize_field_value },
    { "ws_u_command_result", ws_u_command_result },
    { "ws_u_terminal_cfg_value", ws_u_terminal_cfg_value },
    { "ws_u_probe_ssh_backend", ws_u_probe_ssh_backend },
    { "ws_u_probe_modal_backend", ws_u_probe_modal_backend },
    { "ws_u_find_toolset_provider_row", ws_u_find_toolset_provider_row },
    { "ws_u_gc_oauth_sessions", ws_u_gc_oauth_sessions },
    { "ws_u_gc_mcp_oauth_flows", ws_u_gc_mcp_oauth_flows },
    { "ws_u_prune_telegram_onboarding_pairings", ws_u_prune_telegram_onboarding_pairings },
    { "ws_u_mcp_install_action_name", ws_u_mcp_install_action_name },
    { "ws_u_hub_action_name", ws_u_hub_action_name },
    { "ws_u_profile_cli_args", ws_u_profile_cli_args },
    { "ws_u_pairing_store", ws_u_pairing_store },
    { "ws_u_memory_provider_options", ws_u_memory_provider_options },
    { "ws_u_validate_dashboard_cron_context_from", ws_u_validate_dashboard_cron_context_from },
    { "ws_u_channel_managed_env_keys", ws_u_channel_managed_env_keys },
};
#define W3H_CASE_COUNT (sizeof(W3H_CASES) / sizeof(W3H_CASES[0]))

static char *w3h_read_line(FILE *f) {
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
    while ((line = w3h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const w3h_case_t *tc = NULL;
        for (size_t i = 0; i < W3H_CASE_COUNT; i++) {
            if (strcmp(W3H_CASES[i].func, func) == 0) { tc = &W3H_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            w3_script_t sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *trace = json_array();
            w3_io_t io;
            io.call = w3h_disp;
            io.trace = trace;
            io.ctx = &sc;
            g_w3_io = &io;
            json_t *r = tc->fn(work);
            g_w3_io = NULL;
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
