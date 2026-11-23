/*
 * store_handles.c — C home for upstream Hermes tui_gateway/store_handles.py.
 *
 * PoP: storehandl_is_handle @ tui_gateway/store_handles.py:_is_handle
 * PoP: storehandl_launch_persistence @ tui_gateway/store_handles.py:_launch_persistence
 * PoP: storehandl_persistence_beside @ tui_gateway/store_handles.py:_persistence_beside
 *
 * Handle identity / isinstance and open_persistence are explicit fixture seams.
 */
#include "noc_w13_storehandl_0c06da.h"
#include "hermes_json.h"

#include <string.h>

static int bflag(const json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v && v->type == JSON_BOOL && v->bool_val;
}

struct json_t *storehandl_is_handle(const struct json_t *db) {
    return json_bool(bflag(db, "is_handle") ? true : false);
}

struct json_t *storehandl_launch_persistence(const struct json_t *launch_is_handle) {
    json_t *out = json_object();
    if (launch_is_handle && launch_is_handle->type == JSON_BOOL && launch_is_handle->bool_val) {
        json_set_hdr(out, "handle", "launch");
    } else {
        json_free(out);
        out = json_null();
    }
    return out;
}

struct json_t *storehandl_persistence_beside(const struct json_t *db,
                                             const struct json_t *launch_is_handle) {
    if (!db || db->type != JSON_OBJECT) return json_null();
    if (bflag(db, "is_launch")) return storehandl_launch_persistence(launch_is_handle);
    if (bflag(db, "is_handle")) {
        json_t *out = json_object();
        json_set_hdr(out, "handle", "own");
        const char *scope = json_get_str(db, "scope", NULL);
        json_set_hdr(out, "scope", scope ? scope : "");
        return out;
    }
    return json_null();
}
