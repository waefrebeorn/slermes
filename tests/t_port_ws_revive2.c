/* t_port_ws_revive2.c — harness for the web_server_wrappers group-2 revival.
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
 * {"__script__": true, "ret": <value>, "raise": {type, message[, status_code]}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * Seam ownership: the dispatcher ADOPTS args into the trace event (the
 * port never frees args after the call); results are caller-owned copies.
 * read_text results get one leading UTF-8 BOM stripped (utf-8-sig), the
 * same strip the oracle's read_text double applies. HTTPException raise
 * specs render str(exc) == "<status_code>: <message>" (default 400).
 *
 * Gate (the harness #includes the port source, so only libjson is linked):
 *   bash tests/run_one_oracle.sh ws_revive2 oracle_ws_revive2.py "" "lib/libjson/json.o"
 */
#include "hermes_cli/ws_revive2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} ws2h_script_t;

static json_t *ws2h_make_exc(json_t *spec) {
    const char *ty = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", ty);
    if (strcmp(ty, "HTTPException") == 0) {
        int sc = (int)json_get_num(spec, "status_code", 400);
        char buf[1200];
        snprintf(buf, sizeof buf, "%d: %s", sc, msg);
        json_set_hdr(e, "message", buf);
    } else {
        json_set_hdr(e, "message", msg);
    }
    return e;
}

static json_t *ws2h_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ws2h_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *ws2h_dispatch(ws2_io_t *io, const char *name, json_t *args) {
    ws2h_script_t *sc = (ws2h_script_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    json_t *ret;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ws2_raise("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ws2_raise("RuntimeError", msg);
        } else {
            json_set(sc->cursors, name, json_number(cur + 1));
            ret = ws2h_entry_result(json_get(entry, idx));
        }
    } else {
        ret = ws2h_entry_result(entry);
    }
    /* utf-8-sig: one leading BOM never reaches the live json.loads. */
    if (strcmp(name, "read_text") == 0 && ret && ret->type == JSON_STRING &&
        ret->str_val && (unsigned char)ret->str_val[0] == 0xEF &&
        (unsigned char)ret->str_val[1] == 0xBB && (unsigned char)ret->str_val[2] == 0xBF) {
        json_t *stripped = json_string(ret->str_val + 3);
        json_free(ret);
        ret = stripped;
    }
    /* Record the event; args ownership transfers into the trace. */
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

typedef json_t *(*ws2h_fn_t)(json_t *);
typedef struct { const char *func; ws2h_fn_t fn; } ws2h_case_t;
static const ws2h_case_t WS2H_CASES[] = {
    { "ws_u_validate_reference_timeout", ws_u_validate_reference_timeout },
    { "ws_u_is_command_provider_block", ws_u_is_command_provider_block },
    { "ws_u_models_from_custom_endpoint_entry", ws_u_models_from_custom_endpoint_entry },
    { "ws_u_webhook_route_summary", ws_u_webhook_route_summary },
    { "ws_u_field_value", ws_u_field_value },
    { "ws_u_field_visible", ws_u_field_visible },
    { "ws_u_coerce_schema_field", ws_u_coerce_schema_field },
    { "ws_u_aux_task_summary", ws_u_aux_task_summary },
    { "ws_u_merge_aux_into_by_model", ws_u_merge_aux_into_by_model },
    { "ws_u_http_response_error_detail", ws_u_http_response_error_detail },
    { "ws_u_require_memory_provider_ready", ws_u_require_memory_provider_ready },
    { "ws_u_profile_to_dict", ws_u_profile_to_dict },
    { "ws_u_managed_files_policy", ws_u_managed_files_policy },
    { "ws_u_multiplex_port_binding_conflict", ws_u_multiplex_port_binding_conflict },
    { "ws_u_infer_provider_on_model_change", ws_u_infer_provider_on_model_change },
    { "ws_u_split_text_for_speak_stream", ws_u_split_text_for_speak_stream },
    { "ws_u_get_dashboard_plugins", ws_u_get_dashboard_plugins },
    { "ws_u_update_memory_provider_config", ws_u_update_memory_provider_config },
    { "ws_u_whatsapp_linked_account_from_session", ws_u_whatsapp_linked_account_from_session },
};
#define WS2H_CASE_COUNT (sizeof(WS2H_CASES) / sizeof(WS2H_CASES[0]))

static char *ws2h_read_line(FILE *f) {
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
    while ((line = ws2h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        int in_owned = 0;
        if (!in) { in = json_object(); in_owned = 1; }
        json_t *out = NULL;

        const ws2h_case_t *tc = NULL;
        for (size_t i = 0; i < WS2H_CASE_COUNT; i++) {
            if (strcmp(WS2H_CASES[i].func, func) == 0) { tc = &WS2H_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            ws2_io_t io;
            ws2h_script_t sc;
            io.call = ws2h_dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            g_ws2_io = &io;
            json_t *r = tc->fn(work);
            g_ws2_io = NULL;
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(io.trace);
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
        if (in_owned) json_free(in);
        json_free(rec);
    }
    return 0;
}