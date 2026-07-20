/*
 * gw_session.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "gateway_helpers.h"
#include "hermes_skill_commands.h"
#include "hermes_logger.h"
#include "hermes_telegram_filter.h"
#include "gw_server_internals.h"
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

int session_source_description(const gw_session_source_t *src,
                                char *buf, size_t sz) {
    if (!src || !buf || sz == 0) return 0;

    if (!src->has_data) {
        return snprintf(buf, sz, "session (%s:%s)",
                        src->platform[0] ? src->platform : "?",
                        src->chat_id[0] ? src->chat_id : "?");
    }

    if (strcmp(src->chat_type, "dm") == 0) {
        const char *who = src->user_name[0] ? src->user_name
                       : src->user_id[0]    ? src->user_id
                       :                        "user";
        return snprintf(buf, sz, "DM with %s (%s)", who, src->platform);
    } else if (strcmp(src->chat_type, "group") == 0) {
        const char *name = src->chat_name[0] ? src->chat_name : src->chat_id;
        int n = snprintf(buf, sz, "group: %s", name);
        if (src->guild_id[0])
            n += snprintf(buf + n, sz - (size_t)n > 0 ? sz - (size_t)n : 0,
                          " guild:%s", src->guild_id);
        if (src->thread_id[0])
            n += snprintf(buf + n, sz - (size_t)n > 0 ? sz - (size_t)n : 0,
                          " thread:%s", src->thread_id);
        n += snprintf(buf + n, sz - (size_t)n > 0 ? sz - (size_t)n : 0,
                      " (%s)", src->platform);
        return n;
    } else if (strcmp(src->chat_type, "channel") == 0) {
        return snprintf(buf, sz, "channel: %s (%s)",
                        src->chat_name[0] ? src->chat_name : src->chat_id,
                        src->platform);
    }
    return snprintf(buf, sz, "%s (%s:%s)",
                    src->chat_name[0] ? src->chat_name : src->chat_id,
                    src->platform, src->chat_id);
}

void session_source_set(gw_session_source_t *src,
                         const char *platform,
                         const char *chat_id,
                         const char *chat_name,
                         const char *chat_type,
                         const char *user_id,
                         const char *user_name,
                         const char *thread_id,
                         const char *chat_topic,
                         const char *user_id_alt,
                         const char *chat_id_alt,
                         const char *guild_id,
                         const char *parent_chat_id,
                         const char *message_id,
                         bool is_bot) {
    if (!src) return;
    snprintf(src->platform, sizeof(src->platform), "%s", platform ? platform : "");
    snprintf(src->chat_id, sizeof(src->chat_id), "%s", chat_id ? chat_id : "");
    snprintf(src->chat_name, sizeof(src->chat_name), "%s", chat_name ? chat_name : "");
    snprintf(src->chat_type, sizeof(src->chat_type), "%s", chat_type ? chat_type : "dm");
    snprintf(src->user_id, sizeof(src->user_id), "%s", user_id ? user_id : "");
    snprintf(src->user_name, sizeof(src->user_name), "%s", user_name ? user_name : "");
    snprintf(src->thread_id, sizeof(src->thread_id), "%s", thread_id ? thread_id : "");
    snprintf(src->chat_topic, sizeof(src->chat_topic), "%s", chat_topic ? chat_topic : "");
    snprintf(src->user_id_alt, sizeof(src->user_id_alt), "%s", user_id_alt ? user_id_alt : "");
    snprintf(src->chat_id_alt, sizeof(src->chat_id_alt), "%s", chat_id_alt ? chat_id_alt : "");
    snprintf(src->guild_id, sizeof(src->guild_id), "%s", guild_id ? guild_id : "");
    snprintf(src->parent_chat_id, sizeof(src->parent_chat_id), "%s", parent_chat_id ? parent_chat_id : "");
    snprintf(src->message_id, sizeof(src->message_id), "%s", message_id ? message_id : "");
    src->is_bot = is_bot;
    src->has_data = true;
}

bool gw_session_set_source(const char *platform, const char *chat_id,
                            const gw_session_source_t *source) {
    if (!platform || !chat_id || !source) return false;
    pthread_mutex_lock(&g_gw.session_mutex);
    int idx = session_find(platform, chat_id);
    if (idx >= 0) {
        g_gw.sessions[idx].source = *source;
        pthread_mutex_unlock(&g_gw.session_mutex);
        /* GW15: Update LRU cache */
        char key[192];
        snprintf(key, sizeof(key), "%s:%s", platform, chat_id);
        source_cache_put(key, source);
        return true;
    }
    pthread_mutex_unlock(&g_gw.session_mutex);
    return false;
}

