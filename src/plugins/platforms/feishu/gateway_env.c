/*
 * gateway_env.c — real C port of upstream Hermes plugins/platforms/feishu/gateway_env.py constant tables.
 * PoP: cn_a7309cf7dc_constants @ plugins/platforms/feishu/gateway_env.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_a7309cf7dc.h"

json_t *cn_a7309cf7dc_constants(void) {
    json_t *out = json_object();
    json_set(out, "ENV_ENABLE_CREDENTIALS", ({ json_t *__a = json_array();
        json_append(__a, json_string("FEISHU_APP_ID"));
        json_append(__a, json_string("FEISHU_APP_SECRET"));
      __a; }));
    json_set(out, "AUTHZ", ({ json_t *__o = json_object();
        json_set(__o, "allow_all_env", json_string("FEISHU_ALLOW_ALL_USERS"));
        json_set(__o, "allow_bots_env", json_string("FEISHU_ALLOW_BOTS"));
        json_set(__o, "allowed_users_env", json_string("FEISHU_ALLOWED_USERS"));
        json_set(__o, "allowlist_suppresses_pairing", json_bool(true));
      __o; }));
    return out;
}
