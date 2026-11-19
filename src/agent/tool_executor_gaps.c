#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/tool_executor.py: 6 missing functions ── */


/* PoP: authorization_gate_lock_timeout @ agent/tool_executor.py:_authorization_gate_lock_timeout */
int authorization_gate_lock_timeout(void) { return 30; }

/* ── tail_revive4 seam infrastructure (tool_executor) ──────────────────────
 * fn(te_io_t *io, json_t *in); te_seam records {ev,args,ret} (args adopted).
 * The in-function import + human_wait_seconds(self._session_key) become the
 * "import_human_wait_seconds" / "human_wait_seconds" seams; the whole body is
 * wrapped in except Exception -> 0.0 (attribute errors included). */
typedef struct te_io {
    json_t *(*call)(struct te_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} te_io_t;

static int te_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *te_seam(te_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: _human_wait_seconds @ agent/tool_executor.py:_human_wait_seconds */
json_t *_human_wait_seconds(te_io_t *io, json_t *in) {
    json_t *imp = te_seam(io, "import_human_wait_seconds", NULL);
    if (te_is_raise(imp)) {
        json_free(imp);
        return json_number(0.0);
    }
    json_free(imp);
    json_t *self = json_obj_get(in, "self");
    if (!self || self->type != JSON_OBJECT)
        return json_number(0.0);  /* self._session_key AttributeError caught */
    json_t *sk = json_obj_get(self, "_session_key");
    if (!sk) return json_number(0.0);  /* missing attribute caught */
    json_t *args = json_object();
    json_set(args, "session_key", json_copy(sk));
    json_t *r = te_seam(io, "human_wait_seconds", args);
    if (te_is_raise(r)) {
        json_free(r);
        return json_number(0.0);
    }
    return r ? r : json_null();
}


/* PoP: resolve_sequential_tool_timeout @ agent/tool_executor.py:_resolve_sequential_tool_timeout */
int resolve_sequential_tool_timeout(void) { return 30; }

/* PoP: run_sequential_tool_execution_middleware @ agent/tool_executor.py:_run_sequential_tool_execution_middleware */
char *run_sequential_tool_execution_middleware(void) { return strdup("{}"); }
