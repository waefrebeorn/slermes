/*
 * server.c — Multi-platform gateway server for Hermes C.
 * Supports Telegram, Discord, Slack, Matrix, Mattermost, Webhook, WhatsApp.
 * Platforms run concurrently via pthread. Each gets its own HTTP client.
 * Configured via --platform flag (single) or config.yaml gateway.platforms list.
 */

#include "hermes_core_types.h"
#include "gw_server_internals.h"
#include "gw_pollers.h"
#include "hermes_agent.h"
#include "hermes_gateway_core.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "gateway_helpers.h"
#include "hermes_skill_commands.h"
#include "hermes_logger.h"
#include "hermes_telegram_filter.h"
#include "hermes_gateway_runner.h"
#include "port_gateway_run_agent.h"
#include "hermes_gateway.h"
#include "approval.h"
#include "hermes_cdp.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hermes_gateway_config.h"
#include "port_gateway_run_deps.h"
#include "hermes_redact.h"
#include "hermes_tirith.h"
#include "hermes_cron.h"

/* ============================================================================
 *  Hook pipeline appliers (defined here; declared in gw_server_internals.h).
 *  Each hook is gw_hook_t = json_node_t *(*)(json_node_t *, void *userdata)
 *  and operates on a {"platform","chat_id","text"} JSON envelope, mutating
 *  the "text" field. The applier runs every registered hook in order, then
 *  returns a freshly-allocated modified string, or NULL if text is unchanged.
 *  Mirrors the Python gateway hook pipeline.
 * ========================================================================== */

/* Run every registered hook in `arr` (count `n`) over a text envelope.
 * Returns a newly-allocated string (modified text), or NULL if unchanged. */
static char *apply_hook_array(gw_hook_t *arr, void **data, int n,
                              const char *platform, const char *chat_id,
                              const char *text) {
    if (n <= 0 || !text) return NULL;
    json_node_t *env = json_new_object();
    if (platform) json_object_set(env, "platform", json_new_string(platform));
    if (chat_id)  json_object_set(env, "chat_id",  json_new_string(chat_id));
    json_object_set(env, "text", json_new_string(text));

    for (int i = 0; i < n; i++) {
        json_node_t *out = arr[i](env, data ? data[i] : NULL);
        if (out && out != env) {
            json_free(env);
            env = out;
        }
    }

    const char *new_text = json_object_get_string(env, "text", text);
    char *result = NULL;
    if (strcmp(new_text, text) != 0) {
        result = strdup(new_text);
    }
    json_free(env);
    return result;
}

char *gw_apply_pre_send_hooks(const char *platform, const char *text) {
    return apply_hook_array(gw_hooks.pre_send, gw_hooks.pre_send_data,
                            gw_hooks.pre_send_count, platform, NULL, text);
}

char *gw_apply_post_receive_hooks(const char *platform, const char *chat_id,
                                  const char *text) {
    return apply_hook_array(gw_hooks.post_receive, gw_hooks.post_receive_data,
                            gw_hooks.post_receive_count, platform, chat_id, text);
}

char *gw_apply_interceptors(const char *platform, const char *chat_id,
                            const char *text) {
    return apply_hook_array(gw_hooks.interceptor, gw_hooks.interceptor_data,
                            gw_hooks.interceptor_count, platform, chat_id, text);
}

/* ================================================================
 *  Gateway state
 * ================================================================ */

gateway_state_t g_gw;

/* Shared GatewayRunner: owns the faithful session-state maps (model/reasoning
 * overrides, run-generation tokens, sidecar notes, native images) that the
 * turn core (gateway_runner_run_agent_inner) reads. Created in
 * hermes_gateway_main, used by process_update. */
static GatewayRunner *g_runner = NULL;

/* Promoted gateway globals (declared extern in gw_server_internals.h).
 * server.c owns the definitions; the extracted gateway modules reference
 * them. ZERO-initialized; per-field init happens in gateway_setup.
 * (g_gw_log_fp / g_gw_log_path are defined further below near the log code.) */
gw_clarify_state_t  g_gw_clarify;
gw_approval_state_t g_gw_approval;
gw_hooks_t          gw_hooks;
gw_event_bus_t      gw_event_bus;

/* Extract and clear the accumulated observe buffer for a platform (L08).
 * Returns a freshly-allocated string of the buffered observe lines that
 * mention `platform`, or NULL if the buffer is empty / unrelated. The
 * consumed portion is removed from g_gw.observe_buffer. */
char *gw_observe_consume(const char *platform, const char *chat_id) {
    (void)chat_id;
    if (!platform || !g_gw.observe_buffer[0]) return NULL;
    pthread_mutex_lock(&g_gw.observe_mutex);
    /* Only hand back context if the buffer references this platform. */
    char *hit = strstr(g_gw.observe_buffer, platform);
    char *out = NULL;
    if (hit) {
        size_t len = strlen(g_gw.observe_buffer);
        out = malloc(len + 1);
        if (out) {
            memcpy(out, g_gw.observe_buffer, len + 1);
            g_gw.observe_buffer[0] = '\0';
        }
    }
    pthread_mutex_unlock(&g_gw.observe_mutex);
    return out;
}

/* ================================================================
 *  P101: Monotonic time helper
 * ================================================================ */


/* Forward declaration for gw_queue_drain_all */

/* GW13: Kanban notifier thread — polls kanban events and delivers to subscribers */

/* ================================================================
 *  P101: Message queue (thread-safe, bounded circular buffer)
 * ================================================================ */


/* Drain all queued messages — called periodically from polling threads.
   Each message goes through process_update() which re-checks rate limits.
   If still rate-limited, the message gets re-pushed and picked up next cycle. */

/* ================================================================
 *  Gateway Clarify — async clarify prompt response collector
 * ================================================================ */

/* Pending clarify state — set when clarify prompt sent via gateway */
gw_clarify_state_t g_gw_clarify = {false, "", "", "", "", "", PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0, {{0}}, 0, false};

/* Register a platform poll function to use during clarify wait */

/* Begin waiting for clarify response — must be called before gw_clarify_wait_response.
   Sets the platform, chat_id, session_key context and marks pending. Thread-safe. */

/* Internal: check if a message matches pending clarify and capture response.
   Returns true if consumed. Caller must hold g_gw_clarify.mutex. */

/* Check if an incoming message is a clarify response.
   Called from platform message handler threads.
   Returns true if consumed. */

/* Called by clarify.c (via callback) to wait for user's response.
   Runs inside agent_chat() — blocks until resolved or timeout. */
static char *gw_clarify_wait_response(int timeout_sec) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_sec;

    pthread_mutex_lock(&g_gw_clarify.mutex);
    g_gw_clarify.pending = true;
    g_gw_clarify.response[0] = '\0';
    pthread_mutex_unlock(&g_gw_clarify.mutex);

    while (timeout_sec > 0) {
        pthread_mutex_lock(&g_gw_clarify.mutex);
        if (g_gw_clarify.response[0]) {
            char *resp = strdup(g_gw_clarify.response);
            g_gw_clarify.response[0] = '\0';
            g_gw_clarify.pending = false;
            pthread_mutex_unlock(&g_gw_clarify.mutex);
            return resp;
        }

        /* If we have a poll function, do a short poll for new updates */
        if (g_gw_clarify.poll_fn) {
            pthread_mutex_unlock(&g_gw_clarify.mutex);
            char *text = g_gw_clarify.poll_fn(g_gw_clarify.chat_id);
            if (text) {
                pthread_mutex_lock(&g_gw_clarify.mutex);
                gw_clarify_match(g_gw_clarify.platform, g_gw_clarify.chat_id, text);
                pthread_mutex_unlock(&g_gw_clarify.mutex);
                free(text);
                continue;
            }
        } else {
            /* No poll function — wait on condvar with 1s timeout */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&g_gw_clarify.cond, &g_gw_clarify.mutex, &ts);
            pthread_mutex_unlock(&g_gw_clarify.mutex);
        }

        timeout_sec--;

        /* Sleep 1s between polls to avoid busy-waiting */
        if (g_gw_clarify.poll_fn) sleep(1);
    }

    /* Timeout — clean up */
    pthread_mutex_lock(&g_gw_clarify.mutex);
    g_gw_clarify.pending = false;
    g_gw_clarify.response[0] = '\0';
    pthread_mutex_unlock(&g_gw_clarify.mutex);
    return NULL;
}

