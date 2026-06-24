/**
 * @defgroup hermes_gateway_config Gateway Configuration
 * @brief Gateway-specific configuration loading and validation.
 *
 * Port of Python gateway/config.py.
 * Handles loading and validating configuration for:
 * - Connected platforms (Telegram, Discord, WhatsApp, Weixin, and more)
 * - Home channels for each platform
 * - Session reset policies
 * - Delivery preferences
 * - Streaming configuration
 */

#ifndef HERMES_GATEWAY_CONFIG_H
#define HERMES_GATEWAY_CONFIG_H

#include "hermes_json.h"
#include "hermes_gateway.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum platforms in config */
#define GW_MAX_PLATFORMS_CONFIG 32
#define GW_MAX_CHANNEL_NAME 128

/* -----------------------------------------------------------------------------
 * Gateway platform types (mirrors Python Platform enum)
 * ----------------------------------------------------------------------------- */
typedef enum {
    GW_PLATFORM_TELEGRAM = 0,
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

/* -----------------------------------------------------------------------------
 * Home channel configuration
 * Mirrors Python HomeChannel dataclass
 * ----------------------------------------------------------------------------- */
typedef struct {
    gw_platform_type_t platform;
    char chat_id[GW_MAX_CHANNEL_NAME];
    char name[128];
    char thread_id[64];
} gw_home_channel_t;

/** Convert HomeChannel to JSON object */
bool gw_home_channel_to_json(const gw_home_channel_t *hc, json_node_t *obj);

/** Convert JSON object to HomeChannel */
bool gw_home_channel_from_json(const json_node_t *obj, gw_home_channel_t *hc);

/* -----------------------------------------------------------------------------
 * Session reset policy
 * Mirrors Python SessionResetPolicy dataclass
 * ----------------------------------------------------------------------------- */
typedef enum {
    GW_RESET_MODE_DAILY = 0,  /**< Reset at specific hour daily */
    GW_RESET_MODE_IDLE,       /**< Reset after N minutes of inactivity */
    GW_RESET_MODE_BOTH,       /**< Whichever triggers first */
    GW_RESET_MODE_NONE        /**< Never auto-reset */
} gw_reset_mode_t;

typedef struct {
    gw_reset_mode_t mode;
    int at_hour;              /**< Hour for daily reset (0-23) */
    int idle_minutes;         /**< Inactivity timeout in minutes */
    bool notify;              /**< Send notification on auto-reset */
    char notify_exclude_platforms[256]; /**< Comma-separated platform names to exclude */
} gw_session_reset_policy_t;

/** Convert SessionResetPolicy to JSON object */
bool gw_session_reset_policy_to_json(const gw_session_reset_policy_t *p, json_node_t *obj);

/** Convert JSON object to SessionResetPolicy */
bool gw_session_reset_policy_from_json(const json_node_t *obj, gw_session_reset_policy_t *p);

/* -----------------------------------------------------------------------------
 * Platform configuration
 * Mirrors Python PlatformConfig dataclass
 * ----------------------------------------------------------------------------- */
typedef struct {
    bool enabled;
    char token[512];
    char api_key[512];
    gw_home_channel_t home_channel;
    bool has_home_channel;
    char reply_to_mode[16];    /**< "off", "first", "all" */
    bool gateway_restart_notification;
    json_node_t *extra;        /**< Platform-specific extra settings */
} gw_platform_config_t;

/** Convert PlatformConfig to JSON object */
bool gw_platform_config_to_json(const gw_platform_config_t *pc, json_node_t *obj);

/** Convert JSON object to PlatformConfig */
bool gw_platform_config_from_json(const json_node_t *obj, gw_platform_config_t *pc);

/* -----------------------------------------------------------------------------
 * Streaming configuration
 * Mirrors Python StreamingConfig
 * ----------------------------------------------------------------------------- */
typedef enum {
    GW_STREAM_TRANSPORT_AUTO = 0,   /**< Prefer native draft, fallback to edit */
    GW_STREAM_TRANSPORT_DRAFT,      /**< Explicitly request native drafts */
    GW_STREAM_TRANSPORT_EDIT,       /**< Progressive editMessageText only */
    GW_STREAM_TRANSPORT_OFF         /**< Disable streaming entirely */
} gw_stream_transport_t;

typedef struct {
    bool enabled;
    gw_stream_transport_t transport;
    double edit_interval;           /**< Seconds between edits (default 0.8) */
    int buffer_threshold;           /**< Characters before flush (default 24) */
    char cursor[8];                 /**< Streaming cursor (default " ▉") */
    double fresh_final_after_seconds; /**< Replace preview after N seconds (default 0) */
} gw_streaming_config_t;

/** Convert StreamingConfig to JSON object */
bool gw_streaming_config_to_json(const gw_streaming_config_t *sc, json_node_t *obj);

/** Convert JSON object to StreamingConfig */
bool gw_streaming_config_from_json(const json_node_t *obj, gw_streaming_config_t *sc);

/* -----------------------------------------------------------------------------
 * Gateway configuration (main structure)
 * Mirrors Python GatewayConfig
 * ----------------------------------------------------------------------------- */
typedef struct {
    gw_platform_config_t platforms[GW_MAX_PLATFORMS_CONFIG];
    int platform_count;
    gw_session_reset_policy_t reset_policy;
    gw_streaming_config_t streaming;
    int max_concurrent_sessions;    /**< 0 = unlimited */
} gateway_config_t;

/* -----------------------------------------------------------------------------
 * Coercion helpers (Port of Python gateway/config.py)
 * ----------------------------------------------------------------------------- */

/** Port of Python: _coerce_bool
 * Coerce bool-ish config values, preserving a caller-provided default. */
bool gateway_config_coerce_bool(const json_node_t *value, bool default_val);

/** Port of Python: _coerce_float
 * Coerce numeric config values, falling back on malformed input. */
double gateway_config_coerce_float(const json_node_t *value, double default_val);

/** Port of Python: _coerce_int
 * Coerce integer config values, falling back on malformed input. */
int gateway_config_coerce_int(const json_node_t *value, int default_val);

/** Port of Python: _coerce_optional_positive_int
 * Coerce an optional positive integer config value.
 * Returns 0 if disabled/unset. Sets out_valid if provided. */
int gateway_config_coerce_optional_positive_int(const json_node_t *value, const char *key, bool *out_valid);

/* -----------------------------------------------------------------------------
 * Normalization helpers (Port of Python gateway/config.py)
 * ----------------------------------------------------------------------------- */

/** Port of Python: _normalize_unauthorized_dm_behavior
 * Normalize unauthorized DM behavior to a supported value ("pair" or "ignore"). */
const char *gateway_config_normalize_unauthorized_dm_behavior(const char *value, const char *default_val);

/** Port of Python: _normalize_notice_delivery
 * Normalize notice delivery mode to a supported value ("public" or "private"). */
const char *gateway_config_normalize_notice_delivery(const char *value, const char *default_val);

/* -----------------------------------------------------------------------------
 * Helper functions (Port of Python gateway/config.py)
 * ----------------------------------------------------------------------------- */

/** Port of Python: _ensure_platform_extra_dict
 * Get-or-create platform data and its nested "extra" dict.
 * Returns true on success, sets out_plat_data and out_extra. */
bool gateway_config_ensure_platform_extra(json_node_t *platforms_data, const char *name,
                                           json_node_t **out_plat_data, json_node_t **out_extra);

/* -----------------------------------------------------------------------------
 * Main config loading functions
 * ----------------------------------------------------------------------------- */

/** Port of Python: load_gateway_config
 * Load gateway configuration from config.yaml.
 * Returns true on success, false on error. */
bool gateway_config_load(const char *config_dir, gateway_config_t *out_cfg);

/** Port of Python: _validate_gateway_config
 * Validate gateway configuration.
 * Returns true if valid, false otherwise. Populates issues_obj with any issues. */
bool gateway_config_validate(const gateway_config_t *cfg, json_node_t *issues_obj);

/** Port of Python: _apply_env_overrides
 * Apply environment variable overrides to gateway config. */
void gateway_config_apply_env_overrides(gateway_config_t *cfg);

/** Check if a platform is sufficiently configured to be considered "connected" */
bool gateway_config_platform_connected(const gateway_config_t *cfg, int platform_idx);

/** Resolve a platform name string to enum value (Port of Python Platform._missing_). Returns -1 if unknown. */
int gateway_config_platform_missing(const char *value);

/** Scan bundled plugin platforms directory (Port of Python Platform._scan_bundled_plugin_platforms). Returns count of platform plugins found. */
int gateway_config_scan_bundled_plugin_platforms(char *names[], int max_names);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_CONFIG_H */