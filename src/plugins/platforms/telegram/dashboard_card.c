/*
 * dashboard_card.c — real C port of upstream Hermes plugins/platforms/telegram/dashboard_card.py constant tables.
 * PoP: cn_fad49a65cf_constants @ plugins/platforms/telegram/dashboard_card.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_fad49a65cf.h"

json_t *cn_fad49a65cf_constants(void) {
    json_t *out = json_object();
    json_set(out, "DASHBOARD_CARD", ({ json_t *__o = json_object();
        json_set(__o, "dashboard_description", json_string("Run Hermes from Telegram DMs, groups, and topics."));
        json_set(__o, "dashboard_env_vars", ({ json_t *__a = json_array();
            json_append(__a, json_string("TELEGRAM_BOT_TOKEN"));
            json_append(__a, json_string("TELEGRAM_ALLOWED_USERS"));
            json_append(__a, json_string("TELEGRAM_PROXY"));
          __a; }));
        json_set(__o, "docs_url", json_string("https://core.telegram.org/bots/features#botfather"));
      __o; }));
    return out;
}
