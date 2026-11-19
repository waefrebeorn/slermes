/* t_port_update_lock_flow2.c — harness for the update-lock UNCLAIMED-remainder
 * FLOW port (src/cli/update_lock_flow2.c).
 *
 * Self-contained: #includes the port .c by AREA-QUALIFIED name. Reads JSON
 * LINES from stdin (tests/fixtures/update_lock_flow2.jsonl):
 *   {"func": <name>, "in": {...}}
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * call sequence ("trace": [{"name","args"}]), the final return ("ret"), and —
 * for the state-mutating functions — the mutated self/held model ("state").
 * The echoed "in" is the ORIGINAL input (the oracle re-drives live from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim), a LIST consumed in call order (a queue),
 * or {"__script__": true, "ret": <value>, "raise": {type,message,errno?}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/update_lock_flow2.c"

typedef struct {
    json_t *io_map;
    json_t *cursors;
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER) json_set(e, "errno", json_copy(en));
    return e;
}

static json_t *entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(ul2_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        json_t *e = json_object();
        json_set_hdr(e, "__raise__", "RuntimeError");
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set_hdr(e, "message", msg);
        return e;
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            json_t *e = json_object();
            json_set_hdr(e, "__raise__", "RuntimeError");
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            json_set_hdr(e, "message", msg);
            return e;
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return entry_result(json_get(entry, idx));
    }
    return entry_result(entry);
}

typedef json_t *(*ul2_fn_t)(ul2_io_t *, json_t *);
typedef struct { const char *func; ul2_fn_t fn; bool stateful; } ul2_case_t;

static const ul2_case_t CASES[] = {
    { "_git_common_dir", ul2_git_common_dir, false },
    { "checkout_lock_path", ul2_checkout_lock_path, false },
    { "incarnation_live", ul2_incarnation_live, false },
    { "_real_world", ul2_real_world, false },
    { "_identity_line", ul2_identity_line, false },
    { "_is_runtime_host", ul2_is_runtime_host, false },
    { "_read_bytes", ul2_read_bytes, false },
    { "_read_marker", ul2_read_marker, false },
    { "_tmp_sibling", ul2_tmp_sibling, false },
    { "_tmp_writer_gone", ul2_tmp_writer_gone, false },
    { "marker_mutex_path", ul2_marker_mutex_path, false },
    { "_sweep_dead_tmp_siblings", ul2_sweep_dead_tmp_siblings, false },
    { "_compare_and_swap", ul2_compare_and_swap, false },
    { "legacy_profile_claims", ul2_legacy_profile_claims, false },
    { "_lock_holder", ul2_lock_holder, false },
    { "_lock_with_contention_wait", ul2_lock_with_contention_wait, false },
    { "_held_by_our_windows_ancestor", ul2_held_by_our_windows_ancestor, false },
    { "_release_checkout", ul2_release_checkout, true },
    { "checkout_lock_fds", ul2_checkout_lock_fds, false },
    { "custody_spawn_kwargs", ul2_custody_spawn_kwargs, false },
    { "holds_checkout_lock", ul2_holds_checkout_lock, false },
    { "update_in_progress", ul2_update_in_progress, false },
    { "checkout_lock_held", ul2_checkout_lock_held, false },
    { "_marker_unwritable", ul2_marker_unwritable, false },
    { "_release_marker", ul2_release_marker, false },
    { "acquire_checkout", ul2_acquire_checkout, true },
    { "_checkout_held_elsewhere", ul2_checkout_held_elsewhere, false },
    { "_is_partner", ul2_is_partner, false },
    { "_adopt_or_refuse", ul2_adopt_or_refuse, true },
    { "_drop_checkout", ul2_drop_checkout, true },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
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
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const ul2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            ul2_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            if (tc->stateful) {
                json_t *state = json_object();
                json_t *self = json_obj_get(pin, "self");
                json_t *held = json_obj_get(pin, "held");
                json_set(state, "self", self ? json_copy(self) : json_null());
                json_set(state, "held", held ? json_copy(held) : json_null());
                json_set(out, "state", state);
            }
            json_free(pin);
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
        json_free(rec);
    }
    return 0;
}
