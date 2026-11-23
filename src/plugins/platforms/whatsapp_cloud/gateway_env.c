/*
 * gateway_env.c — real C port of upstream Hermes plugins/platforms/whatsapp_cloud/gateway_env.py constant tables.
 * PoP: cn_0feb49f5fa_constants @ plugins/platforms/whatsapp_cloud/gateway_env.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_0feb49f5fa.h"

json_t *cn_0feb49f5fa_constants(void) {
    json_t *out = json_object();
    json_set(out, "ENV_ENABLE_CREDENTIALS", ({ json_t *__a = json_array();
        json_append(__a, json_string("WHATSAPP_CLOUD_PHONE_NUMBER_ID"));
        json_append(__a, json_string("WHATSAPP_CLOUD_ACCESS_TOKEN"));
      __a; }));
    json_set(out, "AUTHZ", ({ json_t *__o = json_object();
        json_set(__o, "allow_all_env", json_string("WHATSAPP_CLOUD_ALLOW_ALL_USERS"));
        json_set(__o, "allowed_users_env", json_string("WHATSAPP_CLOUD_ALLOWED_USERS"));
        json_set(__o, "allowlist_suppresses_pairing", json_bool(true));
      __o; }));
    return out;
}