/* ================================================================
 *  Gateway Approval — async approval prompt response collector
 * ================================================================ */

/* Pending approval state — set when approval prompt sent via gateway */
gw_approval_state_t g_gw_approval = {false, "", "", "", PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0};

/* Register a platform poll function to use during approval wait */

/* Set context for pending approval — called from approval.c */

/* Begin waiting for approval response — must be called before gw_approval_wait_response.
   Sets the platform, chat_id context and marks pending. Thread-safe. */

/* Internal: check if a message matches pending approval and capture response.
   Returns true if consumed. Caller must hold g_gw_approval.mutex. */

/* Called by approval.c (via callback) to wait for user's y/n/a response.
   Runs inside agent_chat() — the poll thread that sent the prompt.
   Uses short-polling with the platform's poll function to capture response. */
static char *gw_approval_wait_response(int timeout_sec) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_sec;

    /* Get the platform/chat_id context that was set by approval_set_gateway_send.
       The approval prompt has already been sent. We just mark ourselves pending
       for response collection. */
    pthread_mutex_lock(&g_gw_approval.mutex);
    g_gw_approval.pending = true;
    g_gw_approval.response[0] = '\0';
    pthread_mutex_unlock(&g_gw_approval.mutex);

    /* Short-poll loop: use the platform's poll function to check for responses */
    while (timeout_sec > 0) {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec >= deadline.tv_sec) break;

        /* Check if response arrived via another thread (condvar signal) */
        pthread_mutex_lock(&g_gw_approval.mutex);
        if (g_gw_approval.response[0]) {
            char *resp = strdup(g_gw_approval.response);
            g_gw_approval.response[0] = '\0';
            g_gw_approval.pending = false;
            pthread_mutex_unlock(&g_gw_approval.mutex);
            return resp;
        }

        /* If we have a poll function, do a short poll for new updates */
        if (g_gw_approval.poll_fn) {
            pthread_mutex_unlock(&g_gw_approval.mutex);
            char *text = g_gw_approval.poll_fn(g_gw_approval.chat_id);
            if (text) {
                pthread_mutex_lock(&g_gw_approval.mutex);
                if (g_gw_approval.pending) {
                    gw_approval_match(g_gw_approval.platform, g_gw_approval.chat_id, text);
                    if (g_gw_approval.response[0]) {
                        char *resp = strdup(g_gw_approval.response);
                        g_gw_approval.response[0] = '\0';
                        g_gw_approval.pending = false;
                        pthread_mutex_unlock(&g_gw_approval.mutex);
                        free(text);
                        return resp;
                    }
                }
                pthread_mutex_unlock(&g_gw_approval.mutex);
                free(text);
            }
        } else {
            /* No poll function — wait on condvar with 1s timeout */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&g_gw_approval.cond, &g_gw_approval.mutex, &ts);
            pthread_mutex_unlock(&g_gw_approval.mutex);
        }

        timeout_sec--;

        /* Sleep 1s between polls to avoid busy-waiting */
        if (g_gw_approval.poll_fn) sleep(1);
    }

    /* Timeout — clean up */
    pthread_mutex_lock(&g_gw_approval.mutex);
    g_gw_approval.pending = false;
    g_gw_approval.response[0] = '\0';
    pthread_mutex_unlock(&g_gw_approval.mutex);
    return NULL;
}

/* Check if an incoming message is an approval response.
   Called from other platform threads (not the one that sent the prompt).
   Returns true if consumed. */

/* ================================================================
 *  Gateway stderr log-to-file with rotation (B15)
 * ================================================================ */


FILE *g_gw_log_fp = NULL;
char  g_gw_log_path[GW_LOG_PATH_MAX] = {0};

/** Open gateway log file, rotate if >10 MB, for persistent log capture. */


/* ================================================================
 *  P101: Rate limiter (token bucket)
 * ================================================================ */


/* ================================================================
 *  P101: HTTP connection pool
 * ================================================================ */

http_client_t *gw_pool_get_client(const char *endpoint) {
    pthread_mutex_lock(&g_gw.pool_mutex);

    /* Look for an idle client with matching endpoint */
    for (int i = 0; i < g_gw.pool_count; i++) {
        if (!g_gw.http_pool[i].in_use &&
            strcmp(g_gw.http_pool[i].endpoint, endpoint) == 0) {
            g_gw.http_pool[i].in_use = true;
            pthread_mutex_unlock(&g_gw.pool_mutex);
            return g_gw.http_pool[i].client;
        }
    }

    /* Create new client if pool not full */
    if (g_gw.pool_count < GW_POOL_MAX) {
        int i = g_gw.pool_count++;
        g_gw.http_pool[i].client = http_client_new(30);
        g_gw.http_pool[i].in_use = true;
        snprintf(g_gw.http_pool[i].endpoint, sizeof(g_gw.http_pool[i].endpoint), "%s", endpoint ? endpoint : "");
        g_gw.http_pool[i].last_used = gw_mono_time();
        pthread_mutex_unlock(&g_gw.pool_mutex);
        return g_gw.http_pool[i].client;
    }

    /* Pool full — return NULL, caller should create one-off */
    pthread_mutex_unlock(&g_gw.pool_mutex);
    return http_client_new(30);
}


/* ================================================================
 *  E27: HTTP keepalive per-platform (set via config)
 * ================================================================ */


/* E28: Message deduplication (TTL-based ring buffer) */
/* Forward declaration for process_update (defined below) */


/* ================================================================
 *  E29: Batch aggregation — coalesce fragmented messages
 * ================================================================ */


/* ================================================================
 *  Thread-safe agent chat
 * ================================================================ */

char *gateway_agent_chat(const char *message) {
    pthread_mutex_lock(&g_gw.agent_mutex);
    char *resp = agent_chat(&g_gw.agent, message);
    pthread_mutex_unlock(&g_gw.agent_mutex);
    return resp;
}

/*
 * P158: Build human-readable session description for the system prompt.
 * Returns number of chars written (like snprintf).
 */

/* Forward declarations for session management functions (defined below) */


/* Populate session source struct with the given values.
 * Strings are truncated to fit their fixed-size fields.
 * v306: added chat_topic, user_id_alt, chat_id_alt, guild_id, parent_chat_id, message_id */

/* GW15: forward declarations for static LRU cache functions */
static gw_session_source_t *source_cache_get(const char *key);

/* Thread-safe: set session source metadata for an existing session.
 * Platform threads call this after session_get_or_create() when they
 * have the metadata (chat_name, user_id, etc.) from their poll data.
 * Returns true if session was found and updated. */

/* ================================================================
 * GW15: Session sources LRU cache
 * Mirrors Python gateway/run.py _session_sources OrderedDict.
 * Fixed-size array: MRU at high indices, LRU at low indices.
 * ================================================================ */

