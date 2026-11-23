/*
 * dashboard_card.c — real C port of upstream Hermes plugins/platforms/discord/dashboard_card.py constant tables.
 * PoP: cn_27753cf9ee_constants @ plugins/platforms/discord/dashboard_card.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_27753cf9ee.h"

json_t *cn_27753cf9ee_constants(void) {
    json_t *out = json_object();
    json_set(out, "DASHBOARD_CARD", ({ json_t *__o = json_object();
        json_set(__o, "dashboard_description", json_string("Connect Hermes to Discord DMs, channels, and threads."));
        json_set(__o, "dashboard_env_vars", ({ json_t *__a = json_array();
            json_append(__a, json_string("DISCORD_BOT_TOKEN"));
            json_append(__a, json_string("DISCORD_ALLOWED_USERS"));
          __a; }));
        json_set(__o, "docs_url", json_string("https://discord.com/developers/applications"));
      __o; }));
    return out;
}
