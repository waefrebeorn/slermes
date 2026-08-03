/**
 * @file hermes_gateway_types.h
 * @brief Gateway type definitions — queue, session, platform, and state structs.
 *
 * Self-contained type header extracted from the hermes_gateway.h god header so
 * that translation units that only need the type declarations (and the global
 * `g_gw` extern) can include this without dragging in every platform API.
 *
 * @{
 */
#ifndef HERMES_GATEWAY_TYPES_H
#define HERMES_GATEWAY_TYPES_H

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hive.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

/* ================================================================
 *  Constants
 * ================================================================ */

/* Max platforms running simultaneously */
#define GW_MAX_PLATFORMS 8

/* Max pending messages in queue */
#define GW_QUEUE_MAX 256

/* Max path length */
#define GW_PATH_MAX 4096

/* Max HTTP clients in pool */
#define GW_POOL_MAX 16

/* P102: Max gateway sessions (one per unique chat_id) */
#define GW_SESSIONS_MAX 64

/* ================================================================
 *  P101: Gateway message queue entry
 * ================================================================ */

typedef struct {
    char platform[32];
    char chat_id[128];
    char text[4096];
    char thread_id[64];     /* Telegram topic / Discord thread */
    double timestamp;        /* monotonic time when queued */
} gateway_msg_t;

/* ================================================================
 *  P101: Per-platform rate limiter (token bucket)
 * ================================================================ */

typedef struct {
    double tokens_per_sec;  /* refill rate */
    double max_tokens;      /* burst capacity */
    double tokens;          /* current tokens */
    double last_refill;     /* monotonic time of last refill */
} gw_rate_limiter_t;

/* ================================================================
 *  P101: HTTP connection pool entry
 * ================================================================ */

typedef struct {
    http_client_t *client;
    bool           in_use;
    double         last_used;  /* monotonic time */
    char           endpoint[256]; /* API base URL this client is configured for */
} gw_http_pool_entry_t;

/* ================================================================
 *  P102a: Session source metadata
 * ================================================================ */

/* Describes where a message originated from — used for system prompt
 * injection and session context. Mirrors Python SessionSource. */
typedef struct {
    char platform[32];      /* "telegram", "discord", etc. */
    char chat_id[128];      /* chat/group/channel/DM ID */
    char chat_name[256];    /* human-readable chat/channel name */
    char chat_type[32];     /* "dm", "group", "channel", "thread" */
    char user_id[128];      /* sender's user ID */
    char user_name[256];    /* sender's display name */
    char thread_id[64];     /* forum topic / thread ID */
    char chat_topic[256];   /* channel topic/description (Discord, Slack) */
    char user_id_alt[128];  /* platform-specific stable alt ID (Signal UUID, Feishu union_id) */
    char chat_id_alt[128];  /* Signal group internal ID, etc. */
    char guild_id[128];     /* Discord guild / Slack workspace / Matrix server scope */
    char parent_chat_id[128]; /* parent channel when chat_id refers to a thread */
    char message_id[128];   /* ID of the triggering message (for pin/reply/react) */
    bool is_bot;            /* true if sender is a bot */
    bool has_data;          /* true when source fields have been populated */
} gw_session_source_t;

/* ================================================================
 *  P102b: Gateway session entry
 * ================================================================ */

/* GW15: Session sources LRU cache entry (hive-backed). */
typedef struct {
    char key[192];              /* "platform:chat_id" */
    gw_session_source_t source; /* cached source metadata */
} gw_source_cache_entry_t;

