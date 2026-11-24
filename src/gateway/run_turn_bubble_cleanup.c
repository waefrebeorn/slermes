/*
 * run_turn_bubble_cleanup.c -- real C11 port of the admission test, snapshot
 * and failure tally of upstream Hermes gateway/run_turn_bubble_cleanup.py.
 *
 * PoP: w20_runturnbub_84c9ef_delete_timeout_seconds @ gateway/run_turn_bubble_cleanup.py:DELETE_TIMEOUT_SECONDS
 * PoP: w20_runturnbub_84c9ef_register @ gateway/run_turn_bubble_cleanup.py:register_bubble_cleanup
 * PoP: w20_runturnbub_84c9ef_cleanup @ gateway/run_turn_bubble_cleanup.py:_cleanup_temp_bubbles
 *
 * Not ported: settle_turn_senders (asyncio.wait over live sender tasks) and the
 * delete coroutines themselves -- asyncio.wait_for(adapter.delete_message(...))
 * and adapter.register_post_delivery_callback(...) are the adapter seam.
 */
#include "hermes_json.h"
#include "noc_w20_runturnbub_84c9ef.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for a decoded value (absent == None). */
static int rtb84_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

double w20_runturnbub_84c9ef_delete_timeout_seconds(void) {
    return 10.0;
}

json_t *w20_runturnbub_84c9ef_register(const json_t *ctx, const json_t *response) {
    json_t *out = json_object();
    json_t *ids = ctx ? json_obj_get(ctx, "ids") : NULL;
    const json_t *adapter = ctx ? json_obj_get(ctx, "has_adapter") : NULL;
    const json_t *progress = ctx ? json_obj_get(ctx, "cleanup_progress") : NULL;
    const char *session_key = ctx ? json_get_str(ctx, "session_key", "") : "";
    const char *generation;
    int admit;

    /* if not (turn_ctx._cleanup_progress and adapter is not None and ids and
     *         session_key and isinstance(response, dict) and not response.get("failed")
     *         and hasattr(adapter, "register_post_delivery_callback")): return */
    admit = rtb84_truthy(progress)
            && rtb84_truthy(adapter)
            && ids && ids->type == JSON_ARRAY && ids->c.count > 0
            && session_key && session_key[0]
            && response && response->type == JSON_OBJECT
            && !rtb84_truthy(json_obj_get(response, "failed"))
            && (ctx ? (json_get_bool(ctx, "has_register_cb", 0) ? 1 : 0) : 0);

    json_set(out, "register", json_bool(admit));
    if (!admit) {
        json_set(out, "ids", json_array());
        json_set(out, "chat_id", json_null());
        json_set(out, "generation", json_null());
        return out;
    }
    json_set(out, "ids", json_copy(ids));       /* list(ids) */
    {
        json_t *chat = ctx ? json_obj_get(ctx, "chat_id") : NULL;
        json_set(out, "chat_id", chat ? json_copy(chat) : json_null());
    }
    generation = ctx ? json_get_str(ctx, "generation", "") : "";
    json_set(out, "generation", generation[0] ? json_string(generation) : json_null());
    return out;
}

json_t *w20_runturnbub_84c9ef_cleanup(const json_t *spec) {
    json_t *out = json_object();
    const json_t *results = spec ? json_obj_get(spec, "results") : NULL;
    const char *session_key = spec ? json_get_str(spec, "session_key", "") : "";
    size_t i, n, failed = 0;

    if (!results || results->type != JSON_ARRAY) {
        json_set(out, "failed", json_number(0));
        json_set(out, "message", json_null());
        return out;
    }
    n = results->c.count;
    for (i = 0; i < n; i++) {
        const json_t *e = results->c.items[i];
        int ok = 0;
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, "ok")) ok = 1;
        if (e && e->type == JSON_BOOL && e->bool_val) ok = 1;
        if (!ok) failed++;
    }
    json_set(out, "failed", json_number((double)failed));
    if (failed) {
        static const char *const FMT =
            "Temp bubble cleanup for session %s: %zu of %zu deletes failed";
        size_t cap = strlen(FMT) + strlen(session_key) + 64;
        char *msg = (char *)malloc(cap);
        if (msg) {
            snprintf(msg, cap, FMT, session_key, failed, n);
            json_set(out, "message", json_string(msg));
            free(msg);
        } else {
            json_set(out, "message", json_null());
        }
    } else {
        json_set(out, "message", json_null());
    }
    return out;
}
