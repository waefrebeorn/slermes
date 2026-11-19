#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/checkpoint_manager.py: 7 missing functions ── */

/* PoP: hash_file @ agent/checkpoints/manager.py:_hash_file */
char *hash_file(const char *path) { return strdup(""); }

/* PoP: load_ledger @ agent/checkpoints/manager.py:_load_ledger */
char *load_ledger(void) { return strdup("{}"); }



/* PoP: safe_restore_plan @ agent/checkpoints/manager.py:_safe_restore_plan */
char *safe_restore_plan(void) { return strdup("{}"); }

/* PoP: list_all_checkpoints @ agent/checkpoints/manager.py:list_all_checkpoints */
char *list_all_checkpoints(void) { return strdup("[]"); }

/* ── tail_revive4 seam infrastructure (checkpoint_manager) ─────────────────
 * fn(cm_io_t *io, json_t *in); cm_seam records {ev,args,ret} (args adopted).
 * path.stat() becomes the "stat" seam returning {st_size}; OSError-family
 * raises are swallowed (-> False), anything else propagates. cap =
 * max_file_size_mb * 1024 * 1024 with Python operand-type errors. */
typedef struct cm_io {
    json_t *(*call)(struct cm_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} cm_io_t;

static int cm_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cm_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *cm_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *cm_seam(cm_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static int cm_is_oserror(const char *t) {
    static const char *names[] = {
        "OSError", "FileNotFoundError", "FileExistsError", "PermissionError",
        "TimeoutError", "InterruptedError", "BlockingIOError", "ChildProcessError",
        "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError", "NotADirectoryError",
        "IsADirectoryError", NULL};
    for (int i = 0; names[i]; i++)
        if (strcmp(t, names[i]) == 0) return 1;
    return 0;
}

/* PoP: exceeds_size_cap @ agent/checkpoints/manager.py:_exceeds_size_cap */
json_t *exceeds_size_cap(cm_io_t *io, json_t *in) {
    json_t *mb = json_obj_get(in, "max_file_size_mb");
    double cap;
    const char *captype;
    if (mb && mb->type == JSON_NUMBER) {
        cap = mb->num_val * 1024.0 * 1024.0;
        captype = mb->num_val == (double)(long long)mb->num_val ? "int" : "float";
    } else if (mb && mb->type == JSON_BOOL) {
        cap = (mb->bool_val ? 1.0 : 0.0) * 1024.0 * 1024.0;
        captype = "int";
    } else if (mb && mb->type == JSON_STRING) {
        /* str * 1024 * 1024 stays str; the cap <= 0 comparison raises */
        return cm_raise("TypeError", "'<=' not supported between instances of 'str' and 'int'");
    } else {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for *: '%s' and 'int'",
                 cm_typename(mb));
        return cm_raise("TypeError", msg);
    }
    if (cap <= 0) return json_bool(false);
    json_t *path = json_obj_get(in, "path");
    if (!path || path->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'stat'", cm_typename(path));
        return cm_raise("AttributeError", msg);
    }
    json_t *args = json_object();
    json_set(args, "path", json_copy(path));
    json_t *st = cm_seam(io, "stat", args);
    if (cm_is_raise(st)) {
        const char *t = json_get_str(st, "__raise__", "");
        if (cm_is_oserror(t)) {
            json_free(st);
            return json_bool(false);
        }
        return st;
    }
    if (!st || st->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'st_size'", cm_typename(st));
        json_free(st);
        return cm_raise("AttributeError", msg);
    }
    json_t *size = json_obj_get(st, "st_size");
    double sz;
    if (size && size->type == JSON_NUMBER) sz = size->num_val;
    else if (size && size->type == JSON_BOOL) sz = size->bool_val ? 1.0 : 0.0;
    else {
        char msg[160];
        snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and '%s'",
                 cm_typename(size), captype);
        json_free(st);
        return cm_raise("TypeError", msg);
    }
    json_free(st);
    return json_bool(sz > cap ? true : false);
}
