/*
 * gateway_env.c — real C port of upstream Hermes plugins/platforms/wecom/gateway_env.py constant tables.
 * PoP: cn_23f115c7d5_constants @ plugins/platforms/wecom/gateway_env.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_23f115c7d5.h"

json_t *cn_23f115c7d5_constants(void) {
    json_t *out = json_object();
    json_set(out, "ENV_ENABLE_CREDENTIALS", ({ json_t *__a = json_array();
        json_append(__a, json_string("WECOM_BOT_ID"));
        json_append(__a, json_string("WECOM_SECRET"));
      __a; }));
    json_set(out, "CALLBACK_ENV_ENABLE_CREDENTIALS", ({ json_t *__a = json_array();
        json_append(__a, json_string("WECOM_CALLBACK_CORP_ID"));
        json_append(__a, json_string("WECOM_CALLBACK_CORP_SECRET"));
      __a; }));
    json_set(out, "AUTHZ", ({ json_t *__o = json_object();
        json_set(__o, "allow_all_env", json_string("WECOM_ALLOW_ALL_USERS"));
        json_set(__o, "allowed_users_env", json_string("WECOM_ALLOWED_USERS"));
        json_set(__o, "allowlist_suppresses_pairing", json_bool(true));
        json_set(__o, "own_policy_env", ({ json_t *__a = json_array();
            json_append(__a, json_string("WECOM_DM_POLICY"));
            json_append(__a, json_string("WECOM_GROUP_POLICY"));
          __a; }));
      __o; }));
    json_set(out, "CALLBACK_AUTHZ", ({ json_t *__o = json_object();
        json_set(__o, "allow_all_env", json_string("WECOM_CALLBACK_ALLOW_ALL_USERS"));
        json_set(__o, "allowed_users_env", json_string("WECOM_CALLBACK_ALLOWED_USERS"));
        json_set(__o, "allowlist_suppresses_pairing", json_bool(true));
      __o; }));
    return out;
}
