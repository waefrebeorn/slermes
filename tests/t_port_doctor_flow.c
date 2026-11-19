/* t_port_doctor_flow.c — harness for the doctor-diagnostic UNCLAIMED FLOW
 * port (src/cli/doctor_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","stdout","stderr","rows"}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated caller-held
 * mirror (restricted to the keys the case seeded); "stdout"/"stderr" captured
 * print text; "rows" the captured report-primitive rows (check_ok/warn/fail/
 * info/_section).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise","import_raise"}
 * or a queue LIST). AUTO cosmetic seams (no script, never fail closed): "print",
 * "color" (identity), and the report primitives "check_ok"/"check_warn"/
 * "check_fail"/"check_info"/"_section" (capture a row). Every other seam is
 * FAIL-CLOSED: a missing script raises RuntimeError("unscripted seam: <name>").
 */
#include "hermes_json.h"
#include "cli/doctor_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} doc_script_ctx_t;

static json_t *doc_script_next(doc_script_ctx_t *sc, const char *name) {
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

static void doc_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static const char *doc_row_kind(const char *name) {
    if (strcmp(name, "check_ok") == 0) return "ok";
    if (strcmp(name, "check_warn") == 0) return "warn";
    if (strcmp(name, "check_fail") == 0) return "fail";
    if (strcmp(name, "check_info") == 0) return "info";
    return "section";   /* _section */
}

static json_t *doc_dispatch(doc_flow_io_t *io, const char *name, json_t *args) {
    doc_script_ctx_t *sc = (doc_script_ctx_t *)io->ctx;
    /* AUTO cosmetic seams. */
    if (strcmp(name, "print") == 0) {
        const char *msg = args ? json_get_str(args, "text", "") : "";
        const char *file = args ? json_get_str(args, "file", "") : "";
        json_t *cap = (strcmp(file, "stderr") == 0) ? io->cap_stderr : io->cap_stdout;
        if (cap && cap->type == JSON_ARRAY) json_append(cap, json_string(msg));
        return json_null();
    }
    if (strcmp(name, "color") == 0) {
        const char *text = args ? json_get_str(args, "text", "") : "";
        return json_string(text);
    }
    if (strcmp(name, "check_ok") == 0 || strcmp(name, "check_warn") == 0 ||
        strcmp(name, "check_fail") == 0 || strcmp(name, "check_info") == 0 ||
        strcmp(name, "_section") == 0) {
        json_t *row = json_object();
        json_set(row, "kind", json_string(doc_row_kind(name)));
        json_set(row, "text", json_string(args ? json_get_str(args, "text", "") : ""));
        json_set(row, "detail", json_string(args ? json_get_str(args, "detail", "") : ""));
        if (io->cap_rows && io->cap_rows->type == JSON_ARRAY) json_append(io->cap_rows, row);
        else json_free(row);
        return json_null();
    }
    /* SCRIPTED fail-closed seams. */
    json_t *result = doc_script_next(sc, name);
    if (!result) {
        char *m = doc_join("unscripted seam: ", name);
        json_t *r = doc_make_raise("RuntimeError", m);
        free(m);
        return r;
    }
    doc_apply_state_effects(io->state, result);
    if (result->type == JSON_OBJECT) {
        json_t *imp = json_obj_get(result, "import_raise");
        if (imp && imp->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(imp));
            json_set(out, "__untrace__", json_bool(true));
            return out;
        }
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(raise));
            return out;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}

