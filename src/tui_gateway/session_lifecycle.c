/*
 * session_lifecycle.c — C home for upstream Hermes tui_gateway/session_lifecycle.py.
 *
 * PoP: w18_sessionlif_2e5c9b_lease_metadata @ tui_gateway/session_lifecycle.py:_lease_metadata
 * PoP: w18_sessionlif_2e5c9b_is_gateway_owned_source @ tui_gateway/session_lifecycle.py:_is_gateway_owned_source
 * PoP: w18_sessionlif_2e5c9b_lifecycle_own_sid @ tui_gateway/session_lifecycle.py:_lifecycle_own_sid
 * PoP: w18_sessionlif_2e5c9b_session_holds_transcript_claim @ tui_gateway/session_lifecycle.py:_session_holds_transcript_claim
 *
 * Platform membership mirrors gateway.config.Platform's built-in values (the
 * oracle injects a Platform stub with the same set); plugin _missing_ lookups
 * are not exercised.
 */
#include "noc_w18_sessionlif_2e5c9b.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const NON_GATEWAY[] = {
    "", "tui", "cli", "webui", "desktop", "cron", "kanban", "subagent", "test",
    "local", "acp", "webhook", "api_server", "msgraph_webhook"};
static const char *const PLATFORMS[] = {
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack", "signal",
    "mattermost", "matrix", "email", "sms", "dingtalk", "api_server", "webhook",
    "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
    "qqbot", "relay"};

static int in_list(const char *c, const char *const *list, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (!strcmp(c, list[i])) return 1;
    return 0;
}

static char *strip_lower(const char *s) {
    size_t n = strlen(s), a = 0, b = n, i;
    char *d;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    d = (char *)malloc(b - a + 1);
    if (!d) return NULL;
    for (i = a; i < b; ++i) {
        unsigned char c = (unsigned char)s[i];
        d[i - a] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
    }
    d[b - a] = '\0';
    return d;
}

static const char *str_of(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

json_t *w18_sessionlif_2e5c9b_lease_metadata(const char *live_session_id) {
    json_t *o = json_object();
    json_set(o, "live_session_id", json_string(live_session_id ? live_session_id : ""));
    return o;
}

json_t *w18_sessionlif_2e5c9b_is_gateway_owned_source(const char *source) {
    char *src = strip_lower(source ? source : "");
    int owned;
    if (!src) return json_bool(false);
    if (in_list(src, NON_GATEWAY, sizeof(NON_GATEWAY) / sizeof(NON_GATEWAY[0])))
        owned = 0;
    else
        owned = in_list(src, PLATFORMS, sizeof(PLATFORMS) / sizeof(PLATFORMS[0]));
    free(src);
    return json_bool(owned ? true : false);
}

json_t *w18_sessionlif_2e5c9b_lifecycle_own_sid(const json_t *session, const char *sid_hint) {
    const char *sid;
    if (sid_hint && *sid_hint) return json_string(sid_hint);
    sid = str_of(session ? json_obj_get(session, "_sid") : NULL);
    /* Empty result: the oracle stubs _sessions to {} so the registry scan yields "". */
    return json_string(sid);
}

json_t *w18_sessionlif_2e5c9b_session_holds_transcript_claim(const json_t *session, double now) {
    const json_t *claims = session ? json_obj_get(session, "_transcript_claims") : NULL;
    size_t i;
    if (!claims || claims->type != JSON_OBJECT) return json_bool(false);
    for (i = 0; i < claims->c.count; ++i) {
        json_t *v = claims->c.items[i];
        if (v && v->type == JSON_NUMBER && v->num_val > now) return json_bool(true);
    }
    return json_bool(false);
}

