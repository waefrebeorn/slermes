#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/desktop_ui.py: 1 missing functions ── */

/* ── rest_revive1 seam infrastructure (desktop_ui_gaps3) ────────────────────
 * json_t *fn(du_io_t *io, json_t *in): io->call is the injected I/O seam.
 * du_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception; the
 * except Exception site swallows plain specs but passes the BaseException
 * trio through. NULL == None.
 *
 * user_enabled: "import_load_config_readonly" + "load_config_readonly"
 * seams; any Exception (import failure, raise, non-dict config .get)
 * returns the default. display must be a dict carrying the setting key
 * (present-null counts as present); the answer is bool(display[setting]).
 * Production default (no seam channel installed) is fail-closed. */
typedef struct du_io {
    json_t *(*call)(struct du_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} du_io_t;

static int du_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int du_is_base_exc(const json_t *r) {
    if (!du_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0;
}

static json_t *du_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static int du_truthy(const json_t *v) {
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

static json_t *du_seam(du_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return du_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: user_enabled @ tools/desktop_ui.py:user_enabled */
json_t *user_enabled(du_io_t *io, json_t *in) {
    json_t *dflt = json_obj_get(in, "default");
    const char *setting = json_get_str(in, "setting", "");
    /* try: from hermes_cli.config import load_config_readonly;
     *      display = load_config_readonly().get("display")
     * except Exception: return default */
    json_t *imp = du_seam(io, "import_load_config_readonly", NULL);
    if (du_is_raise(imp)) {
        if (du_is_base_exc(imp)) return imp;
        json_free(imp);
        return dflt ? json_copy(dflt) : json_null();
    }
    json_free(imp);
    json_t *cfg = du_seam(io, "load_config_readonly", NULL);
    if (du_is_raise(cfg)) {
        if (du_is_base_exc(cfg)) return cfg;
        json_free(cfg);
        return dflt ? json_copy(dflt) : json_null();
    }
    json_t *display = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "display") : NULL;
    if (cfg) json_free(cfg);
    if (!display || display->type != JSON_OBJECT)
        return dflt ? json_copy(dflt) : json_null();
    json_t *v = json_obj_get(display, setting);
    if (!v) return dflt ? json_copy(dflt) : json_null();
    return json_bool(du_truthy(v) ? true : false);
}
