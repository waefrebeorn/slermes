#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/lsp/client.py: 3 missing functions ── */

/* PoP: _folder @ agent/lsp/client.py:_folder */
char *_folder(void) { return strdup(""); }

/* ── tail_revive4 seam infrastructure (lsp_client) ─────────────────────────
 * fn(lc_io_t *io, json_t *in); lc_seam records {ev,args,ret} (args adopted).
 * _LIVE_STATES = {"starting","running"}. in.self models the LSPClient:
 * _proc {_returncode absent/null == None, stdin} and _reader_task; the
 * is_closing()/done() methods become "stdin_is_closing"/"reader_done" seams,
 * invoked only when the short-circuit chain reaches them. */
typedef struct lc_io {
    json_t *(*call)(struct lc_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} lc_io_t;

static int lc_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int lc_truthy(const json_t *v) {
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

static const char *lc_typename(const json_t *v) {
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

static json_t *lc_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *lc_attr_error(const json_t *v, const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", lc_typename(v), attr);
    return lc_raise("AttributeError", msg);
}

static json_t *lc_seam(lc_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: connection_is_open @ agent/lsp/client.py:_connection_is_open */
json_t *connection_is_open(lc_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    if (!self || self->type != JSON_OBJECT) return lc_attr_error(self, "_state");
    json_t *state = json_obj_get(self, "_state");
    if (state && (state->type == JSON_ARRAY || state->type == JSON_OBJECT)) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot use '%s' as a set element (unhashable type: '%s')",
                 lc_typename(state), lc_typename(state));
        return lc_raise("TypeError", msg);
    }
    if (!state || state->type != JSON_STRING) return json_bool(false);
    const char *s = state->str_val ? state->str_val : "";
    if (strcmp(s, "starting") != 0 && strcmp(s, "running") != 0) return json_bool(false);
    json_t *proc = json_obj_get(self, "_proc");
    if (!proc || proc->type == JSON_NULL) return json_bool(false);
    if (proc->type != JSON_OBJECT) return lc_attr_error(proc, "returncode");
    json_t *rc = json_obj_get(proc, "returncode");
    if (rc && rc->type != JSON_NULL) return json_bool(false);  /* returncode is None */
    json_t *stdin_ = json_obj_get(proc, "stdin");
    if (!stdin_ || stdin_->type == JSON_NULL) return json_bool(false);
    if (stdin_->type != JSON_OBJECT) return lc_attr_error(stdin_, "is_closing");
    json_t *closing = lc_seam(io, "stdin_is_closing", NULL);
    if (lc_is_raise(closing)) return closing;
    int is_closing = lc_truthy(closing);
    json_free(closing);
    if (is_closing) return json_bool(false);
    json_t *reader = json_obj_get(self, "_reader_task");
    if (!reader || reader->type == JSON_NULL) return json_bool(false);
    if (reader->type != JSON_OBJECT) return lc_attr_error(reader, "done");
    json_t *done = lc_seam(io, "reader_done", NULL);
    if (lc_is_raise(done)) return done;
    int is_done = lc_truthy(done);
    json_free(done);
    return json_bool(is_done ? false : true);
}