void source_cache_put(const char *key, const gw_session_source_t *source) {
    if (!key || !source) return;
    pthread_mutex_lock(&g_gw.source_cache_mutex);

    /* Check if already present (update in place, move to MRU) */
    int i;
    for (i = 0; i < g_gw.source_cache_count; i++) {
        if (g_gw.source_cache[i].occupied &&
            strcmp(g_gw.source_cache[i].key, key) == 0) {
            g_gw.source_cache[i].source = *source;
            /* Move to MRU */
            if (i != g_gw.source_cache_count - 1) {
                int last = g_gw.source_cache_count - 1;
                struct { char key[192]; gw_session_source_t source; bool occupied; }
                    tmp = {0};
                memcpy(&tmp, &g_gw.source_cache[i], sizeof(tmp));
                memcpy(&g_gw.source_cache[i], &g_gw.source_cache[last], sizeof(tmp));
                memcpy(&g_gw.source_cache[last], &tmp, sizeof(tmp));
            }
            pthread_mutex_unlock(&g_gw.source_cache_mutex);
            return;
        }
    }

    /* Evict LRU (index 0) if full */
    if (g_gw.source_cache_count >= g_gw.source_cache_max) {
        /* Shift all entries left by 1 (evict index 0) */
        memmove(&g_gw.source_cache[0], &g_gw.source_cache[1],
                (g_gw.source_cache_count - 1) * sizeof(g_gw.source_cache[0]));
        g_gw.source_cache_count--;
    }

    /* Insert at MRU position (end) */
    int idx = g_gw.source_cache_count;
    strncpy(g_gw.source_cache[idx].key, key, sizeof(g_gw.source_cache[idx].key) - 1);
    g_gw.source_cache[idx].key[sizeof(g_gw.source_cache[idx].key) - 1] = '\0';
    g_gw.source_cache[idx].source = *source;
    g_gw.source_cache[idx].occupied = true;
    g_gw.source_cache_count++;

    pthread_mutex_unlock(&g_gw.source_cache_mutex);
}

void pii_hash(const char *input, char *out, size_t out_sz) {
    if (!input || !*input) { out[0] = '\0'; return; }
    uint32_t hash = 2166136261u;
    while (*input) {
        hash ^= (unsigned char)*input++;
        hash *= 16777619u;
    }
    snprintf(out, out_sz, "%08x", hash);
}

bool discord_tools_loaded(void) {
    const char *token = getenv("DISCORD_BOT_TOKEN");
    if (!token || !token[0]) return false;
    for (int i = 0; i < g_gw.platform_count; i++) {
        if (strcasecmp(g_gw.platforms[i], "discord") == 0)
            return true;
    }
    return false;
}

bool session_should_reset(double session_sec) {
    const char *mode = g_gw.reset_policy_mode;

    if (strcmp(mode, "none") == 0)
        return false;

    /* Idle check: used in "idle" and "both" modes */
    if (strcmp(mode, "idle") == 0 || strcmp(mode, "both") == 0) {
        double idle_sec = (double)g_gw.reset_policy_idle_min * 60.0;
        if (session_sec > idle_sec)
            return true;
    }

    /* Daily check: used in "daily" and "both" modes.
     * A session that was last active before today's reset hour is expired.
     * We approximate by checking if idle time exceeds the time from
     * the reset hour to now (with wraparound). */
    if (strcmp(mode, "daily") == 0 || strcmp(mode, "both") == 0) {
        time_t now_raw = time(NULL);
        struct tm *now_tm = localtime(&now_raw);
        int reset_hour = g_gw.reset_policy_at_hour;

        /* Seconds since today's reset hour */
        int sec_since_reset = (now_tm->tm_hour - reset_hour) * 3600
                            + now_tm->tm_min * 60
                            + now_tm->tm_sec;
        if (sec_since_reset < 0)
            sec_since_reset += 86400;  /* wrapped to previous day */

        if (session_sec > (double)sec_since_reset)
            return true;
    }

    return false;
}

void session_free(int idx) {
    if (idx < 0 || idx >= GW_SESSIONS_MAX) return;
    if (!g_gw.sessions[idx].in_use) return;
    if (g_gw.sessions[idx].db) {
        db_save(g_gw.sessions[idx].db, g_gw.sessions[idx].session_id, NULL);
        db_close(g_gw.sessions[idx].db);
    }
    agent_free(&g_gw.sessions[idx].agent);
    memset(&g_gw.sessions[idx], 0, sizeof(g_gw.sessions[idx]));
}

/* Port of Python gateway/session.py:is_shared_multi_user_session().
 * Return true when a non-DM session is shared across participants:
 *   - DMs are never shared.
 *   - Threads are shared unless thread_sessions_per_user is true.
 *   - Non-thread group/channel sessions are shared unless
 *     group_sessions_per_user is true (default: true = isolated per user). */