/* Each unique platform:chat_id pair gets its own agent session. */
typedef struct {
    char                key[192];       /* "platform:chat_id" */
    agent_state_t       agent;
    db_t               *db;             /* session DB handle */
    double              last_active;    /* monotonic time */
    double              last_busy_ack;  /* P109: last typing indicator sent (debounce) */
    bool                in_use;
    char                session_id[64]; /* current session ID */
    gw_session_source_t source;         /* P102a: session origin metadata */
    /* GW12: Last-resolved model fallback cache */
    char                last_resolved_model[128]; /* cached model name for recovery */
    char                last_resolved_provider[64]; /* cached provider name */
    /* GW14: Pending /update prompt tracking */
    bool                update_prompt_pending;   /* true when /update prompt awaiting response */
    char                update_prompt_file[512]; /* path to .update_prompt.json sidecar */
    /* SE04: Per-session system prompt override */
    char                session_system_prompt[4096]; /* per-session system prompt (empty = use default) */
    /* SE07: Telegram topic mode binding */
    char                telegram_topic_id[64]; /* bound Telegram topic ID for this session */
    /* session.py extensions */
    char                last_message_id[128];  /* last platform message ID seen */
    char                model_override[128];   /* per-session model override */
    bool                resume_pending;        /* session resume pending flag */
    bool                suspended;              /* session suspended flag */
} gw_session_entry_t;

/* ================================================================
 *  P103: Unified platform interface
 * ================================================================ */

/* Common platform vtable. Each platform module fills this in. */
typedef struct {
    const char *name;         /* "telegram", "discord", etc. */

    /* Initialize platform from global config/env vars. Returns true on success. */
    bool (*init)(void);

    /* Send text to a chat_id. Returns true on success. */
    bool (*send)(const char *chat_id, const char *text);

    /* Send typing indicator (optional — can be NULL). */
    void (*send_typing)(const char *chat_id);

    /* Poll for new messages. Returns a JSON array of updates, or NULL.
     * The caller frees the returned JSON. NULL = no updates or error. */
    json_node_t *(*poll)(void);

    /* Start the platform's own event loop (for push-based platforms). */
    void (*start)(void);

    /* Stop the platform's event loop (for push-based platforms). */
    void (*stop)(void);

    /* Shutdown and free resources */
    void (*shutdown)(void);

    /* Send an emoji reaction to a specific message (optional — can be NULL). */
    bool (*send_reaction)(const char *chat_id, const char *message_id, const char *emoji);

    /* Platform-specific state data (opaque) */
    void *data;
} gw_platform_t;

/* ================================================================
 *  Gateway state (shared across platform modules)
 * ================================================================ */

