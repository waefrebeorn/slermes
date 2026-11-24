/*
 * state.c -- real C11 port of agent/pet/state.py (gate w22_state_fd0065).
 *
 * PoP: w22_state_fd0065_derive_pet_state @ agent/pet/state.py:derive_pet_state
 * PoP: w22_state_fd0065_todos_all_done @ agent/pet/state.py:todos_all_done
 *
 * The module imports only `agent.pet.constants.PetState`, a plain str Enum, so
 * the port returns the member *name* ("IDLE", "RUN", ...) that the oracle reads
 * off the live enum. Nothing here needs a seam.
 */
#include "hermes_json.h"
#include "noc_w22_state_fd0065.h"

#include <string.h>

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int st_fd0065_truthy(const json_t *v) {
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

const char *w22_state_fd0065_derive_pet_state(const json_t *flags) {
    if (flags && flags->type != JSON_OBJECT) flags = NULL;
    if (st_fd0065_truthy(json_obj_get(flags, "error")))          return "FAILED";
    if (st_fd0065_truthy(json_obj_get(flags, "celebrate")))      return "JUMP";
    if (st_fd0065_truthy(json_obj_get(flags, "just_completed"))) return "WAVE";
    if (st_fd0065_truthy(json_obj_get(flags, "awaiting_input"))) return "WAITING";
    if (st_fd0065_truthy(json_obj_get(flags, "tool_running")))   return "RUN";
    if (st_fd0065_truthy(json_obj_get(flags, "reasoning")))      return "REVIEW";
    if (st_fd0065_truthy(json_obj_get(flags, "busy")))           return "RUN";
    return "IDLE";
}

/* `status` of one item of `list(todos or [])`: dicts use .get("status"),
 * everything else uses getattr(item, "status", None) -- None for JSON values. */
static int st_fd0065_item_done(const json_t *item) {
    const char *status;
    if (!item || item->type != JSON_OBJECT) return 0;
    status = json_get_str(item, "status", NULL);
    if (!status) return 0;
    return (!strcmp(status, "completed") || !strcmp(status, "cancelled")) ? 1 : 0;
}

int w22_state_fd0065_todos_all_done(const json_t *todos) {
    size_t i;

    /* `todos or []`: falsy inputs (None, False, 0, "", [], {}) fold to empty. */
    if (!st_fd0065_truthy(todos)) return 0;

    switch (todos->type) {
    case JSON_ARRAY:
        if (todos->c.count == 0) return 0;
        for (i = 0; i < todos->c.count; i++) {
            if (!st_fd0065_item_done(todos->c.items[i])) return 0;
        }
        return 1;
    case JSON_STRING:
        /* list("abc") iterates characters: every item is a str, no status. */
    case JSON_OBJECT:
        /* list(dict) iterates keys: every item is a str, no status. */
        return 0;
    case JSON_NUMBER:
    case JSON_BOOL:
        /* `list(<truthy non-iterable>)` raises TypeError. */
        return -1;
    case JSON_NULL:
        return 0;
    }
    return 0;
}
