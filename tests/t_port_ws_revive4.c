/* t_port_ws_revive4.c — harness for the web-server group-4 revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <cfn>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (NULL -> null; a parked {"__raise__": ...}
 *           spec wins over the return value)
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
 * SEAM OWNERSHIP: the dispatcher ADOPTS args (into the trace event); the
 * port must NOT free them after the call (ws_revive4.c contract).
 *
 * Gate (explicit objs; the harness #includes the port source, so only
 * libjson is linked):
 *   bash tests/run_one_oracle.sh ws_revive4 oracle_ws_revive4.py "" "lib/libjson/json.o"
 */
#include "hermes_cli/ws_revive4.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed work.io */
    json_t *cursors;  /* owned per-seam queue cursors */
    json_t *trace;    /* borrowed trace array */
} ws4h_script_t;

static json_t *ws4h_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message);
    return e;
}

static json_t *ws4h_make_exc(json_t *spec) {
    return ws4h_raise_spec(json_get_str(spec, "type", "RuntimeError"),
                           json_get_str(spec, "message", ""));
}

static json_t *ws4h_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ws4h_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Universal dispatcher: consumes the script, records {ev, args, ret} in
 * call order, ADOPTS args, returns a fresh result node (port frees). */
static json_t *ws4h_dispatch(void *io, const char *name, json_t *args) {
    /* The port passes the CHANNEL (g_ws4_io), like ws_revive2/3; the
     * script state rides chan.ctx. Casting io straight to the script
     * struct read the call-pointer as io_map (every lookup failed ->
     * "no script") and stack garbage as trace (json_append no-op). */
    ws4h_script_t *sc = (ws4h_script_t *)((ws4_io_t *)io)->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    json_t *result;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        result = ws4h_raise_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            result = ws4h_raise_spec("RuntimeError", msg);
        } else {
            json_set(sc->cursors, name, json_number(cur + 1));
            result = ws4h_entry_result(json_get(entry, idx));
        }
    } else {
        result = ws4h_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object()); /* adopts args */
    json_set(ev, "ret", result ? json_copy(result) : json_null());
    json_append(sc->trace, ev);
    return result;
}

typedef json_t *(*ws4h_fn_t)(json_t *);
typedef struct { const char *func; ws4h_fn_t fn; } ws4h_case_t;

static const ws4h_case_t WS4H_CASES[] = {
    { "ws_u_normalize_memory_provider_name", ws_u_normalize_memory_provider_name },
    { "ws_u_env_lookup", ws_u_env_lookup },
    { "ws_u_field_default", ws_u_field_default },
    { "ws_u_load_memory_provider", ws_u_load_memory_provider },
    { "ws_u_memory_provider_manifest", ws_u_memory_provider_manifest },
    { "ws_u_trim_setup_output", ws_u_trim_setup_output },
    { "ws_u_require_valid_memory_provider_name", ws_u_require_valid_memory_provider_name },
    { "ws_u_write_provider_flat", ws_u_write_provider_flat },
    { "ws_u_flat_json_path", ws_u_flat_json_path },
    { "ws_u_custom_endpoint_id", ws_u_custom_endpoint_id },
    { "ws_u_config_api_key_is_env_ref", ws_u_config_api_key_is_env_ref },
    { "ws_u_pool_entry_summary", ws_u_pool_entry_summary },
    { "ws_u_strip_session_list_rows", ws_u_strip_session_list_rows },
    { "ws_u_strip_dashboard_manifest", ws_u_strip_dashboard_manifest },
    { "ws_u_managed_response_meta", ws_u_managed_response_meta },
    { "ws_u_skill_meta_to_payload", ws_u_skill_meta_to_payload },
    { "ws_u_clear_skills_prompt_cache", ws_u_clear_skills_prompt_cache },
    { "ws_u_aux_usage_rows", ws_u_aux_usage_rows },
    { "ws_u_write_platform_enabled", ws_u_write_platform_enabled },
    { "ws_u_whatsapp_session_path", ws_u_whatsapp_session_path },
    { "ws_u_whatsapp_onboarding_payload", ws_u_whatsapp_onboarding_payload },
    { "ws_u_telegram_onboarding_base_url", ws_u_telegram_onboarding_base_url },
    { "ws_u_profile_setup_command", ws_u_profile_setup_command },
    { "ws_u_validate_oauth_profile", ws_u_validate_oauth_profile },
};
#define WS4H_CASE_COUNT (sizeof(WS4H_CASES) / sizeof(WS4H_CASES[0]))

static char *ws4h_read_line(FILE *f) {
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
    while ((line = ws4h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *in_owned = NULL;
        if (!in) { in_owned = json_object(); in = in_owned; }
        json_t *out = NULL;

        const ws4h_case_t *tc = NULL;
        for (size_t i = 0; i < WS4H_CASE_COUNT; i++) {
            if (strcmp(WS4H_CASES[i].func, func) == 0) { tc = &WS4H_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            ws4h_script_t sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *trace = json_array();
            sc.trace = trace;
            ws4_io_t chan;
            chan.call = ws4h_dispatch;
            chan.ctx = &sc;
            g_ws4_io = &chan;
            json_free(g_ws4_pending_raise);
            g_ws4_pending_raise = NULL;

            json_t *r = tc->fn(work);

            out = json_object();
            if (g_ws4_pending_raise) {
                json_free(r);
                json_set(out, "ret", g_ws4_pending_raise); /* adopts */
                g_ws4_pending_raise = NULL;
            } else {
                json_set(out, "ret", r ? r : json_null());
            }
            json_set(out, "trace", trace); /* adopts */
            json_obj_del(work, "io");
            json_set(out, "state", work); /* adopts */
            json_free(sc.cursors);
            g_ws4_io = NULL;
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(in_owned);
        json_free(rec);
    }
    return 0;
}