/* Look up a source by key. Returns pointer to cached entry or NULL.
 * On hit, moves the entry to MRU position (re-insert at hive tail). */
static gw_session_source_t *source_cache_get(const char *key) {
    if (!key || !g_gw.source_cache) return NULL;
    pthread_mutex_lock(&g_gw.source_cache_mutex);
    hive_iter_t it;
    hive_iter_begin(g_gw.source_cache, &it);
    hive_handle_t hnd;
    gw_source_cache_entry_t *e;
    while (hive_iter_next(g_gw.source_cache, &it, &hnd, (void **)&e)) {
        if (strcmp(e->key, key) == 0) {
            gw_session_source_t *result = &e->source;
            pthread_mutex_unlock(&g_gw.source_cache_mutex);
            return result;
        }
    }
    pthread_mutex_unlock(&g_gw.source_cache_mutex);
    return NULL;
}

/* Insert or update a source in the cache. Evicts LRU entry if full. */

/* Public: get session source with LRU cache.
 * Checks cache first, falls back to session DB, populates cache on miss. */
gw_session_source_t *gw_session_get_source(const char *platform, const char *chat_id) {
    if (!platform || !chat_id) return NULL;

    /* Build lookup key */
    char key[192];
    snprintf(key, sizeof(key), "%s:%s", platform, chat_id);

    /* Check LRU cache first */
    gw_session_source_t *cached = source_cache_get(key);
    if (cached) return cached;

    /* Cache miss: look up in session pool */
    pthread_mutex_lock(&g_gw.session_mutex);
    int idx = session_find(platform, chat_id);
    if (idx >= 0) {
        /* Populate cache */
        gw_session_entry_t *se = session_at(idx);
        if (se)
            source_cache_put(key, &se->source);
        pthread_mutex_unlock(&g_gw.session_mutex);
        /* Return from cache (now MRU) */
        return source_cache_get(key);
    }
    pthread_mutex_unlock(&g_gw.session_mutex);
    return NULL;
}

/* PII-safe hash helper: deterministic 8-char hex via FNV-1a */

/* Port of Python gateway/session.py:_discord_tools_loaded
 * Returns true when Discord tools (discord/discord_admin) are available.
 * Checks: DISCORD_BOT_TOKEN in env + discord in gateway_platforms. */

/* Build a ## Current Session Context block for system prompt injection.
 * Port of Python gateway/session.py:build_session_context_prompt.
 * AG26: Port of Python gateway/session.py:build_session_context_prompt().
 * v306e: added PII-safe redaction, multi-user session detection,
 *        platform-specific behavioral notes.
 * Returns malloc'd string (caller must free) or NULL. */
char *build_session_context_prompt(const gw_session_source_t *src) {
    if (!src) return NULL;

    /* Estimate buffer size: 4KB base + platform-specific content */
    char *buf = malloc(8192);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t pos = 0;
    size_t sz = 8192;
#define BUF_APPEND(...) do { \
    int n = snprintf(buf + pos, sz - pos, __VA_ARGS__); \
    if (n > 0 && (size_t)n < sz - pos) pos += n; \
} while(0)

    BUF_APPEND("## Current Session Context\n\n");

    /* PII-safe: hash IDs in description */
    bool is_pii_safe = (strcmp(src->platform, "telegram") == 0 ||
                        strcmp(src->platform, "whatsapp") == 0 ||
                        strcmp(src->platform, "signal") == 0);

    if (strcmp(src->platform, "local") == 0) {
        BUF_APPEND("**Source:** CLI (the machine running this agent)\n");
    } else if (is_pii_safe && src->has_data) {
        /* PII-safe: hash IDs in description */
        char hash_buf[16];
        const char *uname = src->user_name[0] ? src->user_name : "";
        const char *cname = src->chat_name[0] ? src->chat_name : "";
        if (!uname[0] && src->user_id[0]) {
            pii_hash(src->user_id, hash_buf, sizeof(hash_buf));
            BUF_APPEND("**Source:** %s (user_%s)\n", src->platform, hash_buf);
        } else if (uname[0]) {
            BUF_APPEND("**Source:** %s (%s)\n", src->platform, uname);
        } else {
            BUF_APPEND("**Source:** %s\n", src->platform);
        }
        if (!cname[0] && src->chat_id[0]) {
            pii_hash(src->chat_id, hash_buf, sizeof(hash_buf));
            BUF_APPEND("**Chat:** chat_%s\n", hash_buf);
        } else if (cname[0]) {
            BUF_APPEND("**Chat:** %s\n", cname);
        }
    } else {
        char desc[512];
        session_source_description(src, desc, sizeof(desc));
        BUF_APPEND("**Source:** %s (%s)\n", src->platform, desc);
    }

    /* Channel topic */
    if (src->has_data && src->chat_topic[0]) {
        BUF_APPEND("**Channel Topic:** %s\n", src->chat_topic);
    }

    /* Multi-user session detection */
    bool is_multi_user = false;
    if (src->has_data && strcmp(src->chat_type, "group") == 0) {
        is_multi_user = true;
    }

    /* User identity (skipped for multi-user — sender names are prefixed per-message) */
    if (!is_multi_user) {
        if (src->has_data && src->user_name[0]) {
            BUF_APPEND("**User:** %s\n", src->user_name);
        } else if (src->has_data && src->user_id[0]) {
            BUF_APPEND("**User ID:** %s\n", src->user_id);
        }
    } else {
        const char *label = src->thread_id[0] ? "Multi-user thread" : "Multi-user session";
        BUF_APPEND("**Session type:** %s — messages are prefixed with [sender name]. ", label);
        BUF_APPEND("Multiple users may participate.\n");
    }

    /* Platform-specific behavioral notes */
    if (strcmp(src->platform, "slack") == 0) {
        BUF_APPEND("\n**Platform notes:** You are running inside Slack. "
                   "You do NOT have access to Slack-specific APIs — you cannot search "
                   "channel history, pin/unpin messages, manage channels, or list users. "
                   "Do not promise to perform these actions.\n");
    } else if (strcmp(src->platform, "discord") == 0) {
        BUF_APPEND("\n**Platform notes:** You are running inside Discord. "
                   "You do NOT have access to Discord-specific APIs — you cannot search "
                   "channel history, pin messages, manage roles, or list server members. "
                   "Do not promise to perform these actions.\n");
        /* Discord IDs for tool use — only when discord tools are loaded */
        if (src->has_data && discord_tools_loaded()) {
            BUF_APPEND("\n**Discord IDs (for the `discord` / `discord_admin` tools):**");
            if (src->guild_id[0]) BUF_APPEND(" Guild: `%s`", src->guild_id);
            if (src->thread_id[0] && src->parent_chat_id[0]) {
                BUF_APPEND(" Parent channel: `%s` Thread: `%s`", src->parent_chat_id, src->thread_id);
            } else if (src->chat_id[0]) {
                BUF_APPEND(" Channel: `%s`", src->chat_id);
            }
            if (src->message_id[0]) BUF_APPEND(" Message: `%s`", src->message_id);
            BUF_APPEND("\n");
        }
    } else if (strcmp(src->platform, "bluebubbles") == 0) {
        BUF_APPEND("\n**Platform notes:** You are responding via iMessage. "
                   "Keep responses short and conversational — think texts, not essays. "
                   "Structure longer replies as separate short thoughts, each separated "
                   "by a blank line. One idea per bubble, 1-3 sentences each.\n");
    } else if (strcmp(src->platform, "yuanbao") == 0) {
        BUF_APPEND("\n**Platform notes:** You are running inside Yuanbao. "
                   "You CAN send private (DM) messages via the send_message tool. "
                   "Use target='yuanbao:direct:<account_id>' for DM "
                   "and target='yuanbao:group:<group_code>' for group chat.\n");
    }

    /* Connected platforms */
    BUF_APPEND("\n**Connected Platforms:** local (files on this machine)");
    for (int i = 0; i < g_gw.platform_count; i++) {
        BUF_APPEND(", %s: Connected", g_gw.platforms[i]);
    }
    BUF_APPEND("\n");

    /* Home channels — use platform list as home channels */
    if (g_gw.platform_count > 0) {
        BUF_APPEND("\n**Home Channels (default destinations):**\n");
        for (int i = 0; i < g_gw.platform_count; i++) {
            BUF_APPEND("  - %s: Home\n", g_gw.platforms[i]);
        }
    }

    /* Delivery options for scheduled tasks */
    BUF_APPEND("\n**Delivery options for scheduled tasks:**\n");
    BUF_APPEND("- `\"origin\"` → Back to this chat (%s)\n",
               src->chat_name[0] ? src->chat_name : src->chat_id);
    BUF_APPEND("- `\"local\"` → Save to local files only (SLERMES_HOME/cron/output/)\n");
    for (int i = 0; i < g_gw.platform_count; i++) {
        BUF_APPEND("- `\"%s\"` → Home channel\n", g_gw.platforms[i]);
    }
    BUF_APPEND("\n*For explicit targeting, use `\"platform:chat_id\"` format "
               "if the user provides a specific chat ID.*\n");

