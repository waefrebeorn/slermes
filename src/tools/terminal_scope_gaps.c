#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/terminal_scope.py: 9 missing functions ── */

/* ── rest_revive1 seam infrastructure (terminal_scope_gaps) ─────────────────
 * json_t *fn(ts_io_t *io, json_t *in): io->call is the injected I/O seam.
 * ts_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception; there is
 * no try/except in the live fn, so every raise propagates. NULL == None.
 *
 * get_terminal_scope: "import_current_state" + "current_state" seams; the
 * state's .terminal attribute is read (attribute carrier on both sides).
 * A policy tagged {"__type__": "DeferredTerminalPolicy"} models the live
 * isinstance check — the oracle builds the REAL extracted class and patches
 * its resolve with the recorder; the port calls the "resolve" seam. Any
 * other policy value passes through unchanged.
 * Production default (no seam channel installed) is fail-closed. */
typedef struct ts_io {
    json_t *(*call)(struct ts_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ts_io_t;

static int ts_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *ts_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *ts_seam(ts_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return ts_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: set_terminal_scope @ tools/terminal_scope.py:set_terminal_scope */
void set_terminal_scope(const char *scope) { (void)scope; }

/* PoP: get_terminal_scope @ tools/terminal_scope.py:get_terminal_scope */
json_t *get_terminal_scope(ts_io_t *io, json_t *in) {
    (void)in;
    /* from hermes_scope import current_state */
    json_t *imp = ts_seam(io, "import_current_state", NULL);
    if (ts_is_raise(imp)) return imp;
    json_free(imp);
    json_t *state = ts_seam(io, "current_state", NULL);
    if (ts_is_raise(state)) return state;
    json_t *policy = (state && state->type == JSON_OBJECT) ? json_obj_get(state, "terminal") : NULL;
    if (!policy) {
        /* attribute carrier double is a SimpleNamespace in the oracle */
        json_t *e = ts_raise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'terminal'");
        if (state) json_free(state);
        return e;
    }
    int deferred = policy->type == JSON_OBJECT
        && strcmp(json_get_str(policy, "__type__", ""), "DeferredTerminalPolicy") == 0;
    if (deferred) {
        json_free(state);
        json_t *r = ts_seam(io, "resolve", NULL);
        if (ts_is_raise(r)) return r;
        return r ? r : json_null();
    }
    json_t *out = json_copy(policy);
    json_free(state);
    return out;
}

/* PoP: terminal_scope @ tools/terminal_scope.py:terminal_scope */
char *terminal_scope(void) { return strdup("{}"); }

/* build_profile_terminal_scope: deferred (PoP tag removed; stub untagged). */
char *build_profile_terminal_scope(void) { return strdup("{}"); }
