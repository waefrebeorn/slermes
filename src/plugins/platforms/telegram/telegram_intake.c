/*
 * telegram_intake.c -- C11 port of plugins/platforms/telegram/telegram_intake.py.
 *
 * `effective_update_message` is pure over the update object core hands it, so
 * the whole function is real here: the attribute chain, the Python `or`
 * truthiness and the identity test against the two business fields.
 *
 * PoP: w22_telegramin_6b9a1e_effective_update_message @ plugins/platforms/telegram/telegram_intake.py:effective_update_message
 *
 * Object identity: decoded JSON cannot alias two nodes, so a fixture message
 * carries a "label" and equal non-empty labels mean the same object. CPython
 * singleton identity is mirrored for booleans and for small integers.
 */
#include "hermes_json.h"
#include "noc_w22_telegramin_6b9a1e.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* Python truthiness of a decoded value; absent (NULL) reads as None. */
static int ti_truthy(const json_t *v) {
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

/* `a is b` for the shapes a fixture can carry. */
static int ti_same_object(const json_t *a, const json_t *b) {
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    if (a->type == JSON_OBJECT) {
        const json_t *la = json_obj_get(a, "label");
        const json_t *lb = json_obj_get(b, "label");
        if (!la || !lb) return 0;
        if (la->type != JSON_STRING || lb->type != JSON_STRING) return 0;
        if (!la->str_val || !la->str_val[0]) return 0;
        return strcmp(la->str_val, lb->str_val) == 0;
    }
    if (a->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_NUMBER) {
        double d = a->num_val;
        if (d != b->num_val) return 0;
        if (!(d >= -5.0 && d <= 256.0)) return 0;
        return floor(d) == d;
    }
    /* Strings, arrays, nulls: separate decoded nodes are separate objects. */
    return 0;
}

static const json_t *ti_attr(const json_t *update, const char *name) {
    if (!update || update->type != JSON_OBJECT) return NULL;
    return json_obj_get(update, name);
}

json_t *w22_telegramin_6b9a1e_effective_update_message(const json_t *update) {
    static const char *const BUSINESS_FIELDS[] = {"business_message",
                                                  "edited_business_message"};
    const json_t *message, *eff, *plain;
    size_t i;

    eff = ti_attr(update, "effective_message");
    plain = ti_attr(update, "message");
    /* `getattr(update, "effective_message", None) or getattr(update, "message", None)` */
    message = ti_truthy(eff) ? eff : plain;
    if (message == NULL || message->type == JSON_NULL) return json_null();

    for (i = 0; i < sizeof(BUSINESS_FIELDS) / sizeof(BUSINESS_FIELDS[0]); i++) {
        if (ti_same_object(message, ti_attr(update, BUSINESS_FIELDS[i]))) {
            return json_null();
        }
    }
    return json_copy(message);
}
