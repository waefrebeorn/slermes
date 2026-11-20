/* t_port_doctor_flow2.c — harness for the doctor_* sibling remainder FLOW port.
 *
 * Reads JSON LINES {"func": <name>, "in": {...}}; emits JSON LINES
 * {"func","in","out":{"ret","trace","state","stdout","stderr","rows"}}.
 * "trace" = ordered seam-name sequence; "state" = mutated caller-held state
 * (restricted to keys the case seeded); stdout/stderr captured print text;
 * "rows" captured report rows.
 *
 * Scripted I/O in in.io (seam -> {"ret","raise","state"} or a queue LIST).
 * AUTO cosmetic seams (no script, never fail closed): "print", "color", and
 * the report primitives "check_ok"/"check_warn"/"check_fail"/"check_info"/
 * "_section". Every other seam is FAIL-CLOSED: a missing script raises
 * RuntimeError("unscripted seam: <name>").
 */
#include "hermes_json.h"
#include "cli/doctor_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } df2_script_ctx_t;

static json_t *df2_script_next(df2_script_ctx_t *sc, const char *name) {
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
static void df2_apply_state(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}
static const char *df2_row_kind(const char *name) {
    if (strcmp(name, "check_ok") == 0) return "ok";
    if (strcmp(name, "check_warn") == 0) return "warn";
    if (strcmp(name, "check_fail") == 0) return "fail";
    if (strcmp(name, "check_info") == 0) return "info";
    return "section";
}
static json_t *df2_dispatch(df2_io_t *io, const char *name, json_t *args) {
    df2_script_ctx_t *sc = (df2_script_ctx_t *)io->ctx;
    if (strcmp(name, "print") == 0) {
        const char *msg = args ? json_get_str(args, "text", "") : "";
        const char *file = args ? json_get_str(args, "file", "") : "";
        json_t *cap = (strcmp(file, "stderr") == 0) ? io->cap_stderr : io->cap_stdout;
        if (cap && cap->type == JSON_ARRAY) json_append(cap, json_string(msg));
        return json_null();
    }
    if (strcmp(name, "color") == 0)
        return json_string(args ? json_get_str(args, "text", "") : "");
    if (strcmp(name, "check_ok") == 0 || strcmp(name, "check_warn") == 0 ||
        strcmp(name, "check_fail") == 0 || strcmp(name, "check_info") == 0 ||
        strcmp(name, "_section") == 0) {
        json_t *row = json_object();
        json_set(row, "kind", json_string(df2_row_kind(name)));
        json_set(row, "text", json_string(args ? json_get_str(args, "text", "") : ""));
        json_set(row, "detail", json_string(args ? json_get_str(args, "detail", "") : ""));
        if (io->cap_rows && io->cap_rows->type == JSON_ARRAY) json_append(io->cap_rows, row);
        else json_free(row);
        return json_null();
    }
    json_t *result = df2_script_next(sc, name);
    if (!result) {
        char *m = df2_join("unscripted seam: ", name);
        json_t *r = df2_make_raise("RuntimeError", m); free(m); return r;
    }
    df2_apply_state(io->state, result);
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *out = json_object(); json_set(out, "__raise__", json_copy(raise)); return out;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}
static void df2_io_init(df2_io_t *io, df2_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = df2_dispatch;
    io->trace = json_array(); io->state = state;
    io->cap_stdout = json_array(); io->cap_stderr = json_array(); io->cap_rows = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io"); sc->cursors = json_object();
}
static json_t *df2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}
static json_t *df2_state_proj(json_t *state, json_t *in) {
    json_t *out = json_object();
    json_t *seed = json_obj_get(in, "state");
    if (seed && seed->type == JSON_OBJECT)
        for (size_t i = 0; i < seed->c.count; i++) {
            json_t *v = json_obj_get(state, seed->c.keys[i]);
            json_set(out, seed->c.keys[i], v ? json_copy(v) : json_null());
        }
    return out;
}

static json_t *df2_run_case(const char *func, json_t *in) {
    df2_io_t io; df2_script_ctx_t sc;
    json_t *state = df2_state_from(in);
    df2_io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    const json_t *ok = json_obj_get(in, "ok"), *bad = json_obj_get(in, "bad");

    if (strcmp(func, "plural") == 0) {
        ret = df2_plural(json_get_num(in, "n", 0));
    } else if (strcmp(func, "setup_gated") == 0) {
        ret = df2_setup_gated(json_obj_get(in, "item"));
    } else if (strcmp(func, "pm_tool_path") == 0) {
        ret = df2_pm_tool_path(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "pm_package_for_command") == 0) {
        ret = df2_pm_package_for_command(&io, json_get_str(in, "command", ""));
    } else if (strcmp(func, "require") == 0) {
        ret = df2_require(&io, json_obj_get(in, "cond"), ok, bad, json_get_str(in, "issue", ""));
    } else if (strcmp(func, "chromium_not_needed_reason") == 0) {
        ret = df2_chromium_not_needed_reason(&io);
    } else if (strcmp(func, "failing_boot_migration") == 0) {
        ret = df2_failing_boot_migration(&io);
    } else if (strcmp(func, "drift_deprecations") == 0) {
        json_t *cp = json_obj_get(in, "config_path");
        ret = df2_drift_deprecations(&io, cp && cp->type == JSON_STRING ? cp->str_val : NULL);
    } else if (strcmp(func, "drift_structure") == 0) {
        ret = df2_drift_structure(&io);
    } else if (strcmp(func, "provider_has_credentials") == 0) {
        ret = df2_provider_has_credentials(&io, json_get_str(in, "provider", ""));
    } else if (strcmp(func, "python_repair_hint") == 0) {
        ret = df2_python_repair_hint(&io);
    } else if (strcmp(func, "unreadable_reason") == 0) {
        ret = df2_unreadable_reason(&io, json_get_str(in, "path", ""));
    } else {
        ret = json_object(); json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    if (df2_raised(ret)) {
        json_t *spec = json_copy(df2_raise_spec(ret)); json_free(ret);
        ret = json_object(); json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", df2_state_proj(state, in));
    json_set(out, "stdout", json_copy(io.cap_stdout));
    json_set(out, "stderr", json_copy(io.cap_stderr));
    json_set(out, "rows", json_copy(io.cap_rows));
    json_free(io.trace); json_free(io.cap_stdout); json_free(io.cap_stderr);
    json_free(io.cap_rows); json_free(sc.cursors); json_free(state);
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
        json_t *out = df2_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser); json_free(line_out); json_free(rec);
    }
    return 0;
}