#undef BUF_APPEND
    return buf;
}

/* Port of Python gateway/session.py:_should_reset(). */
/* Check if a session should be reset based on the configured policy.
 * Returns true if the session has expired (idle timeout or daily boundary).
 * Mirrors Python SessionStore._is_session_expired().
 * session_sec: seconds since last activity (monotonic time). */

/* Free a session entry (save, close DB, free agent, zero out).
 * Does NOT update session_count. */

/* ================================================================
 *  P102: Per-chat session management
 * ================================================================ */

/* Build session key: "platform:chat_id" */
/* Port of Python gateway/session.py:is_shared_multi_user_session
 * Return true when a non-DM session is shared across participants.
 *   - DMs are never shared.
 *   - Threads are shared unless thread_sessions_per_user is true.
 *   - Non-thread group/channel sessions are shared unless
 *     group_sessions_per_user is true (default: true = isolated per user). */

/* Port of Python gateway/session.py:build_session_key(). */

/* Find existing session entry by platform+chat_id. Returns index or -1. */

/* Port of Python agent/auxiliary_client.py:create(). */
/* Create a new session for a platform:chat_id pair. Returns index or -1. */

/* Get or create a session for platform:chat_id. Returns index or -1. */

/* Auto-save all active sessions */

/* Clean up expired sessions based on configured reset policy.
 * Called by cleanup thread. Replaces hardcoded 30-min idle TTL. */

/* ================================================================
 *  P186: MEDIA: prefix handling — route file paths to platform media APIs
 * ================================================================ */

/* Try to send a file via MEDIA: prefix. Returns true if handled. */

/* ================================================================
 *  Platform-aware message send
 * ================================================================ */


/* Port of Python gateway/platforms/base.py:send_typing().
 * AG26: Port of Python gateway/platforms/base.py:_send_typing().
 */

/* ================================================================
 *  P103: Platform interface implementation
 * ================================================================ */


gw_platform_t *gw_platform_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_gw.platform_def_count; i++) {
        if (strcasecmp(g_gw.platform_defs[i].name, name) == 0)
            return &g_gw.platform_defs[i];
    }
    return NULL;
}


/* 5C-252: Send emoji reaction (optional — NULL if platform doesn't support) */

/* P103: Vtable wrappers — these bridge the gw_platform_t signature
   (no http_client_t parameter) to the platform-specific functions
   (which need http). The http client is captured from g_gw.http. */


/* Generic shutdown for polling-based platforms.
   Threads have already exited via g_gw.running flag + pthread_join by the
   time this is called.  Per-platform cleanup (HTTP pool, sessions) is
   handled by the global cleanup path — this just logs the event. */


/* Context for tool event callback — passes platform + chat_id */
/* Also carries stream callback state for progressive response delivery */
/* gw_status_ctx_t is defined in gw_server_internals.h (shared across modules). */

/* Gateway tool event callback — sends status messages during agent processing */

/* Gateway stream callback — accumulates tokens and periodically
 * updates typing indicator to show the agent is generating.
 * Port of Python gateway/stream_consumer.py (minimal sync version). */

/* ================================================================
 *  Process a single update (called from platform threads)
 * ================================================================ */

