/*
 * gateway/config.c — Gateway configuration management.
 *
 * Port of Python gateway/config.py.
 *
 * Handles loading and validating configuration for:
 * - Connected platforms (Telegram, Discord, WhatsApp, Weixin, and more)
 * - Home channels for each platform
 * - Session reset policies
 * - Delivery preferences
 * - Streaming configuration
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "hermes_json.h"
#include "hermes_gateway_core.h"
#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

#define HERMES_LOG_WARN LOG_WARNING

/* ================================================================
 *  Coercion helpers (Port of Python gateway/config.py)
 * ================================================================ */

/* Port of Python: _coerce_bool */
bool gateway_config_coerce_bool(const json_node_t *value, bool default_val) {
    if (!value || json_node_is_null(value)) {
        return default_val;
    }

    if (json_node_is_string(value)) {
        const char *s = json_node_get_string(value);
        if (!s) return default_val;

        /* Copy and lowercase */
        char buf[128];
        size_t len = strlen(s);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        for (size_t i = 0; i < len; i++) {
            buf[i] = tolower((unsigned char)s[i]);
        }
        buf[len] = '\0';

        if (strcmp(buf, "true") == 0 || strcmp(buf, "1") == 0 ||
            strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0) {
            return true;
        }
        if (strcmp(buf, "false") == 0 || strcmp(buf, "0") == 0 ||
            strcmp(buf, "no") == 0 || strcmp(buf, "off") == 0) {
            return false;
        }
        return default_val;
    }

    if (json_node_is_bool(value)) {
        return json_node_get_bool(value);
    }

    if (json_node_is_number(value)) {
        return json_node_get_int(value) != 0;
    }

    return default_val;
}

/* Port of Python: _coerce_float */
double gateway_config_coerce_float(const json_node_t *value, double default_val) {
    if (!value || json_node_is_null(value)) {
        return default_val;
    }

    if (json_node_is_number(value)) {
        return json_node_get_double(value);
    }

    if (json_node_is_string(value)) {
        const char *s = json_node_get_string(value);
        if (s) {
            char *endptr;
            double val = strtod(s, &endptr);
            if (endptr != s) {
                return val;
            }
        }
    }

    return default_val;
}

/* Port of Python: _coerce_int */
int gateway_config_coerce_int(const json_node_t *value, int default_val) {
    if (!value || json_node_is_null(value)) {
        return default_val;
    }

    if (json_node_is_number(value)) {
        return json_node_get_int(value);
    }

    if (json_node_is_string(value)) {
        const char *s = json_node_get_string(value);
        if (s) {
            char *endptr;
            long val = strtol(s, &endptr, 10);
            if (endptr != s) {
                return (int)val;
            }
        }
    }

    return default_val;
}

/* Port of Python: _coerce_optional_positive_int */
int gateway_config_coerce_optional_positive_int(const json_node_t *value, const char *key, bool *out_valid) {
    if (!value || json_node_is_null(value)) {
        if (out_valid) *out_valid = true;
        return 0; /* 0 means disabled/unset */
    }

    if (json_node_is_bool(value)) {
        hermes_log(HERMES_LOG_WARN, "gateway_config", "Ignoring invalid %s=%s (expected positive integer; 0/null disables)",
                   key, json_node_get_bool(value) ? "true" : "false");
        if (out_valid) *out_valid = false;
        return 0;
    }

    if (json_node_is_number(value)) {
        int val = json_node_get_int(value);
        if (val <= 0) {
            if (out_valid) *out_valid = true;
            return 0;
        }
        if (out_valid) *out_valid = true;
        return val;
    }

    if (json_node_is_string(value)) {
        const char *s = json_node_get_string(value);
        if (!s) {
            hermes_log(HERMES_LOG_WARN, "gateway_config", "Ignoring invalid %s=%s (expected positive integer; 0/null disables)",
                       key, "null");
            if (out_valid) *out_valid = false;
            return 0;
        }

        char *endptr;
        long val = strtol(s, &endptr, 10);
        if (endptr == s || *endptr != '\0') {
            hermes_log(HERMES_LOG_WARN, "gateway_config", "Ignoring invalid %s=%s (expected positive integer; 0/null disables)",
                       key, s);
            if (out_valid) *out_valid = false;
            return 0;
        }
        if (val <= 0) {
            if (out_valid) *out_valid = true;
            return 0;
        }
        if (out_valid) *out_valid = true;
        return (int)val;
    }

    hermes_log(HERMES_LOG_WARN, "gateway_config", "Ignoring invalid %s (expected positive integer; 0/null disables)", key);
    if (out_valid) *out_valid = false;
    return 0;
}

/* ================================================================
 *  Normalization helpers (Port of Python gateway/config.py)
 * ================================================================ */

/* Port of Python: _normalize_unauthorized_dm_behavior */
const char *gateway_config_normalize_unauthorized_dm_behavior(const char *value, const char *default_val) {
    if (!value) return default_val;

    /* Convert to lowercase for comparison */
    char buf[64];
    size_t len = strlen(value);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = tolower((unsigned char)value[i]);
    }
    buf[len] = '\0';

    if (strcmp(buf, "pair") == 0 || strcmp(buf, "ignore") == 0) {
        return value; /* Return original for exact match preservation */
    }
    return default_val;
}

/* Port of Python: _normalize_notice_delivery */
const char *gateway_config_normalize_notice_delivery(const char *value, const char *default_val) {
    if (!value) return default_val;

    char buf[64];
    size_t len = strlen(value);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = tolower((unsigned char)value[i]);
    }
    buf[len] = '\0';

    if (strcmp(buf, "public") == 0 || strcmp(buf, "private") == 0) {
        return value;
    }
    return default_val;
}

/* ================================================================
 *  Helper functions (Port of Python gateway/config.py)
 * ================================================================ */

