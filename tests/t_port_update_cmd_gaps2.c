/* t_port_update_cmd_gaps2.c — leaf differential harness for the second
 * update_cmd_gaps.c port wave (7 PoP-tagged placeholder bodies).
 *
 * Reads JSON LINES from stdin: {"func": <c_name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret", "trace", "noscript"}}.
 *
 * in.args — direct C arguments.  `token` is a JSON VALUE serialized to text
 *           for the const char* parameter; strings are plain JSON strings.
 * in.ctx  — extra Python kwargs the fixed C signatures cannot carry
 *           (runtimes, owner for write_fleet_restart_pending_marker).
 * in.io   — seam-name -> scripted result.  A single value is reused for every
 *           call; a bare array is consumed per call (clamped at the last
 *           element); {"__const__": v} reuses v verbatim; {"__raise__": kind,
 *           "msg": ...} scripts an exception.  print / logger.debug are
 *           ambient (recorded, never scripted).  A missing script is flagged
 *           in out.noscript (fail-closed).
 */
#include "hermes_cli/update_cmd_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── scripted seam dispatcher (mirrors t_port_update_cmd_revive.c) ── */
typedef struct {
    const json_t *scripts;
    json_t *cursors;
    json_t *trace;
    json_t *noscript;
} ucr_ctx_t;

static void ucr_record(ucr_ctx_t *sc, const char *name, const json_t *args,
                       const json_t *ret) {
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(sc->trace, ev);
}

static json_t *ucr_dispatch(void *ctx, const char *name, const json_t *args) {
    ucr_ctx_t *sc = (ucr_ctx_t *)ctx;
    if (strcmp(name, "print") == 0 || strcmp(name, "logger.debug") == 0) {
        ucr_record(sc, name, args, NULL);
        return NULL;
    }
    const json_t *entry = sc->scripts ? json_obj_get(sc->scripts, name) : NULL;
    if (!entry) {
        json_append(sc->noscript, json_string(name));
        json_t *marker = json_object();
        json_set(marker, "__noscript__", json_string(name));
        ucr_record(sc, name, args, marker);
        json_free(marker);
        json_t *err = json_object();
        json_set(err, "__raise__", json_string("RuntimeError"));
        char *m = (char *)malloc(strlen(name) + 32);
        if (m) {
            sprintf(m, "no script for seam %s", name);
            json_set(err, "msg", json_string(m));
            free(m);
        }
        return err;
    }
    const json_t *val = entry;
    if (entry->type == JSON_OBJECT && json_obj_get(entry, "__const__")) {
        val = json_obj_get(entry, "__const__");
    } else if (entry->type == JSON_ARRAY) {
        size_t n = json_len(entry);
        double cur = json_get_num(sc->cursors, name, 0.0);
        size_t idx = (size_t)cur;
        if (n > 0 && idx >= n) idx = n - 1;
        json_set(sc->cursors, name, json_number(cur + 1.0));
        val = n > 0 ? json_get(entry, idx) : NULL;
    }
    ucr_record(sc, name, args, val);
    return val ? json_copy(val) : NULL;
}

static char *ucr_ser(const json_t *v) {
    if (!v) return strdup("null");
    char *s = json_serialize(v);
    return s ? s : strdup("null");
}

/* ── case runner ── */
static json_t *ucr_run_case(const char *func, const json_t *in) {
    ucr_ctx_t sc;
    sc.scripts = json_obj_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    sc.noscript = json_array();
    update_cmd_gaps_set_io(ucr_dispatch, &sc);
    const json_t *ctx = json_obj_get(in, "ctx");
    update_cmd_gaps_set_context(ctx ? json_copy(ctx) : json_object());
    const json_t *args = json_obj_get(in, "args");
    uc_exc_clear();

    int res = 0;
    int is_void = 0;
    char *t1 = NULL;

    if (strcmp(func, "service_unit_supports_graceful_sigusr1_restart") == 0) {
        res = service_unit_supports_graceful_sigusr1_restart(
                  json_get_str(args, "unit", NULL));
    } else if (strcmp(func, "warn_pending_fleet_restart") == 0) {
        warn_pending_fleet_restart(json_get_bool(args, "startup", 0) ? 1 : 0);
        is_void = 1;
    } else if (strcmp(func, "park_stashed_changes") == 0) {
        park_stashed_changes(json_get_str(args, "stash_ref", NULL));
        is_void = 1;
    } else if (strcmp(func, "post_update_sqlite_runtime_status") == 0) {
        res = post_update_sqlite_runtime_status();
    } else if (strcmp(func, "print_verified_update_completion") == 0) {
        res = print_verified_update_completion(json_get_str(args, "message", NULL));
    } else if (strcmp(func, "write_fleet_restart_pending_marker") == 0) {
        write_fleet_restart_pending_marker(json_get_str(args, "expected_sha", NULL));
        is_void = 1;
    } else if (strcmp(func, "relaunch_stopped_serves") == 0) {
        t1 = ucr_ser(json_obj_get(args, "token"));
        relaunch_stopped_serves(t1);
        is_void = 1;
    } else {
        json_t *err = json_object();
        json_set(err, "error", json_string("unknown func"));
        json_set(err, "trace", sc.trace);
        json_set(err, "noscript", sc.noscript);
        json_free(sc.cursors);
        update_cmd_gaps_set_context(NULL);
        update_cmd_gaps_set_io(NULL, NULL);
        return err;
    }
    free(t1);

    json_t *ret;
    if (uc_exc.active) {
        ret = json_object();
        json_set(ret, "__raise__", json_string(uc_exc.kind));
        json_set(ret, "msg", json_string(uc_exc.msg));
        uc_exc_clear();
    } else if (is_void) {
        ret = json_null();
    } else {
        ret = json_bool(res != 0);
    }

    json_t *out = json_object();
    json_set(out, "ret", ret);
    json_set(out, "trace", sc.trace);
    json_set(out, "noscript", sc.noscript);
    json_free(sc.cursors);
    update_cmd_gaps_set_context(NULL);
    update_cmd_gaps_set_io(NULL, NULL);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char *line = (char *)malloc(1 << 20);
    if (!line) return 1;
    while (fgets(line, 1 << 20, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        const json_t *in = json_obj_get(rec, "in");
        json_t *in_empty = NULL;
        if (!in) { in_empty = json_object(); in = in_empty; }
        json_t *out = ucr_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(in_empty);
        json_free(rec);
    }
    free(line);
    return 0;
}