bool is_shared_multi_user_session(const gw_session_source_t *src,
                                          bool group_sessions_per_user,
                                          bool thread_sessions_per_user) {
    if (!src) return false;
    if (strcmp(src->chat_type, "dm") == 0) return false;
    if (src->thread_id[0]) return !thread_sessions_per_user;
    return !group_sessions_per_user;
}

void build_session_key(char *buf, size_t sz,
                               const char *platform, const char *chat_id) {
    snprintf(buf, sz, "%s:%s", platform ? platform : "?", chat_id ? chat_id : "?");
}

int session_find(const char *platform, const char *chat_id) {
    char key[192];
    build_session_key(key, sizeof(key), platform, chat_id);
    for (int i = 0; i < g_gw.session_count; i++) {
        if (strcmp(g_gw.sessions[i].key, key) == 0 && g_gw.sessions[i].in_use)
            return i;
    }
    return -1;
}

int session_create(const char *platform, const char *chat_id) {
    /* M13: Check configurable max concurrent sessions cap */
    if (g_gw.max_concurrent_sessions > 0) {
        int active_count = 0;
        for (int i = 0; i < g_gw.session_count; i++) {
            if (g_gw.sessions[i].in_use) active_count++;
        }
        if (active_count >= g_gw.max_concurrent_sessions) {
            printf("[gateway] Rejecting new session %s:%s: "
                   "max_concurrent_sessions (%d) reached\n",
                   platform ? platform : "?", chat_id ? chat_id : "?",
                   g_gw.max_concurrent_sessions);
            return -1;
        }
    }
    if (g_gw.session_count >= GW_SESSIONS_MAX) {
        /* Evict oldest inactive session */
        int oldest = -1;
        double oldest_time = 1e18;
        for (int i = 0; i < g_gw.session_count; i++) {
            if (g_gw.sessions[i].last_active < oldest_time) {
                oldest_time = g_gw.sessions[i].last_active;
                oldest = i;
            }
        }
        if (oldest < 0) return -1;
        /* Save and free */
        if (g_gw.sessions[oldest].db)
            agent_save_session(&g_gw.sessions[oldest].agent);
        agent_free(&g_gw.sessions[oldest].agent);
        g_gw.sessions[oldest].in_use = false;
    }

    int idx = -1;
    for (int i = 0; i < GW_SESSIONS_MAX; i++) {
        if (!g_gw.sessions[i].in_use) {
            idx = i;
            break;
        }
    }
    if (idx < 0) idx = g_gw.session_count; /* fallback: use next slot */

    gw_session_entry_t *se = &g_gw.sessions[idx];
    memset(se, 0, sizeof(*se));
    build_session_key(se->key, sizeof(se->key), platform, chat_id);
    se->in_use = true;
    se->last_active = gw_mono_time();
    /* Seed the source with identifying fields (platform+chat_id always available).
     * Platform threads fill in the rest via gw_session_set_source(). */
    snprintf(se->source.platform, sizeof(se->source.platform), "%s",
             platform ? platform : "");
    snprintf(se->source.chat_id, sizeof(se->source.chat_id), "%s",
             chat_id ? chat_id : "");
    se->source.has_data = false;  /* not fully populated yet */

    /* Initialize agent */
    init_agent(&se->agent);

    /* Copy config from main agent */
    memcpy(&se->agent.llm, &g_gw.agent.llm, sizeof(se->agent.llm));
    se->agent.max_iterations = g_gw.agent.max_iterations;
    se->agent.compress_enabled = g_gw.agent.compress_enabled;

    /* Open session DB (persistent) */
    if (g_gw.session_db_path[0]) {
        se->db = db_open(g_gw.session_db_path, NULL);
    }

    if (idx >= g_gw.session_count)
        g_gw.session_count = idx + 1;

    return idx;
}

int session_get_or_create(const char *platform, const char *chat_id) {
    int idx = session_find(platform, chat_id);
    if (idx >= 0) {
        double idle = gw_mono_time() - g_gw.sessions[idx].last_active;
        /* Check auto-continue freshness window first (faster check) */
        if (g_gw.auto_continue_freshness_secs > 0.0 &&
            idle > g_gw.auto_continue_freshness_secs) {
            session_free(idx);
            return session_create(platform, chat_id);
        }
        /* Check configurable reset policy (daily/idle/both/none) */
        if (session_should_reset(idle)) {
            session_free(idx);
            return session_create(platform, chat_id);
        }
        g_gw.sessions[idx].last_active = gw_mono_time();
        return idx;
    }
    return session_create(platform, chat_id);
}

void session_save_all(void) {
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use && g_gw.sessions[i].db) {
            agent_save_session(&g_gw.sessions[i].agent);
        }
    }
}

void session_cleanup_idle(void) {
    double now = gw_mono_time();
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use) {
            double idle = now - g_gw.sessions[i].last_active;
            if (session_should_reset(idle))
                session_free(i);
        }
    }
}
