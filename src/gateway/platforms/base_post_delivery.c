/*
 * base_post_delivery.c -- real C11 port of the two pure decisions in upstream
 * Hermes gateway/platforms/base_post_delivery.py.
 *
 * PoP: w22_basepostde_1b117c_turn_run_generation @ gateway/platforms/base_post_delivery.py:turn_run_generation
 * PoP: w22_basepostde_1b117c_callback_outcome @ gateway/platforms/base_post_delivery.py:run_post_delivery_callback
 *
 * run_post_delivery_callback itself owns an event loop (callback() plus
 * asyncio.wait_for), so the loop stays upstream: what is ported is the
 * bounded-and-swallowed contract that loop implements.
 */
#include "hermes_json.h"
#include "noc_w22_basepostde_1b117c.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int bp117c_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `isinstance(x, int) and not isinstance(x, bool)`: only integral numbers pass. */
static int bp117c_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

json_t *w22_basepostde_1b117c_turn_run_generation(const json_t *event) {
    const json_t *gen = NULL;
    if (event && event->type == JSON_OBJECT) {
        gen = json_obj_get(event, "_hermes_run_generation");
    }
    if (!bp117c_is_py_int(gen)) return json_null();
    return json_number(gen->num_val);
}

json_t *w22_basepostde_1b117c_callback_outcome(const json_t *desc) {
    int invoked, awaited;
    json_t *out;

    /* if callable(callback): ... -- None and any non-callable is a no-op. */
    invoked = bp117c_truthy(json_obj_get(desc, "callable"));
    /* result = callback() must return normally before isawaitable() is reached. */
    awaited = invoked &&
              !bp117c_truthy(json_obj_get(desc, "raises")) &&
              bp117c_truthy(json_obj_get(desc, "awaitable"));
    /* contextlib.suppress(asyncio.TimeoutError, Exception) -- `timed_out` changes
     * nothing observable, which is the contract this gate pins. */

    out = json_object();
    json_set(out, "invoked", json_bool(invoked));
    json_set(out, "awaited", json_bool(awaited));
    json_set(out, "raised", json_bool(0));
    json_set(out, "returned_none", json_bool(1));
    return out;
}
