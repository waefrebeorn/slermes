/*
 * warning_notifications.c — real C11 port of upstream Hermes
 * gateway/warning_notifications.py.
 *
 * PoP: w17_warningnot_af9536_is_warning_status             @ gateway/warning_notifications.py:is_warning_status
 * PoP: w17_warningnot_af9536_is_diagnostic_notice          @ gateway/warning_notifications.py:is_diagnostic_notice
 * PoP: w17_warningnot_af9536_diagnostic_metadata           @ gateway/warning_notifications.py:diagnostic_metadata
 * PoP: w17_warningnot_af9536_warning_notifications_enabled @ gateway/warning_notifications.py:warning_notifications_enabled
 * PoP: w17_warningnot_af9536_diagnostic_turn_muted         @ gateway/warning_notifications.py:diagnostic_turn_muted
 *
 * The gateway.display_config.resolve_display_setting dependency is ported for
 * the single setting this module asks about ("suppress_warning_notifications"),
 * matching the oracle's identically-scoped stub.
 */
#include "noc_w17_warningnot_af9536.h"
#include <ctype.h>
#include <string.h>

int w17_warningnot_af9536_is_warning_status(const char *event_type, int message_is_diagnostic) {
    return (event_type && !strcmp(event_type, "warn")) || message_is_diagnostic ? 1 : 0;
}

int w17_warningnot_af9536_is_diagnostic_notice(const char *level, int level_is_null, const char *key) {
    if (!level_is_null && level) {
        if (!strcmp(level, "warn") || !strcmp(level, "error")) return 1;
    }
    const char *k = key ? key : "";
    return strncmp(k, "credits.", 8) == 0 ? 1 : 0;
}

json_t *w17_warningnot_af9536_diagnostic_metadata(int internal, const json_t *metadata) {
    json_t *out = json_object();
    if (internal && json_is_object(metadata)) {
        const json_t *cat = json_obj_get(metadata, "notification_category");
        if (cat && cat->type == JSON_STRING && !strcmp(cat->str_val, "diagnostic"))
            json_set(out, "notification_category", json_string("diagnostic"));
    }
    return out;
}

/* _norm_suppress_warning_notifications: bool -> itself; str.strip().lower() in
 * {"true","1","yes","on"}; anything else -> False (never None). */
static int noc_wn_norm_suppress(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        size_t n = (size_t)(e - p);
        static const char *const truthy[] = {"true", "1", "yes", "on"};
        for (int i = 0; i < 4; i++)
            if (strlen(truthy[i]) == n && strncasecmp(p, truthy[i], n) == 0) return 1;
        return 0;
    }
    return 0;
}

/* resolve_display_setting(uc, platform, "suppress_warning_notifications", False):
 * highest-precedence non-null configured value wins; platform default/global
 * default for this setting is False. */
static int noc_wn_resolve_suppress(const json_t *uc, const char *platform) {
    const json_t *display = json_is_object(uc) ? json_obj_get(uc, "display") : NULL;
    if (!json_is_object(display)) return 0;
    const json_t *configured = NULL;
    const json_t *platforms = json_obj_get(display, "platforms");
    if (json_is_object(platforms) && platform) {
        const json_t *po = json_obj_get(platforms, platform);
        if (json_is_object(po)) {
            const json_t *v = json_obj_get(po, "suppress_warning_notifications");
            if (v && v->type != JSON_NULL) configured = v;
        }
    }
    if (!configured) {
        const json_t *v = json_obj_get(display, "suppress_warning_notifications");
        if (v && v->type != JSON_NULL) configured = v;
    }
    return configured ? noc_wn_norm_suppress(configured) : 0;
}

int w17_warningnot_af9536_warning_notifications_enabled(const char *platform, const json_t *user_config) {
    return noc_wn_resolve_suppress(user_config, platform) ? 0 : 1;
}

int w17_warningnot_af9536_diagnostic_turn_muted(const json_t *display_metadata,
                                                const char *platform, const json_t *user_config) {
    int is_diag = 0;
    if (json_is_object(display_metadata)) {
        const json_t *cat = json_obj_get(display_metadata, "notification_category");
        if (cat && cat->type == JSON_STRING && !strcmp(cat->str_val, "diagnostic")) is_diag = 1;
    }
    return (is_diag && !w17_warningnot_af9536_warning_notifications_enabled(platform, user_config)) ? 1 : 0;
}
