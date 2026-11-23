/*
 * run_api.c — real C11 port of upstream Hermes gateway/run_api.py (pure leaves).
 *
 * PoP: runapi_listener_public_url @ gateway/run_api.py:_listener_public_url
 * PoP: runapi_embedded_chat_enabled @ gateway/run_api.py:_embedded_chat_enabled
 * PoP: runapi_socket_options @ gateway/run_api.py:_listener_socket
 */
#include "noc_w5_runapi_7d29d9.h"

#include <string.h>

json_t *runapi_listener_public_url(int webapp_present, const json_t *public_url) {
    /* live: None if webapp is None else webapp.public_url */
    if (!webapp_present) return json_null();
    if (public_url && public_url->type == JSON_STRING) return json_copy(public_url);
    /* webapp.public_url may be "" (empty string) — preserve it. */
    if (public_url && public_url->type == JSON_NULL) return json_null();
    return json_string("");
}

json_t *runapi_embedded_chat_enabled(int loaded_web, int has_flag, int flag_val) {
    /* live: True if web is None else bool(getattr(web, flag, True)) */
    if (!loaded_web) return json_bool(true);
    if (!has_flag) return json_bool(true);
    return json_bool(flag_val ? true : false);
}

json_t *runapi_socket_options(double port, int is_nt, int dual_stack) {
    /* live:
     *   if dual_stack: IPV6_V6ONLY = 0
     *   if port and REUSE_FIXED_PORT (not nt): SO_REUSEADDR = 1
     *   elif port and EXCLUSIVE_FIXED_PORT (nt): SO_EXCLUSIVEADDRUSE = 1
     */
    int reuse = 0, exclusive = 0;
    json_t *out = json_object();
    if (port != 0.0) {
        if (!is_nt) reuse = 1;
        else exclusive = 1;
    }
    json_set(out, "dual_stack", json_bool(dual_stack ? true : false));
    json_set(out, "reuseaddr", json_bool(reuse ? true : false));
    json_set(out, "exclusive", json_bool(exclusive ? true : false));
    return out;
}