void process_update(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text || !*text) return;

    /* SK06: Update platform scope — invalidates skill cache if platform changed */
    skill_cmd_set_platform(platform);

    printf("[gateway:%s] Message: %s\n", platform, text);

    /* E36: Apply post-receive hooks (may transform/filter incoming text) */
    char *modified_text = NULL;
    if (gw_hooks.post_receive_count > 0) {
        modified_text = gw_apply_post_receive_hooks(platform, chat_id, text);
        if (modified_text) {
            if (*modified_text) {
                text = modified_text;
            } else {
                /* Hook returned empty — message was consumed/filtered */
                free(modified_text);
                return;
            }
        }
    }

    /* Check if this message is an approval response (for parallel platform threads) */
    if (gw_approval_check_response(platform, chat_id, text))
        return;

    /* Check if this message is a clarify response (user replied to a clarify prompt) */
    if (gw_clarify_check_response(platform, chat_id, text))
        return;

    /* Set up approval send context for this platform/chat_id.
       When a dangerous command triggers approval_prompt_user(), it will
       use this context to send the prompt through the correct platform. */
    approval_set_gateway_send(gw_platform_send, platform, chat_id);

    /* Set up clarify send context for this platform/chat_id.
       When the clarify tool is invoked in gateway mode, it will use
       this context to send the prompt through the correct platform. */
    clarify_set_gateway_context(platform, chat_id, gw_platform_send);

    /* L08: Prepend any accumulated observe buffer before processing
     * a triggered message (one where the bot IS mentioned). */
    char *observe_ctx = gw_observe_consume(platform, chat_id);
    if (observe_ctx) {
        free(observe_ctx);
    }

    /* P101: Find platform index for rate limiting */
    int plat_idx = -1;
    for (int i = 0; i < g_gw.platform_count; i++) {
        if (strcasecmp(g_gw.platforms[i], platform) == 0) {
            plat_idx = i;
            break;
        }
    }

    /* P101: Check rate limit — if exceeded, queue the message */
    if (plat_idx >= 0 && !gw_rate_limit_check(plat_idx)) {
        gw_queue_push(platform, chat_id, text, NULL);
        printf("[gateway:%s] Rate limited, queued\n", platform);
        return;
    }

    /* P102: Get or create per-chat session */
    pthread_mutex_lock(&g_gw.session_mutex);
    int sess_idx = session_get_or_create(platform, chat_id);
    if (sess_idx < 0) {
        pthread_mutex_unlock(&g_gw.session_mutex);
        gateway_send(platform, chat_id,
                     "Error: Could not create session (max sessions reached)");
        return;
    }
    agent_state_t *session_agent = NULL;
    gw_session_entry_t *se = session_at(sess_idx);
    if (se) session_agent = &se->agent;
    pthread_mutex_unlock(&g_gw.session_mutex);
    if (!se) {
        gateway_send(platform, chat_id,
                     "Error: session lookup failed");
        return;
    }

    /* M12: Populate agent session context from source metadata */
    gw_session_source_t *src = &se->source;
    snprintf(session_agent->platform, sizeof(session_agent->platform), "%s",
             src->platform[0] ? src->platform : (platform ? platform : ""));
    snprintf(session_agent->chat_id, sizeof(session_agent->chat_id), "%s",
             src->chat_id[0] ? src->chat_id : (chat_id ? chat_id : ""));
    if (src->thread_id[0])
        snprintf(session_agent->thread_id, sizeof(session_agent->thread_id),
                 "%s", src->thread_id);
    if (src->user_id[0])
        snprintf(session_agent->user_id, sizeof(session_agent->user_id),
                 "%s", src->user_id);
    if (src->chat_name[0])
        snprintf(session_agent->chat_name, sizeof(session_agent->chat_name),
                 "%s", src->chat_name);
    if (src->user_name[0])
        snprintf(session_agent->user_name, sizeof(session_agent->user_name),
                 "%s", src->user_name);
    if (src->message_id[0])
        snprintf(session_agent->message_id, sizeof(session_agent->message_id),
                 "%s", src->message_id);
    snprintf(session_agent->session_key, sizeof(session_agent->session_key),
             "%s:%s", session_agent->platform, session_agent->chat_id);

    /* GW12: Last-resolved model fallback recovery.
     * If the session agent has no model (config cache miss), recover
     * from the per-session last-resolved cache. */
    if (!session_agent->llm.model[0]) {
        if (se->last_resolved_model[0]) {
            snprintf(session_agent->llm.model,
                     sizeof(session_agent->llm.model),
                     "%s", se->last_resolved_model);
            if (se->last_resolved_provider[0]) {
                snprintf(session_agent->llm.provider,
                         sizeof(session_agent->llm.provider),
                         "%s", se->last_resolved_provider);
            }
            fprintf(stderr, "[gateway] Recovered model from cache: %s\n",
                    session_agent->llm.model);
        } else if (g_gw.agent.llm.model[0]) {
            /* Global fallback: use the gateway's default agent model */
            snprintf(session_agent->llm.model,
                     sizeof(session_agent->llm.model),
                     "%s", g_gw.agent.llm.model);
            snprintf(session_agent->llm.provider,
                     sizeof(session_agent->llm.provider),
                     "%s", g_gw.agent.llm.provider);
            fprintf(stderr, "[gateway] Recovered model from global default: %s\n",
                    session_agent->llm.model);
        }
    }

    /* P102a: Inject session context prompt into system message on first use.
       This tells the agent where messages are coming from and what platforms
       are available for delivery. Mirrors Python build_session_context_prompt(). */
    if (!session_agent->system_message[0]) {
        char *ctx_prompt = build_session_context_prompt(&se->source);
        if (ctx_prompt) {
            context_set_system(session_agent, ctx_prompt);
            free(ctx_prompt);
        }
    }

    /* P109: Send typing indicator with 30s debounce */
    double now = gw_mono_time();
    if (now - se->last_busy_ack > 30.0) {
        gateway_send_typing(platform, chat_id);
        se->last_busy_ack = now;
    }

    /* GAP-5: Wire status callback for tool.started events during agent processing */
    gw_status_ctx_t status_ctx;
    status_ctx.platform = platform;
    status_ctx.chat_id = chat_id;
    status_ctx.last_status_ts = 0.0;
    status_ctx.last_stream_ts = 0.0;
    status_ctx.stream_len = 0;
    status_ctx.stream_buf[0] = '\0';
    session_agent->tool_event_cb = gateway_tool_event_cb;
    session_agent->tool_event_data = &status_ctx;
    session_agent->stream_cb = gateway_stream_cb;
    session_agent->stream_data = &status_ctx;

    /* ── Gateway command dispatch ──
     * Port of Python gateway/run.py command handling (event.get_command() dispatch).
     * Intercept /-prefixed commands before sending to the AI agent. */
    if (text[0] == '/') {
        /* Extract command: /cmd or /cmd@botname or /cmd args */
        const char *cmd_start = text + 1; /* skip / */
        char cmd_buf[64];
        const char *args = "";
        int ci = 0;
        /* Copy command name up to space or @ */
        while (*cmd_start && *cmd_start != ' ' && *cmd_start != '@' && ci < 63) {
            cmd_buf[ci++] = tolower((unsigned char)*cmd_start);
            cmd_start++;
        }
        cmd_buf[ci] = '\0';
        if (*cmd_start == '@') {
            /* Skip @botname */
            while (*cmd_start && *cmd_start != ' ') cmd_start++;
        }
        if (*cmd_start == ' ') {
            args = cmd_start + 1;
            while (*args == ' ') args++;
        }

        /* Handle known commands */
        if (strcmp(cmd_buf, "new") == 0 || strcmp(cmd_buf, "reset") == 0) {
            /* Reset session — free existing agent messages, keep session */
            pthread_mutex_lock(&g_gw.session_mutex);
            agent_free(session_agent);
            init_agent(session_agent);
            /* Re-setup the session's agent config from gateway config */
            memcpy(session_agent->llm.api_key, g_gw.agent.llm.api_key, sizeof(session_agent->llm.api_key));
            memcpy(session_agent->llm.model, g_gw.agent.llm.model, sizeof(session_agent->llm.model));
            memcpy(session_agent->llm.provider, g_gw.agent.llm.provider, sizeof(session_agent->llm.provider));
            session_agent->llm.max_tokens = g_gw.agent.llm.max_tokens;
            session_agent->llm.temperature = g_gw.agent.llm.temperature;
            if (se) {
                se->last_resolved_model[0] = '\0';
                se->last_resolved_provider[0] = '\0';
            }
            pthread_mutex_unlock(&g_gw.session_mutex);
            gateway_send(platform, chat_id, "Session reset.");
            free(modified_text);
            return;
        } else if (strcmp(cmd_buf, "stop") == 0 || strcmp(cmd_buf, "cancel") == 0) {
            /* Interrupt agent by setting its interrupt flag */
            session_agent->interrupted = true;
            gateway_send(platform, chat_id, "Agent interrupted.");
            free(modified_text);
            return;
        } else if (strcmp(cmd_buf, "help") == 0) {
            char help[1024];
            snprintf(help, sizeof(help),
                     "Available commands:\n"
                     "/new — Start a new session\n"
                     "/stop — Interrupt the current response\n"
                     "/help — Show this help\n"
                     "/model <name> — Switch model\n"
                     "/auth <provider> [key] — Manage auth\n"
                     "/reload — Reload configuration\n"
                     "/status — Show gateway status\n");
            gateway_send(platform, chat_id, help);
            free(modified_text);
            return;
        } else if (strcmp(cmd_buf, "start") == 0) {
            /* Telegram sends /start on bot launch — ignore, no response needed */
            free(modified_text);
            return;
        } else if (strcmp(cmd_buf, "status") == 0) {
            char status[512];
            snprintf(status, sizeof(status),
                     "Gateway: running\n"
                     "Platform: %s\n"
                     "Provider: %s\n"
                     "Model: %s\n",
                     platform,
                     g_gw.agent.llm.provider,
                     g_gw.agent.llm.model);
            gateway_send(platform, chat_id, status);
            free(modified_text);
            return;
        } else if (strcmp(cmd_buf, "reload") == 0) {
            /* Re-read config (env vars already loaded at startup) */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            hermes_config_load_env(&cfg);
            /* Update gateway agent config with any new values */
            if (cfg.model[0]) snprintf(g_gw.agent.llm.model, sizeof(g_gw.agent.llm.model), "%s", cfg.model);
            if (cfg.provider[0]) snprintf(g_gw.agent.llm.provider, sizeof(g_gw.agent.llm.provider), "%s", cfg.provider);
            if (cfg.api_key[0]) snprintf(g_gw.agent.llm.api_key, sizeof(g_gw.agent.llm.api_key), "%s", cfg.api_key);
            gateway_send(platform, chat_id, "Configuration reloaded.");
            free(modified_text);
            return;
        } else if (strcmp(cmd_buf, "model") == 0) {
            if (args[0]) {
                snprintf(session_agent->llm.model, sizeof(session_agent->llm.model), "%s", args);
                char buf[256];
                snprintf(buf, sizeof(buf), "✅ Model switched to: %s", args);
                gateway_send(platform, chat_id, buf);
            } else {
                char buf[256];
                snprintf(buf, sizeof(buf), "Current model: %s\nUsage: /model <name>",
                        session_agent->llm.model);
                gateway_send(platform, chat_id, buf);
            }
            free(modified_text);
            return;
        }
        /* Unknown command — let it fall through to the AI agent */
    }

    /* Run agent on per-chat session.
     *
     * Route through the faithful GatewayRunner turn core when available: it
     * applies /model session overrides, resolves session reasoning config,
     * guards against superseded (stale run-generation) turns, wraps native
     * images + observed context, calls run_conversation, then assembles the
     * result dict (empty-response normalization, MEDIA auto-append, provider
     * sanitize). Falls back to the direct agent_chat path if the runner or
     * turn core is unavailable. */
    char *resp = NULL;
    json_node_t *turn_result = NULL;
    if (g_runner) {
        int run_gen = gateway_runner_begin_session_run_generation(
            g_runner, session_agent->session_key);
        gw_turn_input_t turn_in;
        memset(&turn_in, 0, sizeof(turn_in));
        turn_in.message = text;
        turn_in.context_prompt = NULL;   /* system prompt already set on state */
        turn_in.session_key = session_agent->session_key;
        turn_in.session_id = session_agent->session_id;
        turn_in.platform = platform;
        turn_in.observed_context = NULL;
        turn_in.run_generation = run_gen;
        turn_result = gateway_runner_run_agent_inner(g_runner, session_agent,
                                                     &turn_in);
        if (turn_result) {
            const char *fr = gw_turn_result_final_response(turn_result);
            resp = strdup(fr ? fr : "");
        } else {
            /* Superseded turn (a newer message bumped the generation) — drop
             * this reply silently, exactly as the Python runner does. */
            session_agent->tool_event_cb = NULL;
            session_agent->tool_event_data = NULL;
            session_agent->stream_cb = NULL;
            session_agent->stream_data = NULL;
            free(modified_text);
            return;
        }
    } else {
        resp = agent_chat(session_agent, text);
    }
    /* Clear callbacks after the turn returns — context is stack-local */
    session_agent->tool_event_cb = NULL;
    session_agent->tool_event_data = NULL;
    session_agent->stream_cb = NULL;
    session_agent->stream_data = NULL;
    if (resp) {
        /* P159: Redact secrets before sending response to chat.
           hermes_redact() handles API keys, tokens, JWTs, and
           configured patterns via key:value and free-text prefix matching. */
        char *redacted = hermes_redact(resp);
        /* P160: Sanitize provider errors for Telegram. The turn core already
           sanitizes when routed through the runner; this stays for the
           agent_chat fallback path and is idempotent on clean text. */
        char *sanitized = gateway_sanitize_response(platform, redacted ? redacted : resp);
        gateway_send(platform, chat_id, sanitized ? sanitized : (redacted ? redacted : resp));
        free(sanitized);
        free(redacted);
        free(resp);
        /* GW12: Cache the resolved model/provider for fallback recovery */
        if (session_agent->llm.model[0] && se) {
            snprintf(se->last_resolved_model,
                     sizeof(se->last_resolved_model),
                     "%s", session_agent->llm.model);
            snprintf(se->last_resolved_provider,
                     sizeof(se->last_resolved_provider),
                     "%s", session_agent->llm.provider);
        }
    }
    if (turn_result) json_free(turn_result);
    free(modified_text);
}

