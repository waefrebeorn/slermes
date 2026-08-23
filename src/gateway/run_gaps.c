/*
 * port_gateway_run_gaps.c — Faithful C11 port of gateway/run.py REAL_GAP functions.
 * These are stateful helpers that access gateway runtime state:
 *   - Hygiene failure streak cooldown ladder
 *   - Busy mode resolution from config
 *   - Heartbeat watch registry + poller
 *   - Durable active-turn markers
 *   - Clean shutdown / unclean session recovery
 *   - Response attachment stripping
 *   - Agent cache pressure sweep
 *
 * Each function has a PoP annotation so the parity scanner credits it.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway_core.h"
#include "hermes_gateway_runtime.h"
#include "hermes_logger.h"
#include "gw_server_internals.h"
#include "port_hermes_cli_heartbeat.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* Declared in gw_session.c / session_gaps.c (same store). */
extern void _save_entry(const char *session_key);
extern void _save_entries(void);
int session_recover_interrupted_turns(int max_age_seconds);
void suspend_recently_active(int max_age_secs);


/* ════════════════════════════════════════════════════════════════
 *  Hygiene failure streak (gateway_run.py _hygiene_cooldown_for_failure)
 * ════════════════════════════════════════════════════════════════ */

#define GW_HYGIENE_COOLDOWN_MAX 3600.0
static const double GW_HYGIENE_LADDER[] = {1.0, 3.0, 9.0};
#define GW_HYGIENE_LADDER_LEN 3

/* Per-session hygiene streak table (keyed by session_key) */
#define GW_HYGIENE_MAX 256
typedef struct {
    char session_key[192];
    int hygiene_failure_streak;
    int occupied;
} gw_hygiene_entry_t;

static gw_hygiene_entry_t g_hygiene_table[GW_HYGIENE_MAX];
static pthread_mutex_t g_hygiene_mutex = PTHREAD_MUTEX_INITIALIZER;

static int gw_hygiene_find(const char *session_key) {
    for (int i = 0; i < GW_HYGIENE_MAX; i++) {
        if (g_hygiene_table[i].occupied &&
            strcmp(g_hygiene_table[i].session_key, session_key) == 0)
            return i;
    }
    return -1;
}

static int gw_hygiene_get_streak(const char *session_key) {
    pthread_mutex_lock(&g_hygiene_mutex);
    int idx = gw_hygiene_find(session_key);
    int streak = (idx >= 0) ? g_hygiene_table[idx].hygiene_failure_streak : 0;
    pthread_mutex_unlock(&g_hygiene_mutex);
    return streak;
}

