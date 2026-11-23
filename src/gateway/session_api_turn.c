/*
 * session_api_turn.c — C home for upstream Hermes gateway/session_api_turn.py.
 *
 * PoP: w10_sessionapi_8e7979_valid_owner_scope @ gateway/session_api_turn.py:_valid_owner_scope
 * PoP: w10_sessionapi_8e7979_owns_api_run @ gateway/session_api_turn.py:owns_api_run
 *
 * The opaque 64-hex owner-scope matcher and the failing-closed run-ownership
 * check.  `run_admission` is injected (stored_scope == NULL models "no run",
 * run_raise models AuthorityRefusal) exactly as the live function sees it.
 */
#include "noc_w10_sessionapi_8e7979.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int valid_64hex(const json_t *v) {
    if (!v || v->type != JSON_STRING) return 0;
    if (strlen(v->str_val) != 64) return 0;
    for (const char *p = v->str_val; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return 0;
    return 1;
}

struct json_t *w10_sessionapi_8e7979_valid_owner_scope(const struct json_t *value) {
    return json_bool(valid_64hex(value) != 0);
}

struct json_t *w10_sessionapi_8e7979_owns_api_run(const struct json_t *owner_scope,
                                                 int run_raise,
                                                 const struct json_t *stored_scope) {
    if (!valid_64hex(owner_scope)) return json_bool(false);
    if (run_raise) return json_bool(false);
    if (!valid_64hex(stored_scope)) return json_bool(false);
    return json_bool(strcmp(owner_scope->str_val, stored_scope->str_val) == 0);
}
