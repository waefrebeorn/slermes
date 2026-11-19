/* t_port_redact_flow.c — harness for the agent/redact.py FLOW + leaf port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret", "trace", "state"}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated policy registry.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret": v} / {"raise": {...}} or a
 * queue LIST of those), consumed by the dispatcher exactly as the oracle's
 * recording stubs are fed. A bare scalar entry is a constant return.
 */
#include "hermes_json.h"
#include "agent/redact_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} rf_script_ctx_t;

static json_t *rf_script_next(rf_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}

static json_t *rf_dispatch(rf_io_t *io, const char *name, json_t *args) {
    (void)args;
    rf_script_ctx_t *sc = (rf_script_ctx_t *)io->ctx;
    json_t *result = rf_script_next(sc, name);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *exc = json_object();
            json_t *t = json_obj_get(raise, "type");
            json_set(exc, "__raise__", t ? json_copy(t) : json_string("Exception"));
            json_t *msg = json_obj_get(raise, "message");
            json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return result ? json_copy(result) : json_null();
}

static void rf_io_init(rf_io_t *io, rf_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = rf_dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *rf_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

/* string field, distinguishing JSON null (None) from a present string. */
static const char *rf_str_or_null(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return "";
}

static json_t *rf_make_out(rf_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *rf_run_case(const char *func, json_t *in) {
    rf_io_t io;
    rf_script_ctx_t sc;
    json_t *state = rf_state_from(in);
    rf_io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "should_redact_assignment") == 0) {
        ret = rf_should_redact_assignment(&io, json_get_str(in, "key", ""),
            json_get_str(in, "value", ""), json_get_bool(in, "check_keyword", true));
    } else if (strcmp(func, "json_affixed_key_secret") == 0) {
        ret = rf_json_affixed_key_secret(&io, json_get_str(in, "key", ""),
            json_get_str(in, "value", ""));
    } else if (strcmp(func, "is_sensitive_url_param") == 0) {
        ret = rf_is_sensitive_url_param(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "redact_log_urls") == 0) {
        ret = rf_redact_log_urls(&io, json_obj_get(in, "text"));
    } else if (strcmp(func, "redact_for_egress") == 0) {
        ret = rf_redact_for_egress(&io, rf_str_or_null(in, "text"),
            json_get_bool(in, "urls", false), json_get_bool(in, "keep_phone_numbers", false));
    } else if (strcmp(func, "redact_code_output") == 0) {
        ret = rf_redact_code_output(&io, json_get_str(in, "output", ""),
            json_get_bool(in, "force", false));
    } else if (strcmp(func, "redact_derived_text") == 0) {
        ret = rf_redact_derived_text(&io, json_obj_get(in, "text"));
    } else if (strcmp(func, "redact_enabled") == 0) {
        ret = rf_redact_enabled(&io);
    } else if (strcmp(func, "command_reads_secret_file") == 0) {
        ret = rf_command_reads_secret_file(&io, rf_str_or_null(in, "command"));
    } else if (strcmp(func, "is_secret_file_arg") == 0) {
        ret = rf_is_secret_file_arg(&io, json_get_str(in, "arg", ""));
    } else if (strcmp(func, "note_env_policy") == 0) {
        rf_note_env_policy(&io, json_get_str(in, "home_key", ""), json_obj_get(in, "raw"));
        ret = json_null();
    } else if (strcmp(func, "note_config_policy") == 0) {
        rf_note_config_policy(&io, json_get_str(in, "home_key", ""), json_obj_get(in, "raw"));
        ret = json_null();
    } else if (strcmp(func, "key_name_words") == 0) {
        ret = rf_key_name_words(json_get_str(in, "key", ""));
    } else if (strcmp(func, "is_qualified_key_name") == 0) {
        ret = json_bool(rf_is_qualified_key_name(json_get_str(in, "key", "")));
    } else if (strcmp(func, "is_python_repr_secret_key") == 0) {
        ret = json_bool(rf_is_python_repr_secret_key(json_get_str(in, "key", "")));
    } else if (strcmp(func, "is_secret_field_name") == 0) {
        ret = json_bool(rf_is_secret_field_name(json_obj_get(in, "key")));
    } else if (strcmp(func, "decode_url_param_name") == 0) {
        char *r = rf_decode_url_param_name(json_get_str(in, "name", ""));
        ret = json_string(r ? r : ""); free(r);
    } else if (strcmp(func, "canonical_url_param_name") == 0) {
        char *r = rf_canonical_url_param_name(json_get_str(in, "name", ""));
        ret = json_string(r ? r : ""); free(r);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = rf_make_out(&io, ret);
    json_free(sc.cursors);
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = rf_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}
