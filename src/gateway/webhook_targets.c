/*
 * webhook_targets.c — faithful C11 port of upstream Hermes
 * gateway/webhook_targets.py (gate whtarget).
 * PoP: whtarget_is_known_platform @ gateway/webhook_targets.py:is_known_platform
 * PoP: whtarget_is_dispatchable_target @ gateway/webhook_targets.py:is_dispatchable_target
 */
#include "noc_whtarget.h"
#include "hermes_json.h"
#include <string.h>

/* BUILTIN_DELIVER_PLATFORMS (frozenset) — order immaterial. */
static const char *const BUILTIN[] = {
    "telegram", "discord", "slack", "signal", "sms", "whatsapp", "matrix",
    "mattermost", "email", "dingtalk", "feishu", "wecom", "wecom_callback",
    "weixin", "bluebubbles", "qqbot", NULL
};

static int in_set(const char *name, const char *const *set) {
    size_t i;
    if (!name) return 0;
    for (i = 0; set[i]; i++) {
        if (!strcmp(name, set[i])) return 1;
    }
    return 0;
}

int whtarget_is_known_platform(const char *name) {
    if (!name) return 0;
    if (in_set(name, BUILTIN)) return 1;
    /* The live function next tries platform_registry.is_registered(name) inside
     * suppress(Exception); an unresolvable plugin registry (or an unregistered
     * name) yields False. A pure C port reports 0 here. */
    return 0;
}

int whtarget_is_dispatchable_target(const struct json_t *target) {
    const char *s;
    /* isinstance(target, str) — hand-edited non-strings are not dispatchable. */
    if (!target || target->type != JSON_STRING) return 0;
    s = target->str_val;
    if (!s) return 0;
    if (!strcmp(s, "log") || !strcmp(s, "github_comment")) return 1; /* _NON_PLATFORM_TARGETS */
    return whtarget_is_known_platform(s);
}
