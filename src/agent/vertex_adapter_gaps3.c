#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/vertex_adapter.py: 3 missing functions ── */

/* ── rest_revive1 seam infrastructure (vertex_adapter_gaps3) ────────────────
 * json_t *fn(va_io_t *io, json_t *in): io->call is the injected I/O seam.
 * va_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception; there is
 * no try/except in the live fn, so every raise propagates. NULL == None.
 *
 * has_explicit_vertex_config seams: "_resolve_project_override" (truthy
 * short-circuits to True), "_get_secret" (key VERTEX_CREDENTIALS_PATH),
 * "os.path.isfile" and "os.access" (mode os.R_OK == 4) — each call is
 * guarded by the live `and` chain's short-circuit.
 * Production default (no seam channel installed) is fail-closed. */
typedef struct va_io {
    json_t *(*call)(struct va_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} va_io_t;

static int va_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *va_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static int va_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static json_t *va_seam(va_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return va_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: has_explicit_vertex_config @ agent/vertex_adapter.py:has_explicit_vertex_config */
json_t *has_explicit_vertex_config(va_io_t *io, json_t *in) {
    (void)in;
    json_t *ov = va_seam(io, "_resolve_project_override", NULL);
    if (va_is_raise(ov)) return ov;
    if (va_truthy(ov)) {
        json_free(ov);
        return json_bool(true);
    }
    json_free(ov);
    json_t *sargs = json_object();
    json_set_hdr(sargs, "key", "VERTEX_CREDENTIALS_PATH");
    json_t *sa = va_seam(io, "_get_secret", sargs);
    if (va_is_raise(sa)) return sa;
    if (!va_truthy(sa)) {
        if (sa) json_free(sa);
        return json_bool(false);
    }
    json_t *fargs = json_object();
    json_set(fargs, "path", json_copy(sa));
    json_t *isf = va_seam(io, "os.path.isfile", fargs);
    if (va_is_raise(isf)) { json_free(sa); return isf; }
    if (!va_truthy(isf)) {
        json_free(isf);
        json_free(sa);
        return json_bool(false);
    }
    json_free(isf);
    json_t *aargs = json_object();
    json_set(aargs, "path", sa); /* adopts */
    json_set(aargs, "mode", json_number(4)); /* os.R_OK */
    json_t *acc = va_seam(io, "os.access", aargs);
    if (va_is_raise(acc)) return acc;
    int ok = va_truthy(acc);
    json_free(acc);
    return json_bool(ok ? true : false);
}