/* ================================================================
 *  Per-platform thread functions
 * ================================================================ */

/* Telegram-specific: poll for a response from a specific chat_id.
   Called during approval wait to short-poll Telegram for user's y/n/a response.
   Returns strdup'd text or NULL. */







/* ================================================================
 *  Signal handler
 * ================================================================ */


/* ================================================================
 *  Platform setup helpers
 * ================================================================ */

typedef struct {
    const char *name;
    bool (*setup)(void);
    void *(*thread_fn)(void *);
    int arg_int; /* For port numbers etc. */
} platform_def_t;


/* Port of Python hermes_cli/gateway.py:_setup_whatsapp(). */


/* Port of Python hermes_cli/gateway.py:_setup_signal(). */

/* Setup for API Server platform */



/* HomeAssistant setup + thread */


/* SMS setup + thread */


/* Port of Python hermes_cli/gateway.py:_setup_feishu(). */
/* Feishu setup */


/* Port of Python hermes_cli/gateway.py:_setup_wecom(). */
/* WeCom setup */


/* Port of Python hermes_cli/gateway.py:_setup_dingtalk(). */
/* DingTalk setup */


/* QQ Bot setup */


/* BlueBubbles setup */


/* ================================================================
 *  Get port from env with HERMES_ or SLERMES_ prefix
 * ================================================================ */


/* ================================================================
 *  Gateway entry point
 * ================================================================ */



/* weixin setup + thread */
extern bool weixin_init(const char *token, const char *account_id);
extern void weixin_start(void);
extern void weixin_stop(void);

/* Port of Python hermes_cli/gateway.py:_setup_weixin(). */


/* yuanbao setup + thread */
extern bool yuanbao_init(const char *app_id, const char *app_secret,
                         const char *bot_id, const char *ws_url,
                         const char *api_domain);
extern void yuanbao_start(void);
extern void yuanbao_stop(void);



/* Port of Python hermes_cli/dump.py:_gateway_status(). */
/* ── Gateway subcommand: status ───────────────────────────────── */

/* Port of Python hermes_cli/gateway.py:_gateway_list(). */
/* ── Gateway subcommand: list ─────────────────────────────────── */

/* Periodic cleanup thread — evicts idle sessions every 60s */