static void doc_io_init(doc_flow_io_t *io, doc_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = doc_dispatch;
    io->trace = json_array();
    io->state = state;
    io->cap_stdout = json_array();
    io->cap_stderr = json_array();
    io->cap_rows = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *doc_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *doc_state_proj(json_t *state, json_t *in) {
    json_t *out = json_object();
    json_t *seed = json_obj_get(in, "state");
    if (seed && seed->type == JSON_OBJECT) {
        for (size_t i = 0; i < seed->c.count; i++) {
            json_t *v = json_obj_get(state, seed->c.keys[i]);
            json_set(out, seed->c.keys[i], v ? json_copy(v) : json_null());
        }
    }
    return out;
}

static json_t *doc_run_case(const char *func, json_t *in) {
    doc_flow_io_t io;
    doc_script_ctx_t sc;
    json_t *state = doc_state_from(in);
    doc_io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "mark") == 0) {
        json_t *cl = doc_mark(&io, json_get_str(in, "glyph", ""), json_get_str(in, "col", ""));
        ret = doc_mark_call(&io, cl, json_get_str(in, "text", ""), json_get_str(in, "detail", ""));
        json_free(cl);
    } else if (strcmp(func, "check_bool") == 0) {
        ret = doc_check_bool(&io, json_obj_get(in, "cond"), json_obj_get(in, "ok"),
                             json_obj_get(in, "bad"), json_get_bool(in, "fail", false));
    } else if (strcmp(func, "finding_merge") == 0) {
        ret = doc_finding_merge(&io, json_obj_get(in, "other"));
    } else if (strcmp(func, "ensure_dir") == 0) {
        ret = doc_ensure_dir(&io, json_get_bool(in, "should_fix", false), json_get_str(in, "path", ""),
                             json_get_str(in, "exists_msg", ""), json_get_str(in, "created_msg", ""),
                             json_get_str(in, "missing_msg", ""));
    } else if (strcmp(func, "warn_on_error") == 0) {
        ret = doc_warn_on_error(&io, json_get_str(in, "text", ""), json_get_str(in, "detail", "({e})"),
                                json_get_str(in, "report", "check_warn"), json_get_str(in, "body", "body"));
    } else if (strcmp(func, "login_row") == 0) {
        ret = doc_login_row(&io, json_get_str(in, "label", ""), json_obj_get(in, "status"),
                            json_get_str(in, "ok_detail", "(logged in)"), json_get_bool(in, "show_error", false));
    } else if (strcmp(func, "print_summary") == 0) {
        ret = doc_print_summary(&io, json_get_bool(in, "should_fix", false));
    } else if (strcmp(func, "ack_advisory") == 0) {
        ret = doc_ack_advisory(&io, json_get_str(in, "ack_target", ""), json_obj_get(in, "advisory_ids"),
                               json_get_str(in, "dhh", ""));
    } else if (strcmp(func, "check_auth_providers") == 0) {
        ret = doc_check_auth_providers(&io, json_get_bool(in, "should_fix", false));
    } else if (strcmp(func, "check_api_connectivity") == 0) {
        ret = doc_check_api_connectivity(&io, json_get_bool(in, "should_fix", false));
    } else if (strcmp(func, "run_doctor") == 0) {
        ret = doc_run_doctor(&io, json_obj_get(in, "args"), json_obj_get(in, "checks"),
                             json_obj_get(in, "advisory_ids"), json_get_str(in, "dhh", ""));
    } else if (strcmp(func, "maybe_run_live_checks") == 0) {
        ret = doc_maybe_run_live_checks(&io, json_obj_get(in, "args"));
    } else if (strcmp(func, "keyed_probe") == 0) {
        ret = doc_keyed_probe(&io, json_get_str(in, "name", ""), json_get_str(in, "url", ""),
                              json_get_str(in, "env_var", ""), json_get_str(in, "scheme", ""),
                              json_get_num(in, "timeout", 10.0));
    } else if (strcmp(func, "probe_audio") == 0) {
        ret = doc_probe_audio(&io, json_get_str(in, "kind", ""), json_obj_get(in, "config"),
                              json_get_num(in, "timeout", 10.0));
    } else if (strcmp(func, "bits") == 0) {
        ret = doc_bits(&io, json_obj_get(in, "pairs"));
    } else if (strcmp(func, "host_gateway_note") == 0) {
        ret = doc_host_gateway_note(&io);
    } else if (strcmp(func, "session_count") == 0) {
        ret = doc_session_count(&io, json_get_str(in, "path", ""));
    } else if (strcmp(func, "endpoint_url") == 0) {
        ret = doc_endpoint_url(&io, json_obj_get(in, "entry"));
    } else if (strcmp(func, "check_retired_session_reset") == 0) {
        ret = doc_check_retired_session_reset(&io, json_get_bool(in, "should_fix", false));
    } else if (strcmp(func, "check_xai_retirement") == 0) {
        ret = doc_check_xai_retirement(&io, json_get_bool(in, "should_fix", false),
                                       json_get_str(in, "migration_url", ""));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    if (doc_raised(ret)) {
        json_t *spec = json_copy(doc_raise_spec(ret));
        json_free(ret);
        ret = json_object();
        json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", doc_state_proj(state, in));
    json_set(out, "stdout", json_copy(io.cap_stdout));
    json_set(out, "stderr", json_copy(io.cap_stderr));
    json_set(out, "rows", json_copy(io.cap_rows));
    json_free(io.trace);
    json_free(io.cap_stdout);
    json_free(io.cap_stderr);
    json_free(io.cap_rows);
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
        json_t *out = doc_run_case(func, in);
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