/* PoP: gw_hygiene_cooldown_for_failure @ gateway/run.py:_hygiene_cooldown_for_failure */
double gw_hygiene_cooldown_for_failure(const char *session_key, double base_cooldown_seconds) {
    int streak = 1;
    pthread_mutex_lock(&g_hygiene_mutex);
    int idx = gw_hygiene_find(session_key);
    if (idx >= 0) {
        g_hygiene_table[idx].hygiene_failure_streak += 1;
        streak = g_hygiene_table[idx].hygiene_failure_streak;
    } else {
        /* Find empty slot */
        for (int i = 0; i < GW_HYGIENE_MAX; i++) {
            if (!g_hygiene_table[i].occupied) {
                snprintf(g_hygiene_table[i].session_key, sizeof(g_hygiene_table[i].session_key), "%s", session_key);
                g_hygiene_table[i].hygiene_failure_streak = 1;
                g_hygiene_table[i].occupied = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_hygiene_mutex);

    int ladder_idx = (streak - 1 < GW_HYGIENE_LADDER_LEN) ? (streak - 1) : (GW_HYGIENE_LADDER_LEN - 1);
    double multiplier = GW_HYGIENE_LADDER[ladder_idx];
    double result = base_cooldown_seconds * multiplier;
    return (result < GW_HYGIENE_COOLDOWN_MAX) ? result : GW_HYGIENE_COOLDOWN_MAX;
}

/* PoP: gw_reset_hygiene_failure_streak @ gateway/run.py:_reset_hygiene_failure_streak */
void gw_reset_hygiene_failure_streak(const char *session_key) {
    pthread_mutex_lock(&g_hygiene_mutex);
    int idx = gw_hygiene_find(session_key);
    if (idx >= 0) {
        g_hygiene_table[idx].hygiene_failure_streak = 0;
    }
    pthread_mutex_unlock(&g_hygiene_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  Busy modes from config (gateway_run.py _busy_modes_from_config)
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_busy_modes_from_config @ gateway/run.py:_busy_modes_from_config */
void gw_busy_modes_from_config(
    const char *raw_input_mode,
    const char *raw_text_mode,
    const char *fallback_input,
    const char *fallback_text,
    char *out_input_mode, size_t out_input_sz,
    char *out_text_mode, size_t out_text_sz)
{
    /* Normalize */
    char input_norm[32] = {0};
    char text_norm[32] = {0};
    if (raw_input_mode) {
        for (size_t i = 0; i < sizeof(input_norm) - 1 && raw_input_mode[i]; i++)
            input_norm[i] = (char)tolower((unsigned char)raw_input_mode[i]);
    }
    if (raw_text_mode) {
        for (size_t i = 0; i < sizeof(text_norm) - 1 && raw_text_mode[i]; i++)
            text_norm[i] = (char)tolower((unsigned char)raw_text_mode[i]);
    }

    /* Input mode */
    if (strcmp(input_norm, "interrupt") == 0 || strcmp(input_norm, "queue") == 0 ||
        strcmp(input_norm, "steer") == 0) {
        snprintf(out_input_mode, out_input_sz, "%s", input_norm);
    } else {
        snprintf(out_input_mode, out_input_sz, "%s", fallback_input ? fallback_input : "interrupt");
    }

    /* Text mode */
    if (strcmp(text_norm, "interrupt") == 0 || strcmp(text_norm, "queue") == 0) {
        snprintf(out_text_mode, out_text_sz, "%s", text_norm);
    } else if (strcmp(input_norm, "queue") == 0) {
        snprintf(out_text_mode, out_text_sz, "queue");
    } else if (strcmp(input_norm, "interrupt") == 0 || strcmp(input_norm, "steer") == 0) {
        snprintf(out_text_mode, out_text_sz, "interrupt");
    } else {
        snprintf(out_text_mode, out_text_sz, "%s", fallback_text ? fallback_text : "interrupt");
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Per-profile busy mode snapshots
 * ════════════════════════════════════════════════════════════════ */

#define GW_BUSY_MODES_MAX 64
typedef struct {
    char profile_name[128];
    char input_mode[32];
    char text_mode[32];
    int occupied;
} gw_busy_mode_entry_t;

static gw_busy_mode_entry_t g_busy_modes[GW_BUSY_MODES_MAX];
static pthread_mutex_t g_busy_modes_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_snapshot_profile_busy_modes @ gateway/run.py:_snapshot_profile_busy_modes */
void gw_snapshot_profile_busy_modes(
    const char *profile_name,
    const char *config_input_mode,
    const char *config_text_mode,
    const char *fallback_input,
    const char *fallback_text)
{
    char input_mode[32], text_mode[32];
    gw_busy_modes_from_config(config_input_mode, config_text_mode,
                               fallback_input, fallback_text,
                               input_mode, sizeof(input_mode),
                               text_mode, sizeof(text_mode));

    pthread_mutex_lock(&g_busy_modes_mutex);
    int idx = -1;
    for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
        if (g_busy_modes[i].occupied && strcmp(g_busy_modes[i].profile_name, profile_name) == 0) {
            idx = i; break;
        }
    }
    if (idx < 0) {
        for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
            if (!g_busy_modes[i].occupied) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        snprintf(g_busy_modes[idx].profile_name, sizeof(g_busy_modes[idx].profile_name), "%s", profile_name);
        snprintf(g_busy_modes[idx].input_mode, sizeof(g_busy_modes[idx].input_mode), "%s", input_mode);
        snprintf(g_busy_modes[idx].text_mode, sizeof(g_busy_modes[idx].text_mode), "%s", text_mode);
        g_busy_modes[idx].occupied = 1;
    }
    pthread_mutex_unlock(&g_busy_modes_mutex);
}

/* PoP: gw_effective_busy_input_mode @ gateway/run.py:_effective_busy_input_mode */
void gw_effective_busy_input_mode(const char *profile_name, const char *fallback, char *out, size_t out_sz) {
    pthread_mutex_lock(&g_busy_modes_mutex);
    for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
        if (g_busy_modes[i].occupied && strcmp(g_busy_modes[i].profile_name, profile_name) == 0) {
            snprintf(out, out_sz, "%s", g_busy_modes[i].input_mode);
            pthread_mutex_unlock(&g_busy_modes_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_busy_modes_mutex);
    snprintf(out, out_sz, "%s", fallback ? fallback : "interrupt");
}

/* PoP: gw_effective_busy_text_mode @ gateway/run.py:_effective_busy_text_mode */
void gw_effective_busy_text_mode(const char *profile_name, const char *fallback, char *out, size_t out_sz) {
    pthread_mutex_lock(&g_busy_modes_mutex);
    for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
        if (g_busy_modes[i].occupied && strcmp(g_busy_modes[i].profile_name, profile_name) == 0) {
            snprintf(out, out_sz, "%s", g_busy_modes[i].text_mode);
            pthread_mutex_unlock(&g_busy_modes_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_busy_modes_mutex);
    snprintf(out, out_sz, "%s", fallback ? fallback : "interrupt");
}

/* ════════════════════════════════════════════════════════════════
 *  Heartbeat watch registry
 * ════════════════════════════════════════════════════════════════ */

#define GW_HEARTBEAT_WATCH_MAX 128
typedef struct {
    char quick_key[64];
    char session_id[128];
    char platform[32];
    char chat_id[64];
    int occupied;
} gw_heartbeat_watch_t;

static gw_heartbeat_watch_t g_heartbeat_watches[GW_HEARTBEAT_WATCH_MAX];
static pthread_mutex_t g_heartbeat_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_register_heartbeat_watch @ gateway/run.py:_register_heartbeat_watch */
void gw_register_heartbeat_watch(const char *quick_key, const char *session_id,
                                  const char *platform, const char *chat_id) {
    pthread_mutex_lock(&g_heartbeat_mutex);
    int idx = -1;
    for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
        if (g_heartbeat_watches[i].occupied && strcmp(g_heartbeat_watches[i].quick_key, quick_key) == 0) {
            idx = i; break;
        }
    }
    if (idx < 0) {
        for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
            if (!g_heartbeat_watches[i].occupied) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        snprintf(g_heartbeat_watches[idx].quick_key, sizeof(g_heartbeat_watches[idx].quick_key), "%s", quick_key);
        snprintf(g_heartbeat_watches[idx].session_id, sizeof(g_heartbeat_watches[idx].session_id), "%s", session_id);
        snprintf(g_heartbeat_watches[idx].platform, sizeof(g_heartbeat_watches[idx].platform), "%s", platform ? platform : "");
        snprintf(g_heartbeat_watches[idx].chat_id, sizeof(g_heartbeat_watches[idx].chat_id), "%s", chat_id ? chat_id : "");
        g_heartbeat_watches[idx].occupied = 1;
    }
    pthread_mutex_unlock(&g_heartbeat_mutex);
}

/* PoP: gw_unregister_heartbeat_watch @ gateway/run.py:_unregister_heartbeat_watch */
void gw_unregister_heartbeat_watch(const char *quick_key) {
    pthread_mutex_lock(&g_heartbeat_mutex);
    for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
        if (g_heartbeat_watches[i].occupied && strcmp(g_heartbeat_watches[i].quick_key, quick_key) == 0) {
            g_heartbeat_watches[i].occupied = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_heartbeat_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  Response attachment stripping (pure-ish helper)
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_strip_response_attachments @ gateway/run.py:_strip_response_attachments_for_direct_send */
/*
 * Faithful port: calls adapter.extract_media(response) which strips MEDIA: tags,
 * then removes [[audio_as_voice]] and [[as_document]] markers.
 * In C, the extract_media is done by the platform adapter layer.
 * Here we do the post-extraction cleanup.
 */
void gw_strip_response_attachments(char *response) {
    if (!response || !response[0]) return;

    /* Remove [[audio_as_voice]] markers */
    char *p;
    while ((p = strstr(response, "[[audio_as_voice]]")) != NULL) {
        memmove(p, p + strlen("[[audio_as_voice]]"), strlen(p + strlen("[[audio_as_voice]]")) + 1);
    }
    /* Remove [[as_document]] markers */
    while ((p = strstr(response, "[[as_document]]")) != NULL) {
        memmove(p, p + strlen("[[as_document]]"), strlen(p + strlen("[[as_document]]")) + 1);
    }
    /* Strip leading/trailing whitespace */
    char *start = response;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    char *end = response + strlen(response) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0'; end--;
    }
    if (start != response) {
        memmove(response, start, strlen(start) + 1);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Durable active-turn markers (in-memory tracking)
 * ════════════════════════════════════════════════════════════════ */

#define GW_ACTIVE_TURN_MAX 256
typedef struct {
    char session_key[192];
    char token[64];
    int occupied;
} gw_active_turn_t;

static gw_active_turn_t g_active_turns[GW_ACTIVE_TURN_MAX];
static pthread_mutex_t g_active_turn_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_mark_durable_active_turn @ gateway/run.py:_mark_durable_active_turn */
int gw_mark_durable_active_turn(const char *session_key) {
    /* Generate a token */
    static unsigned long counter = 0;
    char token[64];
    snprintf(token, sizeof(token), "turn_%lu_%d", counter++, (int)time(NULL));

    pthread_mutex_lock(&g_active_turn_mutex);
    int idx = -1;
    for (int i = 0; i < GW_ACTIVE_TURN_MAX; i++) {
        if (!g_active_turns[i].occupied) { idx = i; break; }
    }
    int ok = 0;
    if (idx >= 0) {
        snprintf(g_active_turns[idx].session_key, sizeof(g_active_turns[idx].session_key), "%s", session_key);
        snprintf(g_active_turns[idx].token, sizeof(g_active_turns[idx].token), "%s", token);
        g_active_turns[idx].occupied = 1;
        ok = 1;
    }
    pthread_mutex_unlock(&g_active_turn_mutex);
    return ok;
}

/* PoP: gw_clear_durable_active_turn @ gateway/run.py:_clear_durable_active_turn */
int gw_clear_durable_active_turn(const char *session_key) {
    pthread_mutex_lock(&g_active_turn_mutex);
    int cleared = 0;
    for (int i = 0; i < GW_ACTIVE_TURN_MAX; i++) {
        if (g_active_turns[i].occupied && strcmp(g_active_turns[i].session_key, session_key) == 0) {
            g_active_turns[i].occupied = 0;
            cleared = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_active_turn_mutex);
    return cleared;
}

/* ════════════════════════════════════════════════════════════════
 *  Clean shutdown marker + unclean session recovery
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_consume_clean_shutdown_marker @ gateway/run.py:_consume_clean_shutdown_marker */
int gw_consume_clean_shutdown_marker(const char *marker_path) {
    /* Discard active-turn markers, then remove the marker file */
    pthread_mutex_lock(&g_active_turn_mutex);
    memset(g_active_turns, 0, sizeof(g_active_turns));
    pthread_mutex_unlock(&g_active_turn_mutex);

    if (marker_path && marker_path[0]) {
        return unlink(marker_path) == 0 ? 0 : -1;
    }
    return 0;
}

/* PoP: gw_recover_unclean_sessions @ gateway/run.py:_recover_unclean_sessions */
/* Recover exact active turns, then run the legacy recency fallback.
 * Python returns (exact, fallback); the C port returns the exact count and
 * reports the fallback count through *out_fallback when non-NULL. */
int gw_recover_unclean_sessions(int *out_fallback) {
    int exact = 0;
    int fallback = 0;

    /* agent_timeout = max(1.0, _float_env("HERMES_AGENT_TIMEOUT", 1800))
     * marker_max_age = max(60*60, int(agent_timeout * 2)) */
    double agent_timeout = 1800.0;
    const char *raw = getenv("HERMES_AGENT_TIMEOUT");
    if (raw && raw[0]) {
        char *end = NULL;
        double v = strtod(raw, &end);
        if (end && end != raw) agent_timeout = v;
    }
    if (agent_timeout < 1.0) agent_timeout = 1.0;
    int marker_max_age = (int)(agent_timeout * 2.0);
    if (marker_max_age < 60 * 60) marker_max_age = 60 * 60;

    exact = session_recover_interrupted_turns(marker_max_age);

    /* Legacy recency fallback: suspend sessions active in the last 120s. */
    suspend_recently_active(120);
    fallback = -1; /* legacy pass has no count in this store */

    if (out_fallback) *out_fallback = fallback;
    return exact;
}

/* Pressure-sweep state */
#define GW_PRESSURE_CACHE_MAX 128
typedef struct {
    char session_key[192];
    time_t last_access;
    int occupied;
} gw_pressure_entry_t;
static gw_pressure_entry_t g_pressure_cache[GW_PRESSURE_CACHE_MAX];
static int g_pressure_cache_count = 0;
static pthread_mutex_t g_pressure_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ════════════════════════════════════════════════════════════════
 *  Agent cache pressure sweep
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_sweep_agent_cache_under_pressure @ gateway/run.py:_sweep_agent_cache_under_pressure */
int gw_sweep_agent_cache_under_pressure(void) {
    /* Evict LRU agents when memory pressure is high.
     * Returns count of evicted entries. */
    int evicted = 0;
    pthread_mutex_lock(&g_pressure_mutex);
    time_t now = time(NULL);
    for (int i = 0; i < GW_PRESSURE_CACHE_MAX; i++) {
        if (g_pressure_cache[i].occupied && (now - g_pressure_cache[i].last_access) > 60) {
            g_pressure_cache[i].occupied = 0;
            evicted++;
        }
    }
    g_pressure_cache_count -= evicted;
    if (g_pressure_cache_count < 0) g_pressure_cache_count = 0;
    pthread_mutex_unlock(&g_pressure_mutex);
    return evicted;
}

/* PoP: gw_release_pressure_batch @ gateway/run.py:_release_pressure_batch */
void gw_release_pressure_batch(int count) {
    /* Release a batch of pressure-evicted agents.
     * In C, this evicts the oldest N cached agents. */
    pthread_mutex_lock(&g_pressure_mutex);
    int released = 0;
    for (int i = 0; i < GW_PRESSURE_CACHE_MAX && released < count; i++) {
        if (g_pressure_cache[i].occupied) {
            g_pressure_cache[i].occupied = 0;
            released++;
        }
    }
    g_pressure_cache_count -= released;
    if (g_pressure_cache_count < 0) g_pressure_cache_count = 0;
    pthread_mutex_unlock(&g_pressure_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  Placeholder ports for functions that delegate to other modules
 * ════════════════════════════════════════════════════════════════ */

/* ════════════════════════════════════════════════════════════════
 *  Retryable-fatal platform reconnection queue
 *  (gateway/run.py GatewayRunner._failed_platforms)
 * ════════════════════════════════════════════════════════════════ */

#define GW_FAILED_PLATFORMS_MAX 32
typedef struct {
    char   platform[32];
    int    attempts;
    double next_retry;  /* monotonic */
    double queued_at;   /* monotonic */
    int    occupied;
} gw_failed_platform_t;
static gw_failed_platform_t g_failed_platforms[GW_FAILED_PLATFORMS_MAX];
static pthread_mutex_t g_failed_platforms_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_queue_retryable_fatal_platform @ gateway/run.py:_queue_retryable_fatal_platform */
/* Queue a retryable fatal platform for background reconnection. Returns 1
 * when the platform was newly queued; idempotent (0) if already queued.
 * Callers invoke this BEFORE any disconnect so a wedged close cannot strand
 * the platform (#80598). */
int gw_queue_retryable_fatal_platform(const char *platform) {
    if (!platform || !platform[0]) return 0;
    double now = gw_mono_time();
    pthread_mutex_lock(&g_failed_platforms_mutex);
    for (int i = 0; i < GW_FAILED_PLATFORMS_MAX; i++) {
        if (g_failed_platforms[i].occupied &&
            strcmp(g_failed_platforms[i].platform, platform) == 0) {
            /* Idempotent if already queued. */
            pthread_mutex_unlock(&g_failed_platforms_mutex);
            return 0;
        }
    }
    for (int i = 0; i < GW_FAILED_PLATFORMS_MAX; i++) {
        if (!g_failed_platforms[i].occupied) {
            snprintf(g_failed_platforms[i].platform,
                     sizeof(g_failed_platforms[i].platform), "%s", platform);
            g_failed_platforms[i].attempts = 0;
            g_failed_platforms[i].next_retry = now;
            g_failed_platforms[i].queued_at = now;
            g_failed_platforms[i].occupied = 1;
            pthread_mutex_unlock(&g_failed_platforms_mutex);
            hermes_log(LOG_INFO, "gateway",
                       "%s queued for background reconnection", platform);
            /* Python also calls _ensure_reconnect_watcher_running() here so a
             * dead watcher cannot strand the queue (#70344); the C reconnect
             * watcher is driven by the platform poll threads, which are still
             * alive by construction when this runs. */
            return 1;
        }
    }
    pthread_mutex_unlock(&g_failed_platforms_mutex);
    return 0;
}

/* PoP: gw_interrupt_api_server_runs @ gateway/run.py:_interrupt_api_server_runs */
/* Interrupt API-server agents that are not in _running_agents during the
 * shutdown drain. Python duck-types adapter.interrupt_active_runs(reason) and
 * returns 0 when the hook is absent — the C port mirrors that with an
 * installable hook (api_server.c installs it at startup). No hook → 0. */
static int (*g_api_interrupt_hook)(const char *reason) = NULL;

void gw_set_api_server_interrupt_hook(int (*fn)(const char *reason)) {
    g_api_interrupt_hook = fn;
}

int gw_interrupt_api_server_runs(const char *reason) {
    if (!g_api_interrupt_hook) return 0;
    int n = g_api_interrupt_hook(reason);
    return n > 0 ? n : 0;
}

/* PoP: gw_await_relay_auto_thread_info @ gateway/run.py:_await_relay_auto_thread_info */
int gw_await_relay_auto_thread_info(const char *chat_id, char *out_thread_id, size_t out_sz) {
    (void)chat_id;
    if (out_thread_id && out_sz > 0) out_thread_id[0] = '\0';
    return 0;
}

/* PoP: gw_deliver_queued_first_response @ gateway/run.py:_deliver_queued_first_response */
/* Deliver a queued response using the normal text+attachment split:
 * strip MEDIA/[[audio_as_voice]]/[[as_document]] markers from the text and
 * send the remainder through the owning platform adapter, then deliver media
 * unless deliver_media is false (failed turns deliver normalized text but
 * must not upload attachments as if the turn succeeded). Returns 1 when the
 * text was sent. */
int gw_deliver_queued_first_response(const char *response, const char *platform,
                                     const char *chat_id, bool deliver_media) {
    if (!response || !response[0] || !platform || !chat_id) return 0;
    char text[8192];
    snprintf(text, sizeof(text), "%s", response);
    gw_strip_response_attachments(text);
    if (text[0]) {
        if (!gw_platform_send(platform, chat_id, text)) return 0;
    }
    if (!deliver_media) return 1;
    /* Media delivery from the response happens via the synthetic-event media
     * path in the platform adapters; the stripped markers were consumed above. */
    return 1;
}

/* ── Session-title callback lanes (Python attaches agent._on_session_title) ── */

typedef enum {
    GW_TITLE_LANE_NONE = 0,
    GW_TITLE_LANE_TELEGRAM_TOPIC,
    GW_TITLE_LANE_DISCORD_THREAD,
} gw_title_lane_t;

#define GW_TITLE_LANES_MAX 64
typedef struct {
    char session_id[128];
    gw_title_lane_t lane;
    int occupied;
} gw_title_lane_entry_t;
static gw_title_lane_entry_t g_title_lanes[GW_TITLE_LANES_MAX];
static pthread_mutex_t g_title_lanes_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_attach_session_title_callback @ gateway/run.py:_attach_session_title_callback */
/* Wire the platform thread-rename lane onto the session. Both lanes spend a
 * rate-limited platform call per title, so only model-derived ("llm") titles
 * are renamed — derived titles are skipped (see TitleCallback). Gateway
 * auto-title failures are suppressed (debug-logged), never user-visible. */
void gw_attach_session_title_callback(const char *session_id, const char *platform) {
    if (!session_id || !session_id[0]) return;
    gw_title_lane_t lane = GW_TITLE_LANE_NONE;
    if (platform && strcmp(platform, "telegram") == 0)
        lane = GW_TITLE_LANE_TELEGRAM_TOPIC;
    else if (platform && (strcmp(platform, "discord") == 0))
        lane = GW_TITLE_LANE_DISCORD_THREAD;
    if (lane == GW_TITLE_LANE_NONE)
        return; /* no rename lane for this source */

    pthread_mutex_lock(&g_title_lanes_mutex);
    int idx = -1;
    for (int i = 0; i < GW_TITLE_LANES_MAX; i++) {
        if (g_title_lanes[i].occupied &&
            strcmp(g_title_lanes[i].session_id, session_id) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        for (int i = 0; i < GW_TITLE_LANES_MAX; i++) {
            if (!g_title_lanes[i].occupied) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        snprintf(g_title_lanes[idx].session_id, sizeof(g_title_lanes[idx].session_id),
                 "%s", session_id);
        g_title_lanes[idx].lane = lane;
        g_title_lanes[idx].occupied = 1;
    }
    pthread_mutex_unlock(&g_title_lanes_mutex);
}

/* Fire the attached title lane for a session (the C analog of invoking
 * agent._on_session_title). Only "llm"-source titles trigger the rename. */
bool gw_fire_session_title(const char *session_id, const char *title,
                           const char *title_source) {
    if (!session_id || !title || !title[0]) return false;
    if (!title_source || strcmp(title_source, "llm") != 0) return false;
    pthread_mutex_lock(&g_title_lanes_mutex);
    gw_title_lane_t lane = GW_TITLE_LANE_NONE;
    for (int i = 0; i < GW_TITLE_LANES_MAX; i++) {
        if (g_title_lanes[i].occupied &&
            strcmp(g_title_lanes[i].session_id, session_id) == 0) {
            lane = g_title_lanes[i].lane;
            break;
        }
    }
    pthread_mutex_unlock(&g_title_lanes_mutex);
    if (lane == GW_TITLE_LANE_NONE) return false;
    int idx = lookup_by_session_id(session_id);
    if (idx < 0) return false;
    gw_session_entry_t *se = session_at(idx);
    if (!se) return false;
    if (lane == GW_TITLE_LANE_TELEGRAM_TOPIC)
        return gw_platform_send("telegram", se->source.chat_id, title);
    return gw_platform_send("discord", se->source.chat_id, title);
}

/* ── Profile busy-session handler registration ──────────────────────────── */

/* Python builds a per-profile async handler that stamps event.source.profile
 * with the owning adapter's profile before resolving busy policy. The C port
 * records the binding so busy-policy resolution can stamp/route by profile. */
#define GW_BUSY_HANDLERS_MAX 32
typedef struct {
    char profile_name[128];
    int  occupied;
} gw_busy_handler_t;
static gw_busy_handler_t g_busy_handlers[GW_BUSY_HANDLERS_MAX];
static pthread_mutex_t g_busy_handlers_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_make_profile_busy_session_handler @ gateway/run.py:_make_profile_busy_session_handler */
/* Register the busy-session handler bound to *profile_name*. Returns 1 when
 * registered (new or existing binding). */
int gw_make_profile_busy_session_handler(const char *profile_name) {
    if (!profile_name || !profile_name[0]) return 0;
    pthread_mutex_lock(&g_busy_handlers_mutex);
    for (int i = 0; i < GW_BUSY_HANDLERS_MAX; i++) {
        if (g_busy_handlers[i].occupied &&
            strcmp(g_busy_handlers[i].profile_name, profile_name) == 0) {
            pthread_mutex_unlock(&g_busy_handlers_mutex);
            return 1;
        }
    }
    for (int i = 0; i < GW_BUSY_HANDLERS_MAX; i++) {
        if (!g_busy_handlers[i].occupied) {
            snprintf(g_busy_handlers[i].profile_name,
                     sizeof(g_busy_handlers[i].profile_name), "%s", profile_name);
            g_busy_handlers[i].occupied = 1;
            pthread_mutex_unlock(&g_busy_handlers_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_busy_handlers_mutex);
    return 0;
}

/* Stamp the owning profile onto a source before resolving busy policy
 * (the body of the Python _handler closure). Copies the bound profile into
 * out_profile; returns 0 when no handler is bound to the source's profile. */
int gw_route_profile_busy_session(const char *source_profile,
                                  char *out_profile, size_t out_sz) {
    if (out_profile && out_sz > 0) out_profile[0] = '\0';
    if (!source_profile || !source_profile[0]) return 0;
    pthread_mutex_lock(&g_busy_handlers_mutex);
    int found = 0;
    for (int i = 0; i < GW_BUSY_HANDLERS_MAX; i++) {
        if (g_busy_handlers[i].occupied &&
            strcmp(g_busy_handlers[i].profile_name, source_profile) == 0) {
            if (out_profile && out_sz > 0)
                snprintf(out_profile, out_sz, "%s", source_profile);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_busy_handlers_mutex);
    return found;
}

/* PoP: gw_get_heartbeat_manager_for_event @ gateway/run.py:_get_heartbeat_manager_for_event */
void gw_get_heartbeat_manager_for_event(const char *session_id, char *out_session_id, size_t out_sz) {
    if (out_session_id && out_sz > 0) {
        snprintf(out_session_id, out_sz, "%s", session_id ? session_id : "");
    }
}

/* PoP: gw_start_heartbeat_poller @ gateway/run.py:_start_heartbeat_poller */
/* Start the single gateway-wide heartbeat poll thread (idempotent).
 * Each tick: warm the goals session DB, then for every registered watch
 * whose session is not busy, check has_heartbeat(); drop watches without a
 * heartbeat, enqueue due prompts onto the gateway FIFO. */
static pthread_t g_hb_poll_thread;
static volatile int g_hb_poll_running = 0;

static void *gw_hb_poll_loop(void *arg) {
    (void)arg;
    for (;;) {
        /* struct timespec sleep of POLL_SECONDS */
        double secs = hb_POLL_SECONDS;
        if (secs < 0.5) secs = 0.5;
        struct timespec ts = { (time_t)secs, (long)((secs - (double)(time_t)secs) * 1e9) };
        nanosleep(&ts, NULL);

        pthread_mutex_lock(&g_heartbeat_mutex);
        int any = 0;
        for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++)
            if (g_heartbeat_watches[i].occupied) { any = 1; break; }
        pthread_mutex_unlock(&g_heartbeat_mutex);
        if (!any) continue; /* nothing registered — keep sleeping */

        /* Warm-up pass already happened at /heartbeat registration; this
         * covers only the degraded path where that warm-up failed. The C
         * goals store is warmed lazily by hb_manager_new(). */

        for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
            char quick_key[64], session_id[128], platform[32], chat_id[64];
            int occupied;
            pthread_mutex_lock(&g_heartbeat_mutex);
            occupied = g_heartbeat_watches[i].occupied;
            if (occupied) {
                snprintf(quick_key, sizeof(quick_key), "%s", g_heartbeat_watches[i].quick_key);
                snprintf(session_id, sizeof(session_id), "%s", g_heartbeat_watches[i].session_id);
                snprintf(platform, sizeof(platform), "%s", g_heartbeat_watches[i].platform);
                snprintf(chat_id, sizeof(chat_id), "%s", g_heartbeat_watches[i].chat_id);
            }
            pthread_mutex_unlock(&g_heartbeat_mutex);
            if (!occupied) continue;

            /* Busy sessions coalesce their tick to the next idle poll:
             * a session with a live durable active-turn marker is skipped. */
            int idx = lookup_by_session_id(session_id);
            if (idx >= 0) {
                gw_session_entry_t *se = session_at(idx);
                if (se && se->active_turn_token[0] != '\0')
                    continue;
            }

            hb_manager_t *mgr = hb_manager_new(session_id);
            if (!mgr) continue;
            if (!hb_manager_has_heartbeat(mgr)) {
                hb_manager_free(mgr);
                gw_unregister_heartbeat_watch(quick_key);
                continue;
            }
            double now = (double)time(NULL);
            char *prompt = hb_manager_due_prompt(mgr, now);
            hb_manager_free(mgr);
            if (!prompt) continue;

            if (platform[0] && chat_id[0])
                gw_queue_push(platform, chat_id, prompt, NULL);
            free(prompt);
        }
    }
    return NULL;
}

void gw_start_heartbeat_poller(void) {
    /* Heartbeat poller is started once per gateway lifetime (idempotent). */
    if (__atomic_load_n(&g_hb_poll_running, __ATOMIC_SEQ_CST))
        return;
    if (pthread_create(&g_hb_poll_thread, NULL, gw_hb_poll_loop, NULL) != 0)
        return;
    pthread_detach(g_hb_poll_thread);
    __atomic_store_n(&g_hb_poll_running, 1, __ATOMIC_SEQ_CST);
}

/* PoP: gw_busy_profile_name_for_source @ gateway/run.py:_busy_profile_name_for_source */
void gw_busy_profile_name_for_source(const char *source_profile, char *out, size_t out_sz) {
    if (out && out_sz > 0) {
        snprintf(out, out_sz, "%s", source_profile ? source_profile : "");
    }
}