/* Port of Python: _ensure_platform_extra_dict */
bool gateway_config_ensure_platform_extra(json_node_t *platforms_data, const char *name,
                                           json_node_t **out_plat_data, json_node_t **out_extra) {
    if (!platforms_data || !json_node_is_object(platforms_data) || !name) {
        return false;
    }

    json_node_t *plat_data = json_object_get(platforms_data, name);
    if (!plat_data) {
        plat_data = json_object();
        json_object_set(platforms_data, name, plat_data);
    } else if (!json_node_is_object(plat_data)) {
        plat_data = json_object();
        json_object_set(platforms_data, name, plat_data);
    }

    json_node_t *extra = json_object_get(plat_data, "extra");
    if (!extra) {
        extra = json_object();
        json_object_set(plat_data, "extra", extra);
    } else if (!json_node_is_object(extra)) {
        extra = json_object();
        json_object_set(plat_data, "extra", extra);
    }

    if (out_plat_data) *out_plat_data = plat_data;
    if (out_extra) *out_extra = extra;
    return true;
}

/* ================================================================
 *  Gateway config structures
 * ================================================================ */

#define GW_MAX_HOME_CHANNELS 16
#define GW_MAX_PLATFORMS_CONFIG 32
#define GW_MAX_PLATFORM_NAME 64
#define GW_MAX_CHANNEL_NAME 128

typedef enum {
    GW_PLATFORM_TELEGRAM,
    GW_PLATFORM_DISCORD,
    GW_PLATFORM_WHATSAPP,
    GW_PLATFORM_WHATSAPP_CLOUD,
    GW_PLATFORM_SLACK,
    GW_PLATFORM_SIGNAL,
    GW_PLATFORM_MATTERMOST,
    GW_PLATFORM_MATRIX,
    GW_PLATFORM_HOMEASSISTANT,
    GW_PLATFORM_EMAIL,
    GW_PLATFORM_SMS,
    GW_PLATFORM_DINGTALK,
    GW_PLATFORM_API_SERVER,
    GW_PLATFORM_WEBHOOK,
    GW_PLATFORM_MSGRAPH_WEBHOOK,
    GW_PLATFORM_FEISHU,
    GW_PLATFORM_WECOM,
    GW_PLATFORM_WECOM_CALLBACK,
    GW_PLATFORM_WEIXIN,
    GW_PLATFORM_BLUEBUBBLES,
    GW_PLATFORM_QQBOT,
    GW_PLATFORM_YUANBAO,
    GW_PLATFORM_LOCAL,
    GW_PLATFORM_COUNT
} gw_platform_type_t;

typedef struct {
    gw_platform_type_t platform;
    char chat_id[GW_MAX_CHANNEL_NAME];
    char name[128];
    char thread_id[64];
} gw_home_channel_t;

