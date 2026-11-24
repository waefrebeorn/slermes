/*
 * session_route_recovery.c — real C11 port of upstream Hermes
 *   gateway/session_route_recovery.py pure helper(s).
 * PoP: w18_sessionrou_7912bd_messaging_platform @ gateway/session_route_recovery.py:_messaging_platform
 * Fixed Platform enum (no dynamic plugin members), matching the oracle stub.
 */
#include "hermes_json.h"
#include "noc_w18_sessionrou_7912bd.h"

static const char *const PLATFORM_VALUES[] = {
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "email", "sms", "dingtalk", "api_server",
    "webhook", "msgraph_webhook", "feishu", "wecom", "wecom_callback",
    "weixin", "bluebubbles", "qqbot", "relay", NULL
};

const char *w18_sessionrou_7912bd_messaging_platform(const char *label) {
    if (!label) return NULL;
    const char *found = NULL;
    for (int i = 0; PLATFORM_VALUES[i]; i++) {
        if (!strcmp(label, PLATFORM_VALUES[i])) { found = PLATFORM_VALUES[i]; break; }
    }
    if (!found) return NULL;
    /* Platform.LOCAL and Platform.API_SERVER answer None. */
    if (!strcmp(found, "local") || !strcmp(found, "api_server")) return NULL;
    return found;  /* stable static string, never an interior pointer */
}
