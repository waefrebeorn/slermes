/*
 * gateway_env.c — real C port of upstream Hermes plugins/platforms/weixin/gateway_env.py constant tables.
 * PoP: cn_0760b3e14f_constants @ plugins/platforms/weixin/gateway_env.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_0760b3e14f.h"

json_t *cn_0760b3e14f_constants(void) {
    json_t *out = json_object();
    json_set(out, "ENV_ENABLE_CREDENTIALS", ({ json_t *__a = json_array();
        json_append(__a, json_string("WEIXIN_TOKEN"));
        json_append(__a, json_string("WEIXIN_ACCOUNT_ID"));
      __a; }));
    json_set(out, "AUTHZ", ({ json_t *__o = json_object();
        json_set(__o, "allow_all_env", json_string("WEIXIN_ALLOW_ALL_USERS"));
        json_set(__o, "allowed_users_env", json_string("WEIXIN_ALLOWED_USERS"));
        json_set(__o, "allowlist_suppresses_pairing", json_bool(true));
        json_set(__o, "own_policy_env", ({ json_t *__a = json_array();
            json_append(__a, json_string("WEIXIN_DM_POLICY"));
            json_append(__a, json_string("WEIXIN_GROUP_POLICY"));
          __a; }));
      __o; }));
    return out;
}