int hermes_gateway_main(int argc, char **argv) {
    /* Subcommand dispatch */
    if (argc > 1 && argv[1] && argv[1][0] != '-') {
        if (strcmp(argv[1], "status") == 0)
            return cmd_gateway_status();
        if (strcmp(argv[1], "list") == 0)
            return cmd_gateway_list();
        if (strcmp(argv[1], "start") == 0) {
            /* Shift args forward so --platform and other flags still work */
            argc--;
            argv++;
        }
    }

    memset(&g_gw, 0, sizeof(g_gw));
    g_gw.running = true;
    g_gw.poll_interval = 1;
    g_gw.tg_offset = 0;

    /* Load config to get gateway settings (overrides defaults below) */
    hermes_config_load(&g_gw.config, NULL);
    hermes_config_load_env(&g_gw.config);
    /* Load the authoritative gateway_config_t (per-platform `extra` settings
     * such as authorization_is_upstream / dm_policy / unauthorized_dm_behavior)
     * that the authz mixin reads. */
    gateway_config_load_global();

    /* Faithful port of gateway/run.py GatewayRunner startup:
     *   reason = _own_policy_open_startup_violation(self.config)
     *   if reason: log error; write_runtime_status(startup_failed); _request_clean_exit(reason)
     * Refuse to start when an "open" policy platform lacks the allow-all opt-in. */
    {
        const gateway_config_t *startup_cfg = gateway_config_get_global();
        char *violation = startup_cfg ? gw_own_policy_open_startup_violation(startup_cfg) : NULL;
        if (violation) {
            const char *platform_value = violation;
            const char *colon = strchr(violation, ':');
            if (colon) platform_value = violation;  /* keep full reason for status */
            fprintf(stderr,
                "Refusing to start: %s has dm_policy/group_policy set to 'open' "
                "but neither GATEWAY_ALLOW_ALL_USERS nor the platform allow-all flag is enabled.\n",
                platform_value);
            gw_update_platform_runtime_status(NULL, "startup_failed", NULL, violation);
            free(violation);
            return 1;
        }
    }

    g_gw.auto_continue_freshness_secs = g_gw.config.gateway_auto_continue_freshness > 0.0
        ? g_gw.config.gateway_auto_continue_freshness : 3600.0;  /* default 1h, 0=disabled */
    strcpy(g_gw.reset_policy_mode, "idle");
    g_gw.reset_policy_at_hour = 4;
    g_gw.reset_policy_idle_min = 1440;
    g_gw.max_concurrent_sessions = g_gw.config.gateway_max_concurrent_sessions > 0
        ? g_gw.config.gateway_max_concurrent_sessions : 0;  /* M13: 0 = unlimited */
    pthread_mutex_init(&g_gw.agent_mutex, NULL);

    /* Open log file with rotation (B15) */
    gw_log_open();

    /* P101: Initialize message queue and HTTP pool */
    gw_queue_init();
    g_gw.observe_buffer[0] = '\0';
    pthread_mutex_init(&g_gw.observe_mutex, NULL);
    pthread_mutex_init(&g_gw.pool_mutex, NULL);
    /* 5A-222: Configurable keepalive from env (matching Python _http_client_limits) */
    {
        const char *env_keepalive = getenv("HERMES_GATEWAY_KEEPALIVE_EXPIRY");
        if (env_keepalive) {
            double val = atof(env_keepalive);
            if (val > 0) g_gw.pool_keepalive_expiry = val;
        }
    }
    /* P102: Initialize session pool */
    pthread_mutex_init(&g_gw.session_mutex, NULL);
    /* GW15: Initialize session sources LRU cache */
    g_gw.source_cache_max = 512;
    g_gw.source_cache_count = 0;
    pthread_mutex_init(&g_gw.source_cache_mutex, NULL);
    {
        char db_path[GW_PATH_MAX];
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        snprintf(db_path, sizeof(db_path), "%s/.slermes/sessions", home ? home : "/tmp");
        snprintf(g_gw.session_db_path, sizeof(g_gw.session_db_path), "%s", db_path);
    }

    /* Parse --platform flag for backwards compat (single-platform mode) */
    char cli_platform[32] = {0};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--platform") == 0 && i + 1 < argc) {
            snprintf(cli_platform, sizeof(cli_platform), "%s", argv[++i]);
        }
    }

    /* Load config */
    hermes_config_load(&g_gw.config, NULL);
    hermes_config_load_env(&g_gw.config);

    /* Create default HTTP client */
    g_gw.http = http_client_new_with_retry(30, 3, 1000);

    /* Initialize agent */
    init_agent(&g_gw.agent);
    tools_init_all();
    g_gw.agent.tools = *get_registry();

    /* Copy config to agent */
    memcpy(g_gw.agent.llm.base_url, g_gw.config.base_url, sizeof(g_gw.agent.llm.base_url));
    memcpy(g_gw.agent.llm.api_key, g_gw.config.api_key, sizeof(g_gw.agent.llm.api_key));
    memcpy(g_gw.agent.llm.model, g_gw.config.model, sizeof(g_gw.agent.llm.model));
    memcpy(g_gw.agent.llm.provider, g_gw.config.provider, sizeof(g_gw.agent.llm.provider));
    g_gw.agent.max_iterations = g_gw.config.max_turns;
    g_gw.agent.compress_enabled = g_gw.config.compress_enabled;
    /* P150: Forward enabled/disabled toolsets to agent */
    if (g_gw.config.tools.enabled_toolsets[0])
        snprintf(g_gw.agent.enabled_toolsets, sizeof(g_gw.agent.enabled_toolsets), "%s", g_gw.config.tools.enabled_toolsets);
    if (g_gw.config.tools.disabled_toolsets[0])
        snprintf(g_gw.agent.disabled_toolsets, sizeof(g_gw.agent.disabled_toolsets), "%s", g_gw.config.tools.disabled_toolsets);
    /* Also copy yolo/fast/verbose for gateway runtime */
    approval_set_yolo(g_gw.config.yolo_mode);

    /* Apply CDP URL */
    if (g_gw.config.cdp_url[0])
        cdp_set_url(g_gw.config.cdp_url);

    printf("[gateway] WuBu Slermes Gateway v%s\n", HERMES_VERSION);

    /* Create the shared GatewayRunner that owns the faithful session-state
     * maps consumed by the turn core (gateway_runner_run_agent_inner). */
    g_runner = gateway_runner_create(NULL);

    /* Determine platforms to run */
    platform_def_t all_platforms[] = {
        {"telegram",   setup_telegram,   thread_poll_telegram,   0},
        {"discord",    setup_discord,    thread_poll_discord,    0},
        {"slack",      setup_slack,      thread_poll_slack,      0},
        {"matrix",     setup_matrix,     thread_poll_matrix,     0},
        {"mattermost", setup_mattermost, thread_poll_mattermost, 0},
        {"webhook",    setup_webhook,    thread_webhook,         0},
        {"whatsapp",   setup_whatsapp,   thread_webhook,         0},
        {"email",      setup_email,      thread_poll_email,      0},
        {"signal",     setup_signal,     thread_poll_signal,     0},
        {"homeassistant", setup_ha,      thread_poll_ha,         0},
        {"sms",        setup_sms,        thread_poll_sms,        0},
        {"api_server", setup_api_server, thread_webhook,         0},
        {"feishu",     setup_feishu,     thread_poll_feishu,     0},
        {"wecom",      setup_wecom,      thread_poll_wecom,      0},
        {"dingtalk",   setup_dingtalk,   thread_poll_dingtalk,   0},
        {"qqbot",      setup_qqbot,      thread_poll_qqbot,      0},
        {"bluebubbles",setup_bluebubbles,thread_poll_bluebubbles,0},
        {"msgraph_webhook", setup_msgraph_webhook, thread_msgraph_webhook, 0},
        {"weixin", setup_weixin, thread_weixin, 0},
        {"yuanbao", setup_yuanbao, thread_yuanbao, 0},
        {NULL, NULL, NULL, 0}
    };

    /* P103: Register base platform interface adapters.
     * Each polling-based platform gets its adapter populated from
     * the individual static functions in the platform modules. */

    /* Build platform list:
     * 1. If --platform flag given, add that single one
     * 2. Otherwise, read from config.gateway_platforms (comma-separated)
     * 3. Fallback: "telegram" if no platforms specified */
    char platforms_buf[256];
    platforms_buf[0] = '\0';

    if (cli_platform[0]) {
        snprintf(platforms_buf, sizeof(platforms_buf), "%s", cli_platform);
    } else if (g_gw.config.gateway_platforms[0]) {
        snprintf(platforms_buf, sizeof(platforms_buf), "%s",
                 g_gw.config.gateway_platforms);
    } else {
        /* Default: try env var HERMES_GATEWAY_PLATFORMS */
        const char *env_platforms = getenv("HERMES_GATEWAY_PLATFORMS");
        if (env_platforms)
            snprintf(platforms_buf, sizeof(platforms_buf), "%s", env_platforms);
        else
            snprintf(platforms_buf, sizeof(platforms_buf), "telegram");
    }

    /* Parse comma-separated platform list and start each */
    char *saveptr = NULL;
    char *tok = strtok_r(platforms_buf, ", ", &saveptr);
    while (tok && g_gw.platform_count < GW_MAX_PLATFORMS) {
        /* Find platform definition */
        bool found = false;
        for (int i = 0; all_platforms[i].name; i++) {
            if (strcasecmp(tok, all_platforms[i].name) == 0) {
                /* Setup platform */
                if (all_platforms[i].setup()) {
                    snprintf(g_gw.platforms[g_gw.platform_count],
                             sizeof(g_gw.platforms[0]), "%s", tok);

                    /* Set arg_int for webhook/whatsapp (port number) */
                    all_platforms[i].arg_int = get_webhook_port();

                    printf("[gateway] Starting platform: %s\n", tok);

                    /* Create thread */
                    if (pthread_create(&g_gw.threads[g_gw.platform_count], NULL,
                                       all_platforms[i].thread_fn,
                                       &all_platforms[i].arg_int) == 0) {
                        /* P101: Initialize rate limiter for this platform */
                        double rps = (strcmp(tok, "email") == 0) ? 0.2 :
                                     (strcmp(tok, "sms") == 0) ? 0.1 :
                                     (strcmp(tok, "signal") == 0) ? 0.5 :
                                     (strcmp(tok, "telegram") == 0) ? 30.0 :
                                     (strcmp(tok, "discord") == 0) ? 5.0 : 3.0;
                        gw_rate_limit_init(g_gw.platform_count, rps, rps * 3);
                        /* P103: Register this platform in the interface registry */
                        {
                            gw_platform_t plat;
                            memset(&plat, 0, sizeof(plat));
                            plat.name = g_gw.platforms[g_gw.platform_count];
                            /* All polling-based platforms: init=setup, send=poll-based functions */
                            plat.init = all_platforms[i].setup;
                            plat.shutdown = poll_platform_shutdown;
                            gw_platform_register(&plat);
                            /* Wire platform-specific vtable callbacks */
                            {
                                gw_platform_t *p = gw_platform_find(
                                    g_gw.platforms[g_gw.platform_count]);
                                if (p && strcmp(p->name, "telegram") == 0)
                                    p->send_reaction = telegram_vtable_send_reaction;
                            }
                        }
                        g_gw.platform_count++;
                    } else {
                        fprintf(stderr, "Error: Failed to create thread for %s\n", tok);
                    }
                } else {
                    fprintf(stderr, "[gateway] Skipping platform %s (setup failed)\n", tok);
                }
                found = true;
                break;
            }
        }
        if (!found)
            fprintf(stderr, "Warning: Unknown platform '%s'\n", tok);
        tok = strtok_r(NULL, ", ", &saveptr);
    }

    if (g_gw.platform_count == 0) {
        fprintf(stderr, "Error: No platforms could be started.\n");
        goto cleanup;
    }

    printf("[gateway] %d platform(s) running, %s configured\n",
           g_gw.platform_count, platforms_buf);

    /* Setup signal handler */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Wire cron notifications through gateway */
    cron_notify_set_send_fn(gw_platform_send);

    /* Wire approval prompts through gateway.
       The platform+chat_id are set per-message in process_update(). */
    approval_set_gateway_wait(gw_approval_wait_response);

    /* Wire clarify prompts through gateway.
       Per-message context (platform/chat_id/send_fn) is set in process_update(). */
    clarify_set_gateway_send(NULL, NULL, NULL);
    clarify_set_gateway_wait(gw_clarify_wait_response);
    clarify_set_gateway_begin(gw_clarify_begin);

    /* Set cron notification channel from env var (format: "platform:chat_id") */
    {
        const char *cron_chan = getenv("HERMES_CRON_NOTIFY_CHANNEL");
        if (cron_chan && cron_chan[0]) {
            cron_notify_set_channel(cron_chan);
            printf("[gateway] Cron notification channel: %s\n", cron_chan);
        }
    }

    /* Spawn session cleanup thread (reaps idle sessions every 60s) */
    pthread_t cleanup_thread;
    pthread_create(&cleanup_thread, NULL, thread_cleanup_sessions, NULL);

    /* GW13: Spawn kanban notifier thread — polls kanban_notify_subs and
     * delivers terminal events to subscribed platform/chat/thread targets.
     * Gated by dispatch_in_gateway config (default true). */
    {
        /* Read dispatch_in_gateway from config/env */
        const char *env_dispatch = getenv("HERMES_KANBAN_DISPATCH_IN_GATEWAY");
        g_gw.kanban_notifier_enabled = true;  /* default */
        if (env_dispatch && env_dispatch[0]) {
            if (strcmp(env_dispatch, "0") == 0 || strcmp(env_dispatch, "false") == 0 ||
                strcmp(env_dispatch, "no") == 0 || strcmp(env_dispatch, "off") == 0) {
                g_gw.kanban_notifier_enabled = false;
            }
        }
        g_gw.kanban_notifier_interval_sec = 5;
        g_gw.kanban_notifier_max_fail = 3;
        g_gw.kanban_notifier_profile[0] = '\0';

        if (g_gw.kanban_notifier_enabled) {
            pthread_t kanban_notifier_thread;
            if (pthread_create(&kanban_notifier_thread, NULL,
                               thread_kanban_notifier, NULL) == 0) {
                printf("[gateway] Kanban notifier started (interval=%ds)\n",
                       g_gw.kanban_notifier_interval_sec);
            } else {
                fprintf(stderr, "[gateway] Failed to start kanban notifier thread\n");
            }
        } else {
            printf("[gateway] Kanban notifier disabled (dispatch_in_gateway=false)\n");
        }
    }

    printf("[gateway] %d platform(s) running. Press Ctrl+C to stop\n",
           g_gw.platform_count);

    /* Wait for all threads */
    for (int i = 0; i < g_gw.platform_count; i++)
        pthread_join(g_gw.threads[i], NULL);

    /* Wait for cleanup thread (will exit after recognizing g_gw.running=false) */
    pthread_join(cleanup_thread, NULL);

