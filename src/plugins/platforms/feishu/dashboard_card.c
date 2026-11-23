/*
 * dashboard_card.c — real C port of upstream Hermes plugins/platforms/feishu/dashboard_card.py constant tables.
 * PoP: cn_405f7929b1_constants @ plugins/platforms/feishu/dashboard_card.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_405f7929b1.h"

json_t *cn_405f7929b1_constants(void) {
    json_t *out = json_object();
    json_set(out, "DASHBOARD_CARD", ({ json_t *__o = json_object();
        json_set(__o, "dashboard_description", json_string("Use Hermes inside Feishu / Lark."));
        json_set(__o, "dashboard_env_vars", ({ json_t *__a = json_array();
            json_append(__a, json_string("FEISHU_APP_ID"));
            json_append(__a, json_string("FEISHU_APP_SECRET"));
            json_append(__a, json_string("FEISHU_ENCRYPT_KEY"));
            json_append(__a, json_string("FEISHU_VERIFICATION_TOKEN"));
          __a; }));
        json_set(__o, "docs_url", json_string("https://open.feishu.cn/document/uAjLw4CM/ukTMukTMukTM/reference/im-v1/intro"));
      __o; }));
    return out;
}