/* PoP: to_dict @ gateway/config.py:to_dict */
/* Port of Python gateway/config.py:to_dict(). */
bool gw_home_channel_to_json(const gw_home_channel_t *hc, json_node_t *obj) {
    if (!hc || !obj) return false;
    json_object_set(obj, "platform", json_int(hc->platform));
    json_object_set(obj, "chat_id", json_string(hc->chat_id));
    json_object_set(obj, "name", json_string(hc->name));
    if (hc->thread_id[0]) {
        json_object_set(obj, "thread_id", json_string(hc->thread_id));
    }
    return true;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
/* Port of Python gateway/config.py:from_dict(). */
bool gw_home_channel_from_json(const json_node_t *obj, gw_home_channel_t *hc) {
    if (!hc || !obj || !json_node_is_object(obj)) return false;

    const json_node_t *platform_node = json_object_get(obj, "platform");
    const json_node_t *chat_id_node = json_object_get(obj, "chat_id");
    const json_node_t *name_node = json_object_get(obj, "name");
    const json_node_t *thread_id_node = json_object_get(obj, "thread_id");

    if (!platform_node || !chat_id_node || !name_node) return false;

    hc->platform = (gw_platform_type_t)json_node_get_int(platform_node);
    strncpy(hc->chat_id, json_node_get_string(chat_id_node) ?: "", sizeof(hc->chat_id) - 1);
    strncpy(hc->name, json_node_get_string(name_node) ?: "Home", sizeof(hc->name) - 1);

    if (thread_id_node) {
        strncpy(hc->thread_id, json_node_get_string(thread_id_node) ?: "", sizeof(hc->thread_id) - 1);
    } else {
        hc->thread_id[0] = '\0';
    }
    return true;
}

typedef enum {
    GW_RESET_MODE_DAILY,
    GW_RESET_MODE_IDLE,
    GW_RESET_MODE_BOTH,
    GW_RESET_MODE_NONE
} gw_reset_mode_t;

static const char *gw_reset_mode_strings[] = {
    [GW_RESET_MODE_DAILY] = "daily",
    [GW_RESET_MODE_IDLE] = "idle",
    [GW_RESET_MODE_BOTH] = "both",
    [GW_RESET_MODE_NONE] = "none"
};

typedef struct {
    gw_reset_mode_t mode;
    int at_hour;           /* 0-23 */
    int idle_minutes;      /* inactivity timeout */
    bool notify;
    char notify_exclude_platforms[256]; /* comma-separated */
} gw_session_reset_policy_t;

/* PoP: to_dict @ gateway/config.py:to_dict */
/* Port of Python gateway/config.py:to_dict(). */
bool gw_session_reset_policy_to_json(const gw_session_reset_policy_t *p, json_node_t *obj) {
    if (!p || !obj) return false;
    json_object_set(obj, "mode", json_string(gw_reset_mode_strings[p->mode]));
    json_object_set(obj, "at_hour", json_int(p->at_hour));
    json_object_set(obj, "idle_minutes", json_int(p->idle_minutes));
    json_object_set(obj, "notify", json_bool(p->notify));
    json_object_set(obj, "notify_exclude_platforms", json_string(p->notify_exclude_platforms));
    return true;
}

gw_reset_mode_t gw_reset_mode_from_string(const char *s) {
    if (!s) return GW_RESET_MODE_BOTH;
    if (strcmp(s, "daily") == 0) return GW_RESET_MODE_DAILY;
    if (strcmp(s, "idle") == 0) return GW_RESET_MODE_IDLE;
    if (strcmp(s, "none") == 0) return GW_RESET_MODE_NONE;
    return GW_RESET_MODE_BOTH;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
/* Port of Python gateway/config.py:from_dict(). */
bool gw_session_reset_policy_from_json(const json_node_t *obj, gw_session_reset_policy_t *p) {
    if (!p || !obj || !json_node_is_object(obj)) return false;

    const json_node_t *mode_node = json_object_get(obj, "mode");
    const json_node_t *at_hour_node = json_object_get(obj, "at_hour");
    const json_node_t *idle_node = json_object_get(obj, "idle_minutes");
    const json_node_t *notify_node = json_object_get(obj, "notify");
    const json_node_t *exclude_node = json_object_get(obj, "notify_exclude_platforms");

    p->mode = mode_node ? gw_reset_mode_from_string(json_node_get_string(mode_node)) : GW_RESET_MODE_BOTH;
    p->at_hour = at_hour_node ? json_node_get_int(at_hour_node) : 4;
    p->idle_minutes = idle_node ? json_node_get_int(idle_node) : 1440;
    p->notify = notify_node ? gateway_config_coerce_bool(notify_node, true) : true;

    if (exclude_node) {
        strncpy(p->notify_exclude_platforms, json_node_get_string(exclude_node) ?: "api_server,webhook",
                sizeof(p->notify_exclude_platforms) - 1);
    } else {
        strcpy(p->notify_exclude_platforms, "api_server,webhook");
    }
    return true;
}

typedef struct {
    bool enabled;
    char token[512];
    char api_key[512];
    char name[32]; /* platform key (e.g. "telegram") */
    gw_home_channel_t home_channel;
    bool has_home_channel;
    char reply_to_mode[16]; /* "off", "first", "all" */
    bool gateway_restart_notification;
    json_node_t *extra; /* extra platform-specific settings */
} gw_platform_config_t;

/* PoP: to_dict @ gateway/config.py:to_dict */
/* Port of Python gateway/config.py:to_dict(). */
bool gw_platform_config_to_json(const gw_platform_config_t *pc, json_node_t *obj) {
    if (!pc || !obj) return false;

    json_object_set(obj, "enabled", json_bool(pc->enabled));
    if (pc->token[0]) json_object_set(obj, "token", json_string(pc->token));
    if (pc->api_key[0]) json_object_set(obj, "api_key", json_string(pc->api_key));

    if (pc->has_home_channel) {
        json_node_t *hc_obj = json_object();
        gw_home_channel_to_json(&pc->home_channel, hc_obj);
        json_object_set(obj, "home_channel", hc_obj);
    }

    json_object_set(obj, "reply_to_mode", json_string(pc->reply_to_mode[0] ? pc->reply_to_mode : "first"));
    json_object_set(obj, "gateway_restart_notification", json_bool(pc->gateway_restart_notification));

    if (pc->extra) {
        json_object_set(obj, "extra", pc->extra);
    }

    return true;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
/* Port of Python gateway/config.py:from_dict(). */
bool gw_platform_config_from_json(const json_node_t *obj, gw_platform_config_t *pc) {
    if (!pc || !obj || !json_node_is_object(obj)) return false;

    memset(pc, 0, sizeof(*pc));

    const json_node_t *enabled_node = json_object_get(obj, "enabled");
    const json_node_t *token_node = json_object_get(obj, "token");
    const json_node_t *api_key_node = json_object_get(obj, "api_key");
    const json_node_t *home_channel_node = json_object_get(obj, "home_channel");
    const json_node_t *reply_to_mode_node = json_object_get(obj, "reply_to_mode");
    const json_node_t *grn_node = json_object_get(obj, "gateway_restart_notification");
    const json_node_t *extra_node = json_object_get(obj, "extra");

    pc->enabled = enabled_node ? gateway_config_coerce_bool(enabled_node, false) : false;

    if (token_node) strncpy(pc->token, json_node_get_string(token_node) ?: "", sizeof(pc->token) - 1);
    if (api_key_node) strncpy(pc->api_key, json_node_get_string(api_key_node) ?: "", sizeof(pc->api_key) - 1);
    if (home_channel_node) {
        pc->has_home_channel = gw_home_channel_from_json(home_channel_node, &pc->home_channel);
    }
    strncpy(pc->reply_to_mode, reply_to_mode_node ? json_node_get_string(reply_to_mode_node) ?: "first" : "first",
            sizeof(pc->reply_to_mode) - 1);
    pc->gateway_restart_notification = grn_node ? gateway_config_coerce_bool(grn_node, true) : true;

    if (extra_node && json_node_is_object(extra_node)) {
        pc->extra = json_node_copy(extra_node);
    }

    return true;
}

/* Streaming config defaults */
#define DEFAULT_STREAMING_EDIT_INTERVAL 0.8
#define DEFAULT_STREAMING_BUFFER_THRESHOLD 24
#define DEFAULT_STREAMING_CURSOR " ▉"

typedef enum {
    GW_STREAM_TRANSPORT_AUTO,
    GW_STREAM_TRANSPORT_DRAFT,
    GW_STREAM_TRANSPORT_EDIT,
    GW_STREAM_TRANSPORT_OFF
} gw_stream_transport_t;

static const char *gw_stream_transport_strings[] = {
    [GW_STREAM_TRANSPORT_AUTO] = "auto",
    [GW_STREAM_TRANSPORT_DRAFT] = "draft",
    [GW_STREAM_TRANSPORT_EDIT] = "edit",
    [GW_STREAM_TRANSPORT_OFF] = "off"
};

typedef struct {
    bool enabled;
    gw_stream_transport_t transport;
    double edit_interval;
    int buffer_threshold;
    char cursor[8];
    double fresh_final_after_seconds;
} gw_streaming_config_t;

/* PoP: to_dict @ gateway/config.py:to_dict */
/* Port of Python gateway/config.py:to_dict(). */
bool gw_streaming_config_to_json(const gw_streaming_config_t *sc, json_node_t *obj) {
    if (!sc || !obj) return false;
    json_object_set(obj, "enabled", json_bool(sc->enabled));
    json_object_set(obj, "transport", json_string(gw_stream_transport_strings[sc->transport]));
    json_object_set(obj, "edit_interval", json_double(sc->edit_interval));
    json_object_set(obj, "buffer_threshold", json_int(sc->buffer_threshold));
    json_object_set(obj, "cursor", json_string(sc->cursor));
    json_object_set(obj, "fresh_final_after_seconds", json_double(sc->fresh_final_after_seconds));
    return true;
}

gw_stream_transport_t gw_stream_transport_from_string(const char *s) {
    if (!s) return GW_STREAM_TRANSPORT_AUTO;
    if (strcmp(s, "draft") == 0) return GW_STREAM_TRANSPORT_DRAFT;
    if (strcmp(s, "edit") == 0) return GW_STREAM_TRANSPORT_EDIT;
    if (strcmp(s, "off") == 0) return GW_STREAM_TRANSPORT_OFF;
    return GW_STREAM_TRANSPORT_AUTO;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
/* Port of Python gateway/config.py:from_dict(). */
bool gw_streaming_config_from_json(const json_node_t *obj, gw_streaming_config_t *sc) {
    if (!sc || !obj || !json_node_is_object(obj)) return false;

    const json_node_t *enabled_node = json_object_get(obj, "enabled");
    const json_node_t *transport_node = json_object_get(obj, "transport");
    const json_node_t *ei_node = json_object_get(obj, "edit_interval");
    const json_node_t *bt_node = json_object_get(obj, "buffer_threshold");
    const json_node_t *cursor_node = json_object_get(obj, "cursor");
    const json_node_t *ffas_node = json_object_get(obj, "fresh_final_after_seconds");

    memset(sc, 0, sizeof(*sc));
    sc->enabled = enabled_node ? gateway_config_coerce_bool(enabled_node, false) : false;
    sc->transport = transport_node ? gw_stream_transport_from_string(json_node_get_string(transport_node)) : GW_STREAM_TRANSPORT_AUTO;
    sc->edit_interval = ei_node ? gateway_config_coerce_float(ei_node, DEFAULT_STREAMING_EDIT_INTERVAL) : DEFAULT_STREAMING_EDIT_INTERVAL;
    sc->buffer_threshold = bt_node ? gateway_config_coerce_int(bt_node, DEFAULT_STREAMING_BUFFER_THRESHOLD) : DEFAULT_STREAMING_BUFFER_THRESHOLD;
    strncpy(sc->cursor, cursor_node ? json_node_get_string(cursor_node) ?: DEFAULT_STREAMING_CURSOR : DEFAULT_STREAMING_CURSOR, sizeof(sc->cursor) - 1);
    sc->fresh_final_after_seconds = ffas_node ? gateway_config_coerce_float(ffas_node, 0.0) : 0.0;

    return true;
}

/* Platform connectivity checkers */
typedef bool (*gw_platform_connected_checker_t)(const gw_platform_config_t *cfg);

static bool gw_check_connected_weixin(const gw_platform_config_t *cfg) {
    return cfg && (cfg->extra && json_object_get(cfg->extra, "account_id") &&
            (cfg->token[0] || (cfg->extra && json_object_get(cfg->extra, "token"))));
}

static bool gw_check_connected_whatsapp(const gw_platform_config_t *cfg) {
    (void)cfg;
    return true; /* bridge handles auth */
}

static bool gw_check_connected_whatsapp_cloud(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra &&
           json_object_get(cfg->extra, "phone_number_id") &&
           json_object_get(cfg->extra, "access_token");
}

static bool gw_check_connected_signal(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra && json_object_get(cfg->extra, "http_url");
}

static bool gw_check_connected_email(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra && json_object_get(cfg->extra, "address");
}

static bool gw_check_connected_sms(const gw_platform_config_t *cfg) {
    (void)cfg;
    return getenv("TWILIO_ACCOUNT_SID") != NULL;
}

static bool gw_check_connected_api_server(const gw_platform_config_t *cfg) {
    (void)cfg;
    return true;
}

static bool gw_check_connected_webhook(const gw_platform_config_t *cfg) {
    (void)cfg;
    return true;
}

static bool gw_check_connected_msgraph_webhook(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra && json_object_get(cfg->extra, "client_state");
}

static bool gw_check_connected_feishu(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra && json_object_get(cfg->extra, "app_id");
}

static bool gw_check_connected_wecom(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra && json_object_get(cfg->extra, "bot_id");
}

static bool gw_check_connected_wecom_callback(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra &&
           (json_object_get(cfg->extra, "corp_id") || json_object_get(cfg->extra, "apps"));
}

static bool gw_check_connected_bluebubbles(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra &&
           json_object_get(cfg->extra, "server_url") && json_object_get(cfg->extra, "password");
}

static bool gw_check_connected_qqbot(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra &&
           json_object_get(cfg->extra, "app_id") && json_object_get(cfg->extra, "client_secret");
}

static bool gw_check_connected_yuanbao(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra &&
           json_object_get(cfg->extra, "app_id") && json_object_get(cfg->extra, "app_secret");
}

static bool gw_check_connected_dingtalk(const gw_platform_config_t *cfg) {
    return cfg && cfg->extra &&
           ((json_object_get(cfg->extra, "client_id") || getenv("DINGTALK_CLIENT_ID")) &&
            (json_object_get(cfg->extra, "client_secret") || getenv("DINGTALK_CLIENT_SECRET")));
}

/* Gateway config - main structure */
typedef struct {
    gw_platform_config_t platforms[GW_MAX_PLATFORMS_CONFIG];
    int platform_count;
    gw_session_reset_policy_t reset_policy;
    gw_streaming_config_t streaming;
    int max_concurrent_sessions; /* 0 = unlimited */
} gateway_config_t;

/* Port of Python: load_gateway_config */
/* PoP: load_gateway_config @ gateway/config.py:load_gateway_config */
bool gateway_config_load(const char *config_dir, gateway_config_t *out_cfg) {
    if (!out_cfg) return false;

    /* Load base hermes config */
    hermes_config_t hcfg;
    if (!hermes_config_load(&hcfg, config_dir)) {
        return false;
    }

    /* Apply env overrides */
    hermes_config_load_env(&hcfg);

    memset(out_cfg, 0, sizeof(*out_cfg));

    /* Default reset policy */
    out_cfg->reset_policy.mode = GW_RESET_MODE_BOTH;
    out_cfg->reset_policy.at_hour = 4;
    out_cfg->reset_policy.idle_minutes = 1440;
    out_cfg->reset_policy.notify = true;
    strcpy(out_cfg->reset_policy.notify_exclude_platforms, "api_server,webhook");

    /* Default streaming */
    out_cfg->streaming.enabled = false;
    out_cfg->streaming.transport = GW_STREAM_TRANSPORT_AUTO;
    out_cfg->streaming.edit_interval = DEFAULT_STREAMING_EDIT_INTERVAL;
    out_cfg->streaming.buffer_threshold = DEFAULT_STREAMING_BUFFER_THRESHOLD;
    strcpy(out_cfg->streaming.cursor, DEFAULT_STREAMING_CURSOR);
    out_cfg->streaming.fresh_final_after_seconds = 0.0;

    out_cfg->max_concurrent_sessions = 0;

    /* Load YAML for gateway-specific config */
    char config_path[1024];
    const char *base_dir = config_dir ? config_dir : getenv("HERMES_HOME");
    if (!base_dir) base_dir = "~/.slermes";

    // Expand ~
    if (base_dir[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(config_path, sizeof(config_path), "%s%s", home, base_dir + 1);
            base_dir = config_path;
        }
    }

    snprintf(config_path, sizeof(config_path), "%s/config.yaml", base_dir);

    FILE *f = fopen(config_path, "r");
    if (!f) {
        /* No config file - use defaults */
        return true;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *yaml_content = malloc(fsize + 1);
    if (!yaml_content) {
        fclose(f);
        return false;
    }

    fread(yaml_content, 1, fsize, f);
    yaml_content[fsize] = '\0';
    fclose(f);

    /* Parse YAML using libyaml */
    json_node_t *yaml_doc = json_parse_yaml(yaml_content);
    free(yaml_content);

    if (!yaml_doc) {
        hermes_log(HERMES_LOG_WARN, "gateway_config", "Failed to parse config.yaml, using defaults");
        return true;
    }

    /* Extract gateway section */
    json_node_t *gateway_section = json_object_get(yaml_doc, "gateway");
    if (!gateway_section || !json_node_is_object(gateway_section)) {
        json_free(yaml_doc);
        return true; /* No gateway section - use defaults */
    }

    /* Parse platforms */
    json_node_t *platforms_obj = json_object_get(gateway_section, "platforms");
    if (platforms_obj && json_node_is_object(platforms_obj)) {
        const char *platform_names[] = {
            "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
            "signal", "mattermost", "matrix", "homeassistant", "email",
            "sms", "dingtalk", "api_server", "webhook", "msgraph_webhook",
            "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
            "qqbot", "yuanbao", "local"
        };

        for (int i = 0; i < GW_MAX_PLATFORMS_CONFIG && i < GW_PLATFORM_COUNT; i++) {
            json_node_t *plat_obj = json_object_get(platforms_obj, platform_names[i]);
            if (plat_obj && json_node_is_object(plat_obj)) {
                out_cfg->platforms[i].has_home_channel = false;
                if (gw_platform_config_from_json(plat_obj, &out_cfg->platforms[i])) {
                    strncpy(out_cfg->platforms[i].name, platform_names[i],
                            sizeof(out_cfg->platforms[i].name) - 1);
                    out_cfg->platforms[i].name[sizeof(out_cfg->platforms[i].name) - 1] = '\0';
                    out_cfg->platform_count++;
                }
            }
        }
    }

    /* Parse reset_policy */
    json_node_t *reset_obj = json_object_get(gateway_section, "reset_policy");
    if (reset_obj && json_node_is_object(reset_obj)) {
        gw_session_reset_policy_from_json(reset_obj, &out_cfg->reset_policy);
    }

    /* Parse streaming */
    json_node_t *streaming_obj = json_object_get(gateway_section, "streaming");
    if (streaming_obj && json_node_is_object(streaming_obj)) {
        gw_streaming_config_from_json(streaming_obj, &out_cfg->streaming);
    }

    /* Parse max_concurrent_sessions */
    json_node_t *mcs_node = json_object_get(gateway_section, "max_concurrent_sessions");
    if (mcs_node) {
        out_cfg->max_concurrent_sessions = gateway_config_coerce_int(mcs_node, 0);
    }

    json_free(yaml_doc);
    return true;
}

/* Port of Python: _validate_gateway_config */
bool gateway_config_validate(const gateway_config_t *cfg, json_node_t *issues_obj) {
    if (!cfg) return false;

    bool valid = true;

    /* Validate platforms */
    for (int i = 0; i < cfg->platform_count; i++) {
        const gw_platform_config_t *pc = &cfg->platforms[i];
        if (pc->enabled && !pc->token[0] && !pc->api_key[0]) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Platform %d enabled but no token or api_key configured", i);
            if (issues_obj) {
                json_object_set(issues_obj, "platform_missing_token", json_string(msg));
            }
            valid = false;
        }
    }

    /* Validate reset policy */
    if (cfg->reset_policy.at_hour < 0 || cfg->reset_policy.at_hour > 23) {
        if (issues_obj) {
            json_object_set(issues_obj, "invalid_at_hour", json_string("at_hour must be 0-23"));
        }
        valid = false;
    }

    if (cfg->reset_policy.idle_minutes < 0) {
        if (issues_obj) {
            json_object_set(issues_obj, "invalid_idle_minutes", json_string("idle_minutes must be positive"));
        }
        valid = false;
    }

    /* Validate streaming */
    if (cfg->streaming.edit_interval <= 0.0) {
        if (issues_obj) {
            json_object_set(issues_obj, "invalid_edit_interval", json_string("edit_interval must be positive"));
        }
        valid = false;
    }

    if (cfg->streaming.buffer_threshold <= 0) {
        if (issues_obj) {
            json_object_set(issues_obj, "invalid_buffer_threshold", json_string("buffer_threshold must be positive"));
        }
        valid = false;
    }

    return valid;

/* Port of Python: _apply_env_overrides */
void gateway_config_apply_env_overrides(gateway_config_t *cfg) {
    if (!cfg) return;

    /* Environment variables can override gateway config */
    const char *env_token = getenv("GATEWAY_TELEGRAM_TOKEN");
    if (env_token && env_token[0]) {
        /* Find telegram platform config */
        for (int i = 0; i < cfg->platform_count; i++) {
            if (cfg->platforms[i].enabled && !cfg->platforms[i].token[0]) {
                strncpy(cfg->platforms[i].token, env_token, sizeof(cfg->platforms[i].token) - 1);
                break;
            }
        }
    }

    /* Max concurrent sessions */
    const char *env_mcs = getenv("GATEWAY_MAX_CONCURRENT_SESSIONS");
    if (env_mcs) {
        char *endptr;
        long val = strtol(env_mcs, &endptr, 10);
        if (endptr != env_mcs) {
                    cfg->max_concurrent_sessions = (int)val;
        }
    }
}
}

/* Check if a platform is connected */
bool gateway_config_platform_connected(const gateway_config_t *cfg, int platform_idx) {
    if (!cfg || platform_idx < 0 || platform_idx >= cfg->platform_count) return false;

    const gw_platform_config_t *pc = &cfg->platforms[platform_idx];
    if (!pc->enabled) return false;

    switch (platform_idx) {
        case GW_PLATFORM_WEIXIN:
            return gw_check_connected_weixin(pc);
        case GW_PLATFORM_WHATSAPP:
            return gw_check_connected_whatsapp(pc);
        case GW_PLATFORM_WHATSAPP_CLOUD:
            return gw_check_connected_whatsapp_cloud(pc);
        case GW_PLATFORM_SIGNAL:
            return gw_check_connected_signal(pc);
        case GW_PLATFORM_EMAIL:
            return gw_check_connected_email(pc);
        case GW_PLATFORM_SMS:
            return gw_check_connected_sms(pc);
        case GW_PLATFORM_API_SERVER:
            return gw_check_connected_api_server(pc);
        case GW_PLATFORM_WEBHOOK:
            return gw_check_connected_webhook(pc);
        case GW_PLATFORM_MSGRAPH_WEBHOOK:
            return gw_check_connected_msgraph_webhook(pc);
        case GW_PLATFORM_FEISHU:
            return gw_check_connected_feishu(pc);
        case GW_PLATFORM_WECOM:
            return gw_check_connected_wecom(pc);
        case GW_PLATFORM_WECOM_CALLBACK:
            return gw_check_connected_wecom_callback(pc);
        case GW_PLATFORM_BLUEBUBBLES:
            return gw_check_connected_bluebubbles(pc);
        case GW_PLATFORM_QQBOT:
            return gw_check_connected_qqbot(pc);
        case GW_PLATFORM_YUANBAO:
            return gw_check_connected_yuanbao(pc);
        case GW_PLATFORM_DINGTALK:
            return gw_check_connected_dingtalk(pc);
        default:
            /* Generic: token or api_key */
            return pc->token[0] != '\0' || pc->api_key[0] != '\0';
    }
}

/* ================================================================
 *  GatewayConfig helper functions (Port of Python gateway/config.py)
 * ================================================================ */

/* Port of Python: get_connected_platforms
 * Returns a JSON array of connected platform names. */
/* PoP: get_connected_platforms @ gateway/config.py:get_connected_platforms */
/* Port of Python gateway/config.py:get_connected_platforms(). */
json_node_t *gateway_config_get_connected_platforms(const gateway_config_t *cfg)
{
    if (!cfg) return json_array();

    json_node_t *arr = json_array();
    const char *platform_names[] = {
        "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
        "signal", "mattermost", "matrix", "homeassistant", "email",
        "sms", "dingtalk", "api_server", "webhook", "msgraph_webhook",
        "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
        "qqbot", "yuanbao", "local"
    };

    for (int i = 0; i < cfg->platform_count; i++) {
        if (gateway_config_platform_connected(cfg, i)) {
            json_append(arr, json_string(platform_names[i]));
        }
    }
    return arr;
}

/* PoP: _is_platform_connected @ gateway/config.py:_is_platform_connected */
/* Port of Python gateway/config.py:_is_platform_connected(). */
bool gateway_config_is_platform_connected(const gateway_config_t *cfg, int platform_idx)
{
    return gateway_config_platform_connected(cfg, platform_idx);
}

/* Port of Python: get_home_channel
 * Get the home channel for a platform. Returns malloc'd JSON, caller must free. */
/* PoP: get_home_channel @ gateway/config.py:get_home_channel */
/* Port of Python gateway/config.py:get_home_channel(). */
json_node_t *gateway_config_get_home_channel(const gateway_config_t *cfg, int platform_idx)
{
    if (!cfg || platform_idx < 0 || platform_idx >= cfg->platform_count) {
        return NULL;
    }

    const gw_platform_config_t *pc = &cfg->platforms[platform_idx];
    if (!pc->has_home_channel) {
        return NULL;
    }

    json_node_t *obj = json_object();
    if (!obj) return NULL;

    gw_home_channel_to_json(&pc->home_channel, obj);
    return obj;
}

/* Port of Python: get_reset_policy
 * Get the session reset policy as JSON. Returns malloc'd JSON, caller must free. */
/* PoP: get_reset_policy @ gateway/config.py:get_reset_policy */
/* Port of Python gateway/config.py:get_reset_policy(). */
json_node_t *gateway_config_get_reset_policy(const gateway_config_t *cfg)
{
    if (!cfg) return NULL;

    json_node_t *obj = json_object();
    if (!obj) return NULL;

    gw_session_reset_policy_to_json(&cfg->reset_policy, obj);
    return obj;
}

/* PoP: get_unauthorized_dm_behavior @ gateway/config.py:get_unauthorized_dm_behavior */
/* Port of Python gateway/config.py:get_unauthorized_dm_behavior(). */
const char *gateway_config_get_unauthorized_dm_behavior(const gateway_config_t *cfg)
{
    if (!cfg) return "pair";

    /* Check the first enabled platform's extra settings for unauthorized_dm_behavior */
    for (int i = 0; i < cfg->platform_count; i++) {
        const gw_platform_config_t *pc = &cfg->platforms[i];
        if (pc->enabled && pc->extra) {
            const json_node_t *val = json_obj_get(pc->extra, "unauthorized_dm_behavior");
            if (val && json_node_is_string(val)) {
                return json_node_get_string(val);
            }
        }
    }
    return "pair";
}

/* PoP: get_notice_delivery @ gateway/config.py:get_notice_delivery */
/* Port of Python gateway/config.py:get_notice_delivery(). */
const char *gateway_config_get_notice_delivery(const gateway_config_t *cfg)
{
    if (!cfg) return "public";

    /* Check the first enabled platform's extra settings for notice_delivery */
    for (int i = 0; i < cfg->platform_count; i++) {
        const gw_platform_config_t *pc = &cfg->platforms[i];
        if (pc->enabled && pc->extra) {
            const json_node_t *val = json_obj_get(pc->extra, "notice_delivery");
            if (val && json_node_is_string(val)) {
                return json_node_get_string(val);
            }
        }
    }
    return "public";
}

/* ================================================================
 *  Top-level gateway_config_t serialization (Port of Python GatewayConfig.to_dict/from_dict)
 * ================================================================ */

/* Port of Python: GatewayConfig.to_dict() */
bool gateway_config_to_json(const gateway_config_t *cfg, json_node_t *obj)
{
    if (!cfg || !obj) return false;

    json_node_t *platforms_obj = json_object();
    const char *platform_names[] = {
        "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
        "signal", "mattermost", "matrix", "homeassistant", "email",
        "sms", "dingtalk", "api_server", "webhook", "msgraph_webhook",
        "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
        "qqbot", "yuanbao", "local"
    };

    for (int i = 0; i < cfg->platform_count; i++) {
        json_node_t *plat_obj = json_object();
        if (gw_platform_config_to_json(&cfg->platforms[i], plat_obj)) {
            json_object_set(platforms_obj, platform_names[i], plat_obj);
        } else {
            json_free(plat_obj);
        }
    }
    json_object_set(obj, "platforms", platforms_obj);

    json_node_t *reset_obj = json_object();
    gw_session_reset_policy_to_json(&cfg->reset_policy, reset_obj);
    json_object_set(obj, "default_reset_policy", reset_obj);

    /* reset_by_platform and reset_by_type would need additional fields in gateway_config_t */
    json_object_set(obj, "reset_by_platform", json_object());
    json_object_set(obj, "reset_by_type", json_object());

    json_object_set(obj, "reset_triggers", json_string("/new,/reset"));
    json_object_set(obj, "quick_commands", json_object());
    json_object_set(obj, "sessions_dir", json_string("~/.hermes/sessions"));
    json_object_set(obj, "always_log_local", json_bool(true));
    json_object_set(obj, "filter_silence_narration", json_bool(true));
    json_object_set(obj, "stt_enabled", json_bool(true));
    json_object_set(obj, "group_sessions_per_user", json_bool(true));
    json_object_set(obj, "thread_sessions_per_user", json_bool(false));
    json_object_set(obj, "max_concurrent_sessions", json_int(cfg->max_concurrent_sessions));
    json_object_set(obj, "unauthorized_dm_behavior", json_string("pair"));

    json_node_t *streaming_obj = json_object();
    gw_streaming_config_to_json(&cfg->streaming, streaming_obj);
    json_object_set(obj, "streaming", streaming_obj);

    json_object_set(obj, "session_store_max_age_days", json_int(90));

    return true;
}

/* Port of Python: GatewayConfig.from_dict() */
bool gateway_config_from_json(const json_node_t *obj, gateway_config_t *cfg)
{
    if (!cfg || !obj || !json_node_is_object(obj)) return false;

    memset(cfg, 0, sizeof(*cfg));

    /* Default reset policy */
    cfg->reset_policy.mode = GW_RESET_MODE_BOTH;
    cfg->reset_policy.at_hour = 4;
    cfg->reset_policy.idle_minutes = 1440;
    cfg->reset_policy.notify = true;
    strcpy(cfg->reset_policy.notify_exclude_platforms, "api_server,webhook");

    /* Default streaming */
    cfg->streaming.enabled = false;
    cfg->streaming.transport = GW_STREAM_TRANSPORT_AUTO;
    cfg->streaming.edit_interval = DEFAULT_STREAMING_EDIT_INTERVAL;
    cfg->streaming.buffer_threshold = DEFAULT_STREAMING_BUFFER_THRESHOLD;
    strcpy(cfg->streaming.cursor, DEFAULT_STREAMING_CURSOR);
    cfg->streaming.fresh_final_after_seconds = 0.0;

    cfg->max_concurrent_sessions = 0;

    /* Parse platforms */
    json_node_t *platforms_obj = json_object_get(obj, "platforms");
    if (platforms_obj && json_node_is_object(platforms_obj)) {
        const char *platform_names[] = {
            "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
            "signal", "mattermost", "matrix", "homeassistant", "email",
            "sms", "dingtalk", "api_server", "webhook", "msgraph_webhook",
            "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
            "qqbot", "yuanbao", "local"
        };

        for (int i = 0; i < GW_MAX_PLATFORMS_CONFIG && i < GW_PLATFORM_COUNT; i++) {
            json_node_t *plat_obj = json_object_get(platforms_obj, platform_names[i]);
            if (plat_obj && json_node_is_object(plat_obj)) {
                cfg->platforms[i].has_home_channel = false;
                if (gw_platform_config_from_json(plat_obj, &cfg->platforms[i])) {
                    strncpy(cfg->platforms[i].name, platform_names[i],
                            sizeof(cfg->platforms[i].name) - 1);
                    cfg->platforms[i].name[sizeof(cfg->platforms[i].name) - 1] = '\0';
                    cfg->platform_count++;
                }
            }
        }
    }

    /* Parse reset_policy */
    json_node_t *reset_obj = json_object_get(obj, "default_reset_policy");
    if (reset_obj && json_node_is_object(reset_obj)) {
        gw_session_reset_policy_from_json(reset_obj, &cfg->reset_policy);
    }

    /* Parse streaming */
    json_node_t *streaming_obj = json_object_get(obj, "streaming");
    if (streaming_obj && json_node_is_object(streaming_obj)) {
        gw_streaming_config_from_json(streaming_obj, &cfg->streaming);
    }

    /* Parse max_concurrent_sessions */
    json_node_t *mcs_node = json_object_get(obj, "max_concurrent_sessions");
    if (mcs_node) {
        cfg->max_concurrent_sessions = gateway_config_coerce_int(mcs_node, 0);
    }

    return true;
}

/* ================================================================
 *  Platform enum helpers (Port of Python gateway/config.py Platform enum)
 * ================================================================ */

/* Platform name table for _missing_ lookup (Port of Python Platform._missing_) */
static const struct { const char *name; gw_platform_type_t type; } g_platform_table[] = {
    {"telegram",           GW_PLATFORM_TELEGRAM},
    {"discord",            GW_PLATFORM_DISCORD},
    {"whatsapp",           GW_PLATFORM_WHATSAPP},
    {"whatsapp_cloud",     GW_PLATFORM_WHATSAPP_CLOUD},
    {"slack",              GW_PLATFORM_SLACK},
    {"signal",             GW_PLATFORM_SIGNAL},
    {"mattermost",         GW_PLATFORM_MATTERMOST},
    {"matrix",             GW_PLATFORM_MATRIX},
    {"homeassistant",      GW_PLATFORM_HOMEASSISTANT},
    {"email",              GW_PLATFORM_EMAIL},
    {"sms",                GW_PLATFORM_SMS},
    {"dingtalk",           GW_PLATFORM_DINGTALK},
    {"api_server",         GW_PLATFORM_API_SERVER},
    {"webhook",            GW_PLATFORM_WEBHOOK},
    {"msgraph_webhook",    GW_PLATFORM_MSGRAPH_WEBHOOK},
    {"feishu",             GW_PLATFORM_FEISHU},
    {"wecom",              GW_PLATFORM_WECOM},
    {"wecom_callback",     GW_PLATFORM_WECOM_CALLBACK},
    {"weixin",             GW_PLATFORM_WEIXIN},
    {"bluebubbles",        GW_PLATFORM_BLUEBUBBLES},
    {"qqbot",              GW_PLATFORM_QQBOT},
    {"yuanbao",            GW_PLATFORM_YUANBAO},
    {"local",              GW_PLATFORM_LOCAL},
    {NULL,                 GW_PLATFORM_COUNT}
};

/* PoP: gateway_config_platform_missing @ gateway/config.py:Platform._missing_ */
/* Resolve a platform name string to enum value. Returns -1 if unknown. */
int gateway_config_platform_missing(const char *value)
{
    if (!value || !value[0]) return -1;
    size_t len = strlen(value);
    if (len > 63) len = 63;
    char buf[64];
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)value[i]);
    buf[len] = '\0';
    for (int i = 0; g_platform_table[i].name; i++) {
        if (strcmp(buf, g_platform_table[i].name) == 0)
            return (int)g_platform_table[i].type;
    }
    return -1;
}

/* PoP: gateway_config_scan_bundled_plugin_platforms @ gateway/config.py:Platform._scan_bundled_plugin_platforms */
/* Scan the bundled plugins directory and return the count of platform plugins found. */
int gateway_config_scan_bundled_plugin_platforms(char *names[], int max_names)
{
    int count = 0;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return 0;

    char plugins_dir[1024];
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins/platforms", home);

    DIR *d = opendir(plugins_dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < max_names) {
        if (entry->d_name[0] == '.') continue;
        char subdir[2048];
        snprintf(subdir, sizeof(subdir), "%s/%s", plugins_dir, entry->d_name);
        struct stat st;
        if (stat(subdir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        char init_py[2048], plugin_yaml[2048], plugin_yml[2048];
        snprintf(init_py, sizeof(init_py), "%s/__init__.py", subdir);
        snprintf(plugin_yaml, sizeof(plugin_yaml), "%s/plugin.yaml", subdir);
        snprintf(plugin_yml, sizeof(plugin_yml), "%s/plugin.yml", subdir);
        if (stat(init_py, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (stat(plugin_yaml, &st) != 0 && stat(plugin_yml, &st) != 0) continue;
        names[count] = strdup(entry->d_name);
        count++;
    }
    closedir(d);
    return count;
}

/* ================================================================
 *  Global (runtime) gateway config instance
 *  Loaded once at gateway startup into a private static so other modules
 *  (e.g. the authz mixin) can read per-platform `extra` settings via the
 *  accessors below — without config.c's gateway_config_t details leaking
 *  across the header boundary. The canonical platform-name table (index-
 *  aligned with platforms[]) is reused so name lookups need no duplicated
 *  per-platform index.
 * ================================================================ */

static gateway_config_t g_gw_config;
static bool g_gw_config_loaded = false;

/* Canonical gateway platform-name list, index-aligned with platforms[] and
 * with the platform_names[] tables used inside gateway_config_load / _from_json. */
static const char *g_gw_platform_names[] = {
    "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "homeassistant", "email",
    "sms", "dingtalk", "api_server", "webhook", "msgraph_webhook",
    "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
    "qqbot", "yuanbao", "local", NULL
};

void gateway_config_load_global(void) {
    memset(&g_gw_config, 0, sizeof(g_gw_config));
    g_gw_config_loaded = gateway_config_load(NULL, &g_gw_config);
}

/* Return the authoritative loaded global gateway config (mirrors Python
 * GatewayRunner.config / load_gateway_config() result). Returns NULL until
 * gateway_config_load_global() has run. Callers pass this to ported helpers
 * that take a config object (e.g. gw_own_policy_open_startup_violation). */
const gateway_config_t *gateway_config_get_global(void) {
    if (!g_gw_config_loaded) return NULL;
    return &g_gw_config;
}

const char *gateway_config_get_unauthorized_dm_behavior_global(void) {
    if (!g_gw_config_loaded) return "pair";
    return gateway_config_get_unauthorized_dm_behavior(&g_gw_config);
}

/* Shared lookup: find a loaded platform config by (case-insensitive) name.
 * Returns a pointer into the loaded global config, or NULL. Declared in
 * hermes_gateway_config.h so the authz mixin (and other gateway modules)
 * can reuse it without re-deriving the platform-name index. */
const gw_platform_config_t *gateway_config_find_platform(const char *name) {
    if (!name || !*name || !g_gw_config_loaded) return NULL;
    for (int i = 0; g_gw_platform_names[i]; i++) {
        if (strcasecmp(g_gw_platform_names[i], name) == 0) {
            if (i < g_gw_config.platform_count) {
                return &g_gw_config.platforms[i];
            }
            return NULL;
        }
    }
    return NULL;
}

/* Read a boolean `extra` setting for a platform by name (case-insensitive).
 * Returns false if the platform/key is absent. Env overrides are the
 * caller's responsibility (matching Python's config.extra + <PLATFORM>_*
 * env folding). */
bool gateway_config_platform_extra_bool(const char *platform, const char *key) {
    if (!platform || !key) return false;
    const gw_platform_config_t *pc = gateway_config_find_platform(platform);
    if (pc && pc->extra) {
        json_node_t *v = json_obj_get(pc->extra, key);
        if (v && json_node_is_bool(v)) return json_node_get_bool(v);
    }
    return false;
}
