/*
 * gateway_env.c — real C port of upstream Hermes plugins/platforms/email/gateway_env.py constant tables.
 * PoP: cn_06e4137121_constants @ plugins/platforms/email/gateway_env.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_06e4137121.h"

json_t *cn_06e4137121_constants(void) {
    json_t *out = json_object();
    json_set(out, "ENV_ENABLE_CREDENTIALS", ({ json_t *__a = json_array();
        json_append(__a, json_string("EMAIL_ADDRESS"));
        json_append(__a, json_string("EMAIL_PASSWORD"));
        json_append(__a, json_string("EMAIL_IMAP_HOST"));
        json_append(__a, json_string("EMAIL_SMTP_HOST"));
      __a; }));
    json_set(out, "AUTHZ", ({ json_t *__o = json_object();
        json_set(__o, "allow_all_env", json_string("EMAIL_ALLOW_ALL_USERS"));
        json_set(__o, "allowed_users_env", json_string("EMAIL_ALLOWED_USERS"));
        json_set(__o, "allowlist_suppresses_pairing", json_bool(true));
      __o; }));
    return out;
}
