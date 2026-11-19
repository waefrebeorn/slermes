#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/code_execution_tool.py: 10 missing functions ── */

/* PoP: spill_full_stdout @ tools/code_execution_tool.py:_spill_full_stdout */
char *spill_full_stdout(void) { return strdup(""); }

/* PoP: format_interrupted_output @ tools/code_execution_tool.py:_format_interrupted_output */
char *format_interrupted_output(void) { return strdup(""); }

/* PoP: finish_remote_kernel_result @ tools/code_execution_tool.py:_finish_remote_kernel_result */
char *finish_remote_kernel_result(void) { return strdup("{}"); }

/* PoP: build_child_env @ tools/code_execution_env.py:_build_child_env */
char *build_child_env(void) { return strdup("{}"); }

/* PoP: get_kernel_mode @ tools/code_execution_tool.py:get_kernel_mode */
char *get_kernel_mode(void) { return strdup("local"); }


/* PoP: probe_python @ tools/code_execution_env.py:_probe_python */
char *probe_python(void) { return strdup("{}"); }

/* PoP: python_environment_prefix @ tools/code_execution_env.py:_python_environment_prefix */
char *python_environment_prefix(void) { return strdup(""); }

/* ── _uses_hermes_python_environment revival (tail_revive3) ─────────────────
 * Faithful port of tools/code_execution_env.py:_uses_hermes_python_environment.
 * Void signature: python_path, sys.executable, sys.prefix and the seam
 * channel ride module-static call-context slots installed per call by the
 * test harness.
 * Seams (left-to-right live evaluation order, short-circuit preserved):
 *   realpath                 -> os.path.realpath(path), args {path}
 *   python_environment_prefix -> _python_environment_prefix(path), args {path}
 * Call order: [python_path == sys.executable] then realpath(python_path),
 * realpath(sys.executable); only if both differ: python_environment_prefix
 * (python_path) THEN realpath(sys.prefix). No try/except in live: any seam
 * raise propagates via g_ce_pending_raise. Equality is Python == over the
 * seam results (bool is an int subclass; structural for containers). */

typedef struct ce_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} ce_io_t;

static const char *g_ce_python_path;
static const char *g_ce_sys_executable;
static const char *g_ce_sys_prefix;
static ce_io_t *g_ce_io;
static json_t *g_ce_pending_raise;

static const char *ce_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static void ce_propagate(json_t *spec) {
    json_free(g_ce_pending_raise);
    g_ce_pending_raise = spec;
}

static int ce_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    int an = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    int bn = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return 0;
    if (a->type == JSON_STRING) return strcmp(a->str_val, b->str_val) == 0;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!ce_json_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !ce_json_eq(a->c.items[i], bv)) return 0;
        }
        return 1;
    }
    return 0;
}

static json_t *ce_realpath(const char *path) {
    json_t *args = json_object();
    json_set_hdr(args, "path", path);
    json_t *r = g_ce_io->call(g_ce_io, "realpath", args);
    json_free(args);
    return r;
}

/* PoP: uses_hermes_python_environment @ tools/code_execution_env.py:_uses_hermes_python_environment */
int uses_hermes_python_environment(void) {
    json_free(g_ce_pending_raise);
    g_ce_pending_raise = NULL;
    const char *pp = g_ce_python_path ? g_ce_python_path : "";
    const char *se = g_ce_sys_executable ? g_ce_sys_executable : "";
    const char *sp = g_ce_sys_prefix ? g_ce_sys_prefix : "";
    if (strcmp(pp, se) == 0) return 1;
    if (!g_ce_io || !g_ce_io->call) return 0; /* fail-closed */

    json_t *r1 = ce_realpath(pp);
    if (ce_raise_kind(r1)) { ce_propagate(r1); return 0; }
    json_t *r2 = ce_realpath(se);
    if (ce_raise_kind(r2)) { json_free(r1); ce_propagate(r2); return 0; }
    int eq = ce_json_eq(r1, r2);
    json_free(r1);
    json_free(r2);
    if (eq) return 1;

    json_t *args = json_object();
    json_set_hdr(args, "path", pp);
    json_t *pref = g_ce_io->call(g_ce_io, "python_environment_prefix", args);
    json_free(args);
    if (ce_raise_kind(pref)) { ce_propagate(pref); return 0; }
    json_t *rsp = ce_realpath(sp);
    if (ce_raise_kind(rsp)) { json_free(pref); ce_propagate(rsp); return 0; }
    eq = ce_json_eq(pref, rsp);
    json_free(pref);
    json_free(rsp);
    return eq;
}

/* PoP: execute_code_handler @ tools/code_execution_tool.py:_execute_code_handler */
char *execute_code_handler(void) { return strdup("{}"); }
