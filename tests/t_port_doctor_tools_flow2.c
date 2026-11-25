/* t_port_doctor_tools_flow2.c — harness for the doctor_tools.py REMAINDER FLOW port.
 *
 * Reads JSON LINES {"func": <name>, "in": {...}}; emits JSON LINES
 * {"func","in","out":{"ret","trace","state","stdout","stderr","rows"}}.
 * "trace" = ordered seam-name sequence (names only); "state" = mutated
 * caller-held state (restricted to keys the case seeded); stdout/stderr
 * captured print text; "rows" captured report rows.
 *
 * Scripted I/O in in.io (seam -> {"ret","raise","state"} or a queue LIST).
 * AUTO cosmetic seams (no script, never fail closed): report primitives
 * check_ok/check_warn/check_fail/check_info/_section. Every other seam is
 * FAIL-CLOSED: a missing script raises RuntimeError("unscripted seam: <name>").
 */
#include "hermes_json.h"
#include "cli/doctor_tools_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } dt2_script_ctx_t;

static json_t *dt2_script_next(dt2_script_ctx_t *sc, const char *name) {
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
static void dt2_apply_state(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}
static const char *dt2_row_kind(const char *name) {
    if (strcmp(name, "check_ok") == 0) return "ok";
    if (strcmp(name, "check_warn") == 0) return "warn";
    if (strcmp(name, "check_fail") == 0) return "fail";
    if (strcmp(name, "check_info") == 0) return "info";
    return "section";
}
static json_t *dt2_dispatch(dt2_io_t *io, const char *name, json_t *args) {
    dt2_script_ctx_t *sc = (dt2_script_ctx_t *)io->ctx;
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
        json_set(row, "kind", json_string(dt2_row_kind(name)));
        json_set(row, "text", json_string(args ? json_get_str(args, "text", "") : ""));
        json_set(row, "detail", json_string(args ? json_get_str(args, "detail", "") : ""));
        if (io->cap_rows && io->cap_rows->type == JSON_ARRAY) json_append(io->cap_rows, row);
        else json_free(row);
        return json_null();
    }
    json_t *result = dt2_script_next(sc, name);
    if (!result) {
        char *m = dt2_join("unscripted seam: ", name);
        json_t *r = dt2_make_raise("RuntimeError", m); free(m); return r;
    }
    dt2_apply_state(io->state, result);
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
static void dt2_io_init(dt2_io_t *io, dt2_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dt2_dispatch;
    io->trace = json_array(); io->state = state;
    io->cap_stdout = json_array(); io->cap_stderr = json_array(); io->cap_rows = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io"); sc->cursors = json_object();
}
static json_t *dt2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}
static json_t *dt2_state_proj(json_t *state, json_t *in) {
    json_t *out = json_object();
    json_t *seed = json_obj_get(in, "state");
    if (seed && seed->type == JSON_OBJECT)
        for (size_t i = 0; i < seed->c.count; i++) {
            json_t *v = json_obj_get(state, seed->c.keys[i]);
            json_set(out, seed->c.keys[i], v ? json_copy(v) : json_null());
        }
    return out;
}
/* materialize a json array of strings into (const char**, n) */
static const char **dt2_strvec(json_t *arr, size_t *n) {
    if (!arr || arr->type != JSON_ARRAY) { *n = 0; return NULL; }
    *n = arr->c.count;
    const char **v = calloc(arr->c.count ? arr->c.count : 1, sizeof(char*));
    for (size_t i = 0; i < arr->c.count; i++) v[i] = dt2_sval(json_get(arr, i));
    return v;
}

static json_t *dt2_run_case(const char *func, json_t *in) {
    dt2_io_t io; dt2_script_ctx_t sc;
    json_t *state = dt2_state_from(in);
    dt2_io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    const char *name = json_get_str(in, "name", "");
    const char *tool = json_get_str(in, "tool", "");
    const char *cwd = json_get_str(in, "cwd", "");
    const char *label = json_get_str(in, "label", "");
    const char *tenv = json_get_str(in, "terminal_env", "");
    bool should_fix = json_get_bool(in, "should_fix", false);
    bool in_container = json_get_bool(in, "running_in_container", false);
    double timeout = json_get_num(in, "timeout", 10);
    size_t ncmd = 0; const char **cmdv = dt2_strvec(json_obj_get(in, "cmd"), &ncmd);
    size_t nextra = 0; const char **extrav = dt2_strvec(json_obj_get(in, "audit_extra"), &nextra);

    if (strcmp(func, "doctor_tool") == 0) {
        ret = dt2_doctor_tool(&io, name);
    } else if (strcmp(func, "run_ok") == 0) {
        ret = dt2_run_ok(&io, cmdv, ncmd, timeout);
    } else if (strcmp(func, "doctor_web_capability_rows") == 0) {
        ret = dt2_doctor_web_capability_rows(&io);
    } else if (strcmp(func, "check_git_and_rg") == 0) {
        ret = dt2_check_git_and_rg(&io);
    } else if (strcmp(func, "check_docker_backend") == 0) {
        ret = dt2_check_docker_backend(&io, tenv, in_container);
    } else if (strcmp(func, "check_ssh_backend") == 0) {
        ret = dt2_check_ssh_backend(&io);
    } else if (strcmp(func, "check_daytona_backend") == 0) {
        ret = dt2_check_daytona_backend(&io);
    } else if (strcmp(func, "check_vercel_backend") == 0) {
        ret = dt2_check_vercel_backend(&io);
    } else if (strcmp(func, "check_plugin_backend") == 0) {
        ret = dt2_check_plugin_backend(&io, tenv);
    } else if (strcmp(func, "check_terminal_backend") == 0) {
        ret = dt2_check_terminal_backend(&io);
    } else if (strcmp(func, "check_chromium") == 0) {
        ret = dt2_check_chromium(&io, should_fix);
    } else if (strcmp(func, "check_lightpanda") == 0) {
        ret = dt2_check_lightpanda(&io);
    } else if (strcmp(func, "check_node_and_browser") == 0) {
        ret = dt2_check_node_and_browser(&io, should_fix);
    } else if (strcmp(func, "audit_one") == 0) {
        ret = dt2_audit_one(&io, tool, cwd, label, extrav, nextra);
    } else if (strcmp(func, "check_npm_audit") == 0) {
        ret = dt2_check_npm_audit(&io);
    } else if (strcmp(func, "check_tool_availability") == 0) {
        ret = dt2_check_tool_availability(&io);
    } else if (strcmp(func, "warn_partially_stripped_toolsets") == 0) {
        ret = dt2_warn_partially_stripped_toolsets(&io);
    } else {
        ret = json_object(); json_set(ret, "error", json_string("unknown func"));
    }
    free(cmdv); free(extrav);

    json_t *out = json_object();
    if (dt2_raised(ret)) {
        json_t *spec = json_copy(dt2_raise_spec(ret)); json_free(ret);
        ret = json_object(); json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", dt2_state_proj(state, in));
    json_set(out, "stdout", json_copy(io.cap_stdout));
    json_set(out, "stderr", json_copy(io.cap_stderr));
    json_set(out, "rows", json_copy(io.cap_rows));
    json_free(io.trace); json_free(io.cap_stdout); json_free(io.cap_stderr);
    json_free(io.cap_rows); json_free(sc.cursors); json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = dt2_run_case(func, in);
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
