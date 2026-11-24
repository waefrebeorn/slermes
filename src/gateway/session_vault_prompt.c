/*
 * session_vault_prompt.c — C port of upstream Hermes gateway/session_vault_prompt.py.
 *
 * PoP: w16_sessionvau_0697e7_unlock_params @ gateway/session_vault_prompt.py:unlock_prompt
 * PoP: w16_sessionvau_0697e7_save_login_params @ gateway/session_vault_prompt.py:save_login
 * PoP: w16_sessionvau_0697e7_code_params @ gateway/session_vault_prompt.py:code_prompt
 * PoP: w16_sessionvau_0697e7_save_login @ gateway/session_vault_prompt.py:save_login
 * PoP: w16_sessionvau_0697e7_timeouts @ gateway/session_vault_prompt.py:UNLOCK_TIMEOUT_S
 *
 * The viewer-ask transport is a scripted seam: `raw` is exactly what the
 * live ask("vault.save_login", ...) returns, so the JSON parse / dict / password
 * truthiness decisions below are the real ones.
 */
#include "noc_w16_sessionvau_0697e7.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static json_t *copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

json_t *w16_sessionvau_0697e7_unlock_params(const json_t *backend, const json_t *display_name) {
    json_t *o = json_object();
    json_set(o, "backend", copy_or_null(backend));
    json_set(o, "display_name", copy_or_null(display_name));
    return o;
}

json_t *w16_sessionvau_0697e7_save_login_params(const json_t *origin, const json_t *site) {
    json_t *o = json_object();
    json_set(o, "origin", copy_or_null(origin));
    json_set(o, "site", copy_or_null(site));
    return o;
}

json_t *w16_sessionvau_0697e7_code_params(const json_t *site, const json_t *hint) {
    json_t *o = json_object();
    json_set(o, "site", copy_or_null(site));
    json_set(o, "hint", copy_or_null(hint));
    return o;
}

json_t *w16_sessionvau_0697e7_save_login(const char *raw) {
    if (!raw || !raw[0]) return NULL;              /* `raw = ask(...)`; `if raw else None` */
    json_t *data = json_parse(raw, NULL);
    if (!data) return NULL;                         /* ValueError -> None */
    if (data->type != JSON_OBJECT) { json_free(data); return NULL; }
    json_t *pw = json_obj_get(data, "password");
    if (!py_truthy(pw)) { json_free(data); return NULL; }
    json_t *result = json_copy(data);              /* copy BEFORE json_free(data) */
    json_free(data);
    return result;
}

json_t *w16_sessionvau_0697e7_timeouts(void) {
    json_t *o = json_object();
    json_set(o, "unlock", json_number(120));
    json_set(o, "save_login", json_number(180));
    json_set(o, "code", json_number(180));
    return o;
}

