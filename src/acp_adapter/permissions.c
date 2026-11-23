/*
 * permissions.c -- faithful C11 port of upstream Hermes acp_adapter/permissions.py.
 *
 * PoP: w9_permissions_d89402_resolve_timeout @ acp_adapter/permissions.py:resolve_permission_timeout
 * PoP: w9_permissions_d89402_map_outcome     @ acp_adapter/permissions.py:_map_outcome_to_hermes
 * PoP: w9_permissions_d89402_build_options    @ acp_adapter/permissions.py:_build_permission_options
 *
 * The asyncio request/answer bridge is not portable; the timeout resolution,
 * outcome mapping and option construction are, including the SDK capability seam.
 */
#include "noc_w9_permissions_d89402.h"
#include <string.h>

double w9_permissions_d89402_resolve_timeout(double timeout, int given, double config_timeout) {
    return given ? timeout : config_timeout;
}

const char *w9_permissions_d89402_map_outcome(const char *option_id, int valid, const json_t *allowed) {
    if (!valid || !option_id) return "deny";
    int in = 0;
    if (allowed && allowed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(allowed); i++) {
            json_t *e = json_get(allowed, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(option_id, e->str_val)) {
                in = 1;
                break;
            }
        }
    }
    if (!in) return "deny";
    if (!strcmp(option_id, "allow_once"))   return "once";
    if (!strcmp(option_id, "allow_session")) return "session";
    if (!strcmp(option_id, "allow_always"))  return "always";
    if (!strcmp(option_id, "deny"))          return "deny";
    if (!strcmp(option_id, "deny_always"))   return "deny";
    return "deny";
}

static void add_opt(json_t *arr, const char *id, const char *kind, const char *name) {
    json_t *o = json_object();
    json_set(o, "option_id", json_string(id));
    json_set(o, "kind", json_string(kind));
    json_set(o, "name", json_string(name));
    json_append(arr, o);
}

json_t *w9_permissions_d89402_build_options(int allow_permanent, int allow_session,
                                            int smart_denied, int supports_reject_always) {
    json_t *out = json_array();
    int once_only = smart_denied || !allow_session;
    add_opt(out, "allow_once", "allow_once", "Allow once");
    if (!once_only) {
        add_opt(out, "allow_session", "allow_always", "Allow for session");
        if (allow_permanent) add_opt(out, "allow_always", "allow_always", "Allow always");
    }
    add_opt(out, "deny", "reject_once", "Deny");
    if (!once_only && supports_reject_always)
        add_opt(out, "deny_always", "reject_always", "Deny always");
    return out;
}
