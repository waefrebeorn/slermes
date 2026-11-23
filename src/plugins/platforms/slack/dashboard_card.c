/*
 * dashboard_card.c — real C port of upstream Hermes plugins/platforms/slack/dashboard_card.py constant tables.
 * PoP: cn_966d9e89b2_constants @ plugins/platforms/slack/dashboard_card.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_966d9e89b2.h"

json_t *cn_966d9e89b2_constants(void) {
    json_t *out = json_object();
    json_set(out, "DASHBOARD_CARD", ({ json_t *__o = json_object();
        json_set(__o, "dashboard_description", json_string("Use Hermes from Slack via Socket Mode. Add allowed Slack member IDs so connected bots can respond."));
        json_set(__o, "dashboard_env_vars", ({ json_t *__a = json_array();
            json_append(__a, json_string("SLACK_BOT_TOKEN"));
            json_append(__a, json_string("SLACK_APP_TOKEN"));
            json_append(__a, json_string("SLACK_ALLOWED_USERS"));
          __a; }));
        json_set(__o, "docs_url", json_string("https://api.slack.com/apps"));
      __o; }));
    return out;
}