typedef struct {
    agent_state_t   agent;
    hermes_config_t config;
    http_client_t  *http;           /* Default HTTP client (for gateway_send) */
    bool            running;
    int             poll_interval;  /* seconds between polls */
    int             platform_count;
    char            platforms[GW_MAX_PLATFORMS][32]; /* Active platform names */
    pthread_t       threads[GW_MAX_PLATFORMS];
    pthread_mutex_t agent_mutex;    /* Mutex for thread-safe agent_chat */

    /* Platform-specific state */
    int             tg_offset;      /* Telegram: last update_id + 1 */

    /* P101: Message queue (circular buffer) */
    gateway_msg_t   msg_queue[GW_QUEUE_MAX];
    volatile int    msg_queue_head; /* producer writes here */
    volatile int    msg_queue_tail; /* consumer reads from here */
    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;     /* signal when new message queued */

    /* P101: Per-platform rate limiters */
    gw_rate_limiter_t rate_limiters[GW_MAX_PLATFORMS];

    /* P101: HTTP connection pool */
    gw_http_pool_entry_t http_pool[GW_POOL_MAX];
    int                  pool_count;
    pthread_mutex_t      pool_mutex;
    double               pool_keepalive_expiry;  /* seconds before idle connection freed */

    /* P102: Per-chat session pool — hive-backed (no landlocked array) */
    hive_t *sessions;            /* of gw_session_entry_t* (heap) */
    int session_count;
    pthread_mutex_t session_mutex;
    char                 session_db_path[GW_PATH_MAX];  /* where sessions are stored */

    /* P103: Platform registry */
    gw_platform_t        platform_defs[GW_MAX_PLATFORMS];
    int                  platform_def_count;

    /* Per-platform HTTP clients (owned by thread functions) */
    http_client_t  *platform_http[GW_MAX_PLATFORMS];

    /* Per-platform HTTP keepalive config (seconds, 0=default) */
    double platform_keepalive_sec[GW_MAX_PLATFORMS];

    /* E28: Message deduplication — ring buffer of recent message IDs */
    char   dedup_ids[64][128];     /* recent message IDs */
    double dedup_timestamps[64];   /* when each ID was seen (monotonic) */
    int    dedup_head;
    int    dedup_count;
    double dedup_ttl;              /* seconds to keep dedup entries (default 5.0) */

    /* E29: Batch aggregation — coalesce fragmented messages */
    char   batch_buf[4096];        /* accumulated text */
    char   batch_platform[32];
    char   batch_chat_id[128];
    double batch_start_time;       /* when batch accumulation started */
    bool   batch_active;

    /* E31: Per-platform cooldown (seconds to wait between actions) */
    double platform_cooldown_sec[GW_MAX_PLATFORMS];
    double platform_last_action[GW_MAX_PLATFORMS];

    /* E32: Reconnect backoff — exponential backoff per platform */
    int    reconnect_attempt[GW_MAX_PLATFORMS];
    double reconnect_delay_sec[GW_MAX_PLATFORMS];
#define GW_RECONNECT_BASE_SEC   1.0
#define GW_RECONNECT_MAX_SEC    60.0
#define GW_RECONNECT_JITTER     0.1

    /* E33: Proxy per-platform */
    char   platform_proxy[GW_MAX_PLATFORMS][512];
    bool   proxy_enabled[GW_MAX_PLATFORMS];

    /* E34: Group observe — observe unmentioned messages */
    char   group_observe_prefix[64];  /* prefix to strip from group names */
    bool   group_observe_enabled;
    /* L08: Group observe buffer — accumulated unmentioned messages */
    char   observe_buffer[65536];     /* rolling buffer of observed messages */
    pthread_mutex_t observe_mutex;

    /* P102: Auto-continue freshness window (seconds) */
    double auto_continue_freshness_secs;
    /* P102: Session reset policy */
    char   reset_policy_mode[16];   /* "idle", "daily", "both", "none" */
    int    reset_policy_at_hour;    /* 0-23, for daily/both mode */
    int    reset_policy_idle_min;   /* minutes of idle before reset */

    /* M13: Configurable max concurrent sessions cap */
    int max_concurrent_sessions;

    /* GW15: Session sources LRU cache — hive-backed (no landlocked array) */
    hive_t *source_cache;           /* of gw_source_cache_entry_t* (heap) */
    int source_cache_count;         /* number of occupied entries */
    int source_cache_max;           /* max entries (default 512) */
    pthread_mutex_t source_cache_mutex;

    /* GW13: Kanban notifier profile integration */
    char   kanban_notifier_profile[128];
    bool   kanban_notifier_enabled;
    int    kanban_notifier_interval_sec; /* poll interval (default 5) */
    int    kanban_notifier_max_fail;     /* max consecutive send failures (default 3) */
    /* session.py extensions */
    char   routing_scope[64];           /* routing scope for session recovery */
    char   active_profile[128];         /* active profile name */
} gateway_state_t;

/* Global gateway state — defined in server.c */
extern gateway_state_t g_gw;

/* ================================================================
 *  Webhook subscription types
 * ================================================================ */

#define WEBHOOK_SUBS_MAX 32
#define WEBHOOK_HEADERS_MAX 16

/* Custom header entry */
typedef struct {
    char key[128];
    char value[512];
} webhook_header_t;

/* Webhook subscription entry */
typedef struct {
    char             endpoint[512];        /* Outbound URL */
    char             hmac_secret[128];     /* HMAC secret for signature verification */
    int              max_retries;          /* Max retry attempts (0 = no retry) */
    int              backoff_ms;           /* Initial backoff in ms (doubles each retry) */
    webhook_header_t headers[WEBHOOK_HEADERS_MAX]; /* Custom headers for outgoing calls */
    int              header_count;         /* Number of custom headers */
    gw_rate_limiter_t rate_limiter;        /* Per-subscription rate limiter */
    bool             in_use;              /* Slot occupied */
} webhook_subscription_t;

/* ================================================================
 *  Slash Access Policy
 * ================================================================ */

/* Opaque policy handle for slash command gating. */
typedef struct {
    bool   enabled;
    char   admin_user_ids[4096];
    char   user_allowed_commands[4096];
} slash_policy_t;

/** @} */
#endif /* HERMES_GATEWAY_TYPES_H */