cleanup:
    /* Destroy the shared GatewayRunner (session-state maps). */
    if (g_runner) { gateway_runner_destroy(g_runner); g_runner = NULL; }
    /* Shutdown all platforms */
    gw_platform_shutdown_all();
    /* P102: Save and free all sessions */
    session_save_all();
    if (g_gw.sessions) {
        hive_iter_t it;
        hive_iter_begin(g_gw.sessions, &it);
        hive_handle_t hnd;
        gw_session_entry_t *se;
        while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
            if (se->in_use) {
                if (se->db) db_close(se->db);
                agent_free(&se->agent);
            }
            free(se);
            hive_erase(g_gw.sessions, hnd);
        }
    }
    pthread_mutex_destroy(&g_gw.session_mutex);
    pthread_mutex_destroy(&g_gw.agent_mutex);
    /* P101: Cleanup HTTP pool and queue */
    gw_pool_cleanup();
    pthread_mutex_destroy(&g_gw.pool_mutex);
    pthread_mutex_destroy(&g_gw.queue_mutex);
    pthread_cond_destroy(&g_gw.queue_cond);
    agent_free(&g_gw.agent);
    http_client_free(g_gw.http);
    gw_log_close();
    printf("[gateway] Shutdown complete\n");
    return g_gw.platform_count > 0 ? 0 : 1;
}
