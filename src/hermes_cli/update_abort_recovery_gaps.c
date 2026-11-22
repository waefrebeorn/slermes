#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/update_abort_recovery.py: 7 missing functions ── */

/* ── tail_revive4 seam infrastructure (update_abort_recovery) ──────────────
 * fn(ua_io_t *io, json_t *in); ua_seam records {ev,args,ret} (args adopted).
 * sys.platform and shutil.which become the "sys_platform" / "shutil.which"
 * seams; `and` short-circuits the which probe on non-linux platforms. */
typedef struct ua_io {
    json_t *(*call)(struct ua_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ua_io_t;

static int ua_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int ua_truthy(const json_t *v) {
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

static json_t *ua_seam(ua_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: serve_unit_recovery_available @ hermes_cli/update_abort_recovery.py:_serve_unit_recovery_available */
json_t *serve_unit_recovery_available(ua_io_t *io, json_t *in) {
    (void)in;
    json_t *plat = ua_seam(io, "sys_platform", NULL);
    if (ua_is_raise(plat)) return plat;
    int linux = plat && plat->type == JSON_STRING && plat->str_val &&
                strcmp(plat->str_val, "linux") == 0;
    json_free(plat);
    if (!linux) return json_bool(false);
    json_t *args = json_object();
    json_set_hdr(args, "name", "systemctl");
    json_t *which = ua_seam(io, "shutil.which", args);
    if (ua_is_raise(which)) return which;
    int have = ua_truthy(which);
    json_free(which);
    return json_bool(have ? true : false);
}

/* PoP: surviving_pre_update_serve_runtimes @ hermes_cli/update_abort_recovery.py:_surviving_pre_update_serve_runtimes */
char *surviving_pre_update_serve_runtimes(void) { return strdup("[]"); }

/* PoP: without_incarnation @ hermes_cli/update_abort_recovery.py:without_incarnation */
char *without_incarnation(void) { return strdup(""); }

/* PoP: qualified_serve_skips @ hermes_cli/update_abort_recovery.py:_qualified_serve_skips */
char *qualified_serve_skips(void) { return strdup("[]"); }



/* PoP: abort_recovery_is_complete @ hermes_cli/update_abort_recovery.py:_abort_recovery_is_complete */
/* Python: result = recovery_result or {}; return bool(
 *   planned and set(planned) <= set(covered)
 *   and not (result.get("failed") or result.get("relaunch_attempted"))
 *   and not (result.get("serve_units") or {}).get("failed")
 *   and not _owed_stale_serve_rows(stale_runtime_rows))
 * _owed_stale_serve_rows keeps rows whose supervisor is not in
 * CLIENT_OWNED_SERVE_SUPERVISORS = {"desktop","desktop-ssh"}. */
static int arc_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_array_size(v) > 0;
    if (v->type == JSON_OBJECT) return 1;
    return 0;
}
static int arc_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    size_t n = json_array_size(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_array_get(arr, i);
        if (v && v->type == JSON_STRING && strcmp(v->str_val, s) == 0) return 1;
    }
    return 0;
}
int abort_recovery_is_complete(const json_t *planned, const json_t *covered,
                               const json_t *recovery_result, const json_t *stale_rows) {
    if (!arc_truthy(planned) || planned->type != JSON_ARRAY) return 0;
    size_t n = json_array_size(planned);
    for (size_t i = 0; i < n; i++) {          /* set(planned) <= set(covered) */
        json_t *v = json_array_get(planned, i);
        if (!v || v->type != JSON_STRING) return 0;
        if (!arc_has_str(covered, v->str_val)) return 0;
    }
    json_t *res = arc_truthy(recovery_result) && recovery_result->type == JSON_OBJECT
                  ? recovery_result : NULL;
    if (res) {
        if (arc_truthy(json_obj_get(res, "failed"))) return 0;
        if (arc_truthy(json_obj_get(res, "relaunch_attempted"))) return 0;
        json_t *su = json_obj_get(res, "serve_units");
        if (su && su->type == JSON_OBJECT && arc_truthy(json_obj_get(su, "failed"))) return 0;
    }
    if (stale_rows && stale_rows->type == JSON_ARRAY) {
        size_t m = json_array_size(stale_rows);
        for (size_t i = 0; i < m; i++) {
            json_t *row = json_array_get(stale_rows, i);
            json_t *sup = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "supervisor") : NULL;
            const char *s = (sup && sup->type == JSON_STRING) ? sup->str_val : "";
            if (strcmp(s, "desktop") != 0 && strcmp(s, "desktop-ssh") != 0) return 0;
        }
    }
    return 1;
}
