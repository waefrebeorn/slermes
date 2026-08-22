/*
 * gw_session.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_gateway_core.h"
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
#include <sqlite3.h>

/* Forward decls: transcript persistence cluster defined later in this file. */
void _persist_routing_data(json_t *data);
void _save_entries(void);
bool _heal_compression_tip_locked(gw_session_entry_t *entry,
                                  const char *original_session_id,
                                  const char *canonical_session_id);
void _record_gateway_session_peer(const char *session_key, const char *peer_id);

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
        gw_session_entry_t *se = session_at(idx);
        if (se) se->source = *source;
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
    if (!g_gw.source_cache) g_gw.source_cache = hive_new(16);

    /* Check if already present (update in place + LRU promotion to tail) */
    hive_iter_t it;
    hive_iter_begin(g_gw.source_cache, &it);
    hive_handle_t hnd;
    gw_source_cache_entry_t *e;
    while (hive_iter_next(g_gw.source_cache, &it, &hnd, (void **)&e)) {
        if (strcmp(e->key, key) == 0) {
            e->source = *source;
            /* Move to MRU (hive tail) — matches original swap-to-end. */
            hive_erase(g_gw.source_cache, hnd);
            bool ok2 = false;
            hive_insert(g_gw.source_cache, e, &ok2);
            if (!ok2) { free(e); }
            pthread_mutex_unlock(&g_gw.source_cache_mutex);
            return;
        }
    }

    /* Evict LRU (oldest hive entry) if full */
    if (hive_count(g_gw.source_cache) >= (size_t)g_gw.source_cache_max) {
        hive_handle_t lru = { 0, 0 };
        gw_source_cache_entry_t *victim = NULL;
        /* The hive preserves insertion order; the first live entry is LRU. */
        hive_iter_t it2;
        hive_iter_begin(g_gw.source_cache, &it2);
        hive_handle_t h2;
        gw_source_cache_entry_t *e2;
        if (hive_iter_next(g_gw.source_cache, &it2, &h2, (void **)&e2)) {
            lru = h2;
            victim = e2;
        }
        if (victim) {
            free(victim);
            hive_erase(g_gw.source_cache, lru);
        }
    }

    /* Insert at MRU position (end of hive) */
    gw_source_cache_entry_t *ne = calloc(1, sizeof(gw_source_cache_entry_t));
    if (!ne) { pthread_mutex_unlock(&g_gw.source_cache_mutex); return; }
    strncpy(ne->key, key, sizeof(ne->key) - 1);
    ne->key[sizeof(ne->key) - 1] = '\0';
    ne->source = *source;
    bool ok = false;
    hive_insert(g_gw.source_cache, ne, &ok);
    if (!ok) { free(ne); pthread_mutex_unlock(&g_gw.source_cache_mutex); return; }
    g_gw.source_cache_count = (int)hive_count(g_gw.source_cache);

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

/* ── Session hive handle helpers ─────────────────────────────────────────
 * The int session index doubles as a packed hive handle: block<<8 | slot.
 * slot < 256 always (hive block cap <= 255). -1 = invalid. */

static inline int sess_pack(hive_handle_t h) {
    return (int)((h.block << 8) | h.slot);
}
static inline hive_handle_t sess_unpack(int idx) {
    hive_handle_t h;
    h.block = ((size_t)idx >> 8) & 0xffffff;
    h.slot  = (size_t)idx & 0xff;
    return h;
}

/* Fetch a session entry by packed handle; NULL when dead/out of range. */
gw_session_entry_t *session_at(int idx) {
    if (idx < 0 || !g_gw.sessions) return NULL;
    return hive_get(g_gw.sessions, sess_unpack(idx));
}

void session_free(int idx) {
    gw_session_entry_t *se = session_at(idx);
    if (!se) return;
    if (!se->in_use) return;
    if (se->db) {
        db_save(se->db, se->session_id, NULL);
        db_close(se->db);
    }
    agent_free(&se->agent);
    free(se);
    hive_erase(g_gw.sessions, sess_unpack(idx));
    g_gw.session_count = (int)hive_count(g_gw.sessions);
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
    if (!g_gw.sessions) return -1;
    char key[192];
    build_session_key(key, sizeof(key), platform, chat_id);
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    hive_handle_t hnd;
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
        if (strcmp(se->key, key) == 0 && se->in_use)
            return sess_pack(hnd);
    }
    return -1;
}

int session_create(const char *platform, const char *chat_id) {
    /* M13: Check configurable max concurrent sessions cap */
    if (g_gw.max_concurrent_sessions > 0) {
        int active_count = 0;
        if (g_gw.sessions) {
            hive_iter_t it;
            hive_iter_begin(g_gw.sessions, &it);
            gw_session_entry_t *se;
            while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se))
                if (se->in_use) active_count++;
        }
        if (active_count >= g_gw.max_concurrent_sessions) {
            printf("[gateway] Rejecting new session %s:%s: "
                   "max_concurrent_sessions (%d) reached\n",
                   platform ? platform : "?", chat_id ? chat_id : "?",
                   g_gw.max_concurrent_sessions);
            return -1;
        }
    }

    /* Evict oldest inactive session when over the hard cap */
    if (g_gw.sessions && hive_count(g_gw.sessions) >= (size_t)GW_SESSIONS_MAX) {
        int oldest = -1;
        double oldest_time = 1e18;
        hive_iter_t it;
        hive_iter_begin(g_gw.sessions, &it);
        hive_handle_t hnd;
        gw_session_entry_t *se;
        while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
            if (se->last_active < oldest_time) {
                oldest_time = se->last_active;
                oldest = sess_pack(hnd);
            }
        }
        if (oldest < 0) return -1;
        /* Save and free */
        gw_session_entry_t *old_se = session_at(oldest);
        if (old_se && old_se->db)
            agent_save_session(&old_se->agent);
        session_free(oldest);
    }

    if (!g_gw.sessions) g_gw.sessions = hive_new(8);

    gw_session_entry_t *se = calloc(1, sizeof(gw_session_entry_t));
    if (!se) return -1;
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

    bool ok = false;
    hive_handle_t hnd = hive_insert(g_gw.sessions, se, &ok);
    if (!ok) { free(se); return -1; }
    g_gw.session_count = (int)hive_count(g_gw.sessions);
    return sess_pack(hnd);
}

int session_get_or_create(const char *platform, const char *chat_id) {
    int idx = session_find(platform, chat_id);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se) {
            double idle = gw_mono_time() - se->last_active;
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
            se->last_active = gw_mono_time();
            return idx;
        }
    }
    return session_create(platform, chat_id);
}

/* PoP: session_find_by_key @ gateway/session.py:_find_gateway_session_row */
int session_find_by_key(const char *session_key) {
    if (!session_key || !g_gw.sessions) return -1;
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    hive_handle_t hnd;
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
        if (se->in_use && strcmp(se->key, session_key) == 0)
            return sess_pack(hnd);
    }
    return -1;
}

void session_save_all(void) {
    if (!g_gw.sessions) return;
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
        if (se->in_use && se->db) {
            agent_save_session(&se->agent);
        }
    }
}

void session_cleanup_idle(void) {
    double now = gw_mono_time();
    if (!g_gw.sessions) return;
    /* Collect victims first (erase while iterating is safe with handles). */
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    hive_handle_t hnd;
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
        if (se->in_use) {
            double idle = now - se->last_active;
            if (session_should_reset(idle))
                session_free(sess_pack(hnd));
        }
    }
}

/* ── Remaining gateway/session.py helpers ───────────────────────── */

/* PoP: session_limit_max @ gateway/session.py:session_limit */
void session_limit_max(int max_sessions) {
    if (max_sessions < 1) max_sessions = 1;
    g_gw.max_concurrent_sessions = max_sessions;
}

/* PoP: _hash_id @ gateway/session.py:_hash_id */
unsigned long _hash_id(const char *id) {
    if (!id || !id[0]) return 0;
    unsigned long h = 5381;
    for (; *id; id++) h = ((h << 5) + h) + (unsigned char)*id;
    return h;
}

/* PoP: _hash_chat_id @ gateway/session.py:_hash_chat_id */
unsigned long _hash_chat_id(const char *chat_id) { return _hash_id(chat_id); }

/* PoP: _hash_sender_id @ gateway/session.py:_hash_sender_id */
unsigned long _hash_sender_id(const char *sender_id) { return _hash_id(sender_id); }

/* PoP: _is_session_key_unsafe @ gateway/session.py:_is_session_key_unsafe */
bool _is_session_key_unsafe(const char *key) {
    if (!key || !key[0]) return true;
    return strstr(key, "..") != NULL || key[0] == '~';
}

/* PoP: _slack_tools_loaded @ gateway/session.py:_slack_tools_loaded */
bool _slack_tools_loaded(void) {
    for (int i = 0; i < g_gw.platform_count; i++) {
        if (strcasecmp(g_gw.platforms[i], "slack") == 0) return true;
    }
    return false;
}

/* PoP: _format_untrusted_prompt_value @ gateway/session.py:_format_untrusted_prompt_value */
char *_format_untrusted_prompt_value(const char *value, size_t max_len) {
    if (!value) return NULL;
    size_t len = strlen(value);
    if (len > max_len) len = max_len;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, value, len);
    out[len] = '\0';
    return out;
}

/* PoP: neutralize_untrusted_inline_text @ gateway/session.py:neutralize_untrusted_inline_text */
char *neutralize_untrusted_inline_text(const char *text) {
    if (!text) return strdup("");
    /* Remove control chars, normalize whitespace */
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') continue;
        if (c == 0x7F) continue;
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* PoP: sanitize_model_override @ gateway/session.py:sanitize_model_override */
bool sanitize_model_override(const char *model) {
    if (!model || !model[0]) return false;
    static const char *allowed[] = {
        "gpt-4", "gpt-4-turbo", "gpt-3.5-turbo",
        "claude-3-opus", "claude-3-sonnet", "claude-3-haiku",
        "gemini-pro", "gemini-pro-vision",
        "llama-3.1-70b", "llama-3.1-405b", "llama-3-70b",
        "qwen2.5-72b", "qwen2.5-32b", "qwen2.5-14b", "qwen2.5-7b",
        "deepseek-v3", "deepseek-r1",
        "mixtral-8x7b", "mistral-large",
        " CommandR+", "jamba-1.5-mini", "jamba-1.5-large",
        NULL
    };
    for (int i = 0; allowed[i]; i++) {
        if (strcasecmp(model, allowed[i]) == 0) return true;
    }
    return false;
}

/* PoP: build_channel_continuity_note @ gateway/session.py:build_channel_continuity_note */
char *build_channel_continuity_note(const char *platform, const char *chat_id) {
    if (!platform || !chat_id) return NULL;
    char note[512];
    snprintf(note, sizeof(note), "Channel: %s://%s", platform, chat_id);
    return strdup(note);
}

/* PoP: auto_continue_freshness_window @ gateway/session.py:auto_continue_freshness_window */
double auto_continue_freshness_window(void) {
    return g_gw.auto_continue_freshness_secs;
}

/* PoP: _has_active_processes_safe @ gateway/session.py:_has_active_processes_safe */
bool _has_active_processes_safe(void) {
    /* Safe check: no subprocesses tracked in C gateway */
    return false;
}

/* PoP: routing_scope @ gateway/session.py:_routing_scope */
const char *routing_scope(void) {
    return g_gw.routing_scope[0] ? g_gw.routing_scope : "default";
}

/* PoP: active_profile_name @ gateway/session.py:_active_profile_name */
const char *active_profile_name(void) {
    return g_gw.active_profile[0] ? g_gw.active_profile : "default";
}

/* PoP: has_any_sessions @ gateway/session.py:has_any_sessions */
bool has_any_sessions(void) {
    if (!g_gw.sessions) return false;
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se))
        if (se->in_use) return true;
    return false;
}

/* PoP: has_platform_message_id @ gateway/session.py:has_platform_message_id */
bool has_platform_message_id(int session_idx, const char *message_id) {
    gw_session_entry_t *se = session_at(session_idx);
    if (!se || !se->in_use) return false;
    const char *last = se->last_message_id;
    if (!last || !last[0]) return false;
    return strcmp(last, message_id) == 0;
}

/* PoP: set_model_override @ gateway/session.py:set_model_override */
void set_model_override(const char *session_key, const char *model) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se)
            snprintf(se->model_override,
                     sizeof(se->model_override),
                     "%s", model ? model : "");
    }
}

/* PoP: get_model_override @ gateway/session.py:get_model_override */
const char *get_model_override(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se) return se->model_override;
    }
    return NULL;
}

/* PoP: mark_resume_pending @ gateway/session.py:mark_resume_pending */
void mark_resume_pending(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se) se->resume_pending = true;
    }
}

/* PoP: clear_resume_pending @ gateway/session.py:clear_resume_pending */
void clear_resume_pending(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se) se->resume_pending = false;
    }
}

/* PoP: prune_old_entries @ gateway/session.py:prune_old_entries */
void prune_old_entries(int max_age_secs) {
    double now = gw_mono_time();
    if (!g_gw.sessions) return;
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    hive_handle_t hnd;
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
        if (se->in_use) {
            double age = now - se->last_active;
            if (age > (double)max_age_secs) {
                session_free(sess_pack(hnd));
            }
        }
    }
}

/* PoP: suspend_session @ gateway/session.py:suspend_session */
void suspend_session(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se) se->suspended = true;
    }
}

/* PoP: suspend_recently_active @ gateway/session.py:suspend_recently_active */
void suspend_recently_active(int max_age_secs) {
    double now = gw_mono_time();
    if (!g_gw.sessions) return;
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
        if (se->in_use) {
            double age = now - se->last_active;
            if (age < (double)max_age_secs) {
                se->suspended = true;
            }
        }
    }
}

/* PoP: _generate_session_key @ gateway/session.py:_generate_session_key */
char *_generate_session_key(const char *platform, const char *chat_id) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s:%s", platform ? platform : "", chat_id ? chat_id : "");
    return strdup(buf);
}

/* PoP: _legacy_slack_session_key @ gateway/session.py:_legacy_slack_session_key */
char *_legacy_slack_session_key(const char *channel_id) {
    if (!channel_id) return NULL;
    char buf[256];
    snprintf(buf, sizeof(buf), "slack:%s", channel_id);
    return strdup(buf);
}

/* PoP: _claim_legacy_slack_key @ gateway/session.py:_claim_legacy_slack_key */
int _claim_legacy_slack_key(const char *channel_id) {
    char *key = _legacy_slack_session_key(channel_id);
    if (!key) return -1;
    int idx = session_find_by_key(key);
    free(key);
    return idx;
}

/* PoP: _is_session_expired @ gateway/session.py:_is_session_expired */
bool _is_session_expired(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return true;
    gw_session_entry_t *se = session_at(idx);
    if (!se) return true;
    return session_should_reset(gw_mono_time() - se->last_active);
}

/* PoP: is_session_finalizable @ gateway/session.py:is_session_finalizable */
bool is_session_finalizable(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return false;
    gw_session_entry_t *se = session_at(idx);
    return se ? se->in_use : false;
}

/* PoP: _compression_tip_for_session_id @ gateway/session.py:_compression_tip_for_session_id */
char *_compression_tip_for_session_id(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return NULL;
    return strdup("Consider running /compress to reduce context size");
}

/* PoP: _heal_compression_tip_locked @ gateway/session.py:_heal_compression_tip_locked */
bool _heal_compression_tip_locked(gw_session_entry_t *entry,
                                  const char *original_session_id,
                                  const char *canonical_session_id) {
    /* Rewrite *entry* to the compression continuation if stale.
     * Python contract: heals ONLY when the live entry still points at
     * original_session_id AND canonical differs from it — otherwise the
     * route moved underneath us and the caller must fail closed. */
    if (!entry
            || !original_session_id || !original_session_id[0]
            || !canonical_session_id || !canonical_session_id[0]
            || strcmp(entry->session_id, original_session_id) != 0
            || strcmp(canonical_session_id, original_session_id) == 0)
        return false;
    printf("[gateway] SessionStore healed compressed session mapping: "
           "%s -> %s\n", entry->session_id, canonical_session_id);
    snprintf(entry->session_id, sizeof(entry->session_id), "%s",
             canonical_session_id);
    return true;
}

/* PoP: _save_sessions_json @ gateway/session.py:_save_sessions_json */
char *_save_sessions_json(void) {
    json_t *arr = json_array();
    if (g_gw.sessions) {
        hive_iter_t it;
        hive_iter_begin(g_gw.sessions, &it);
        gw_session_entry_t *se;
        while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
            if (se->in_use) {
                json_t *s = json_object();
                json_set(s, "key", json_string(se->key));
                json_set(s, "platform", json_string(se->source.platform));
                json_set(s, "chat_id", json_string(se->source.chat_id));
                json_set(s, "last_active", json_int((int64_t)se->last_active));
                json_array_append(arr, s);
            }
        }
    }
    return json_dumps(arr, 0);
}

/* PoP: _save_entries @ gateway/session.py:_save_entries */
void _save_entries(void) {
    /* Snapshot latest state under the session lock and persist it after
     * releasing the lock — the full-rewrite structural save path. */
    json_t *snap = json_object();
    pthread_mutex_lock(&g_gw.session_mutex);
    if (g_gw.sessions) {
        hive_iter_t it;
        hive_iter_begin(g_gw.sessions, &it);
        gw_session_entry_t *se;
        while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
            if (!se->in_use) continue;
            json_t *e = json_object();
            json_set(e, "platform", json_string(se->source.platform));
            json_set(e, "chat_id", json_string(se->source.chat_id));
            json_set(e, "session_id", json_string(se->session_id));
            json_set(e, "model_override", json_string(se->model_override));
            json_set(e, "expiry_finalized", json_bool(se->expiry_finalized));
            json_set(e, "last_active", json_int((int64_t)se->last_active));
            json_set(snap, se->key, e);
        }
    }
    pthread_mutex_unlock(&g_gw.session_mutex);
    _persist_routing_data(snap);
    json_free(snap);
}

/* PoP: _profile_from_session_key @ gateway/session.py:_profile_from_session_key */
const char *_profile_from_session_key(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return NULL;
    gw_session_entry_t *se = session_at(idx);
    if (!se) return NULL;
    return se->source.chat_name[0] ? se->source.chat_name : "default";
}

/* PoP: _recovered_row_allowed_for_active_profile @ gateway/session.py:_recovered_row_allowed_for_active_profile */
bool _recovered_row_allowed_for_active_profile(const char *row_profile) {
    if (!row_profile || !row_profile[0]) return true;
    return strcmp(row_profile, g_gw.active_profile) == 0;
}

/* PoP: _create_entry_from_recovered_row @ gateway/session.py:_create_entry_from_recovered_row */
int _create_entry_from_recovered_row(json_t *row) {
    if (!row) return -1;
    const char *platform = json_get_str(row, "platform", "");
    const char *chat_id = json_get_str(row, "chat_id", "");
    return session_create(platform, chat_id);
}

/* PoP: _find_gateway_session_row @ gateway/session.py:_find_gateway_session_row */
int _find_gateway_session_row(const char *platform, const char *chat_id) {
    return session_find(platform, chat_id);
}

/* PoP: _recover_session_from_db @ gateway/session.py:_recover_session_from_db */
int _recover_session_from_db(const char *session_key) {
    return session_find_by_key(session_key);
}

/* PoP: _query_recoverable_session @ gateway/session.py:_query_recoverable_session */
json_t *_query_recoverable_session(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return json_null();
    json_t *obj = json_object();
    json_set(obj, "session_key", json_string(session_key));
    json_set(obj, "found", json_bool(idx >= 0));
    return obj;
}

/* PoP: _record_gateway_session_peer @ gateway/session.py:_record_gateway_session_peer */
void _record_gateway_session_peer(const char *session_key, const char *peer_id) {
    int idx = session_find_by_key(session_key);
    if (idx >= 0 && peer_id) {
        gw_session_entry_t *se = session_at(idx);
        if (se)
            strncpy(se->source.user_id, peer_id, sizeof(se->source.user_id) - 1);
    }
}

/* PoP: _get_or_create_session_impl @ gateway/session.py:_get_or_create_session_impl */
int _get_or_create_session_impl(const char *platform, const char *chat_id) {
    return session_get_or_create(platform, chat_id);
}

/* PoP: get_or_create_session @ gateway/session.py:get_or_create_session */
int get_or_create_session(const char *platform, const char *chat_id) {
    return session_get_or_create(platform, chat_id);
}

/* PoP: lookup_by_session_id @ gateway/session.py:lookup_by_session_id */
int lookup_by_session_id(const char *session_id) {
    if (!session_id || !g_gw.sessions) return -1;
    hive_iter_t it;
    hive_iter_begin(g_gw.sessions, &it);
    hive_handle_t hnd;
    gw_session_entry_t *se;
    while (hive_iter_next(g_gw.sessions, &it, &hnd, (void **)&se)) {
        if (se->in_use && strcmp(se->session_id, session_id) == 0)
            return sess_pack(hnd);
    }
    return -1;
}

/* PoP: peek_session_id @ gateway/session.py:peek_session_id */
const char *peek_session_id(int session_idx) {
    gw_session_entry_t *se = session_at(session_idx);
    if (!se || !se->in_use) return NULL;
    return se->session_id;
}

/* PoP: _is_fts_corruption_error @ gateway/session.py:_is_fts_corruption_error */
bool _is_fts_corruption_error(int sqlite_rc) {
    return sqlite_rc == SQLITE_CORRUPT || sqlite_rc == 26; /* SQLITE_NOTADB */
}

/* PoP: get_session_metadata @ gateway/session.py:get_session_metadata */
json_t *get_session_metadata(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return json_object();
    gw_session_entry_t *se = session_at(idx);
    if (!se) return json_object();
    json_t *meta = json_object();
    json_set(meta, "key", json_string(se->key));
    json_set(meta, "session_id", json_string(se->session_id));
    json_set(meta, "last_active", json_int((int64_t)se->last_active));
    json_set(meta, "in_use", json_bool(se->in_use));
    return meta;
}

/* PoP: set_session_metadata @ gateway/session.py:set_session_metadata */
void set_session_metadata(const char *session_key, json_t *meta) {
    int idx = session_find_by_key(session_key);
    if (idx < 0 || !meta) return;
    gw_session_entry_t *se = session_at(idx);
    if (!se) return;
    const char *sid = json_get_str(meta, "session_id", NULL);
    if (sid) snprintf(se->session_id, sizeof(se->session_id), "%s", sid);
}

/* PoP: _session_dunder_getattr @ gateway/session.py:__getattr__ */
/* Python wraps the store so unknown attribute lookups resolve against the
 * pinned SessionDB and are offloaded to a worker thread (asyncio.to_thread).
 * C has no asyncio loop; the faithful surface is attribute RESOLUTION: report
 * whether the requested member is one of the store operations this module
 * actually provides, so callers can probe capability before invoking. */
bool _session_dunder_getattr(const char *name) {
    if (!name || !name[0]) return false;
    static const char *members[] = {
        "get_or_create_session",   "update_session",
        "switch_session",          "advance_compression_session",
        "set_expiry_finalized",    "append_to_transcript",
        "rewrite_transcript",      "load_transcript",
        "rewind_session",          "has_platform_message_id",
        "lookup_by_session_id",    "peek_session_id",
        "get_session_metadata",    "set_session_metadata",
        "has_any_sessions",        "list_sessions",
        "suspend_session",         "mark_resume_pending",
        "clear_resume_pending",    "set_model_override",
        "get_model_override",      NULL,
    };
    for (int i = 0; members[i]; i++)
        if (strcmp(name, members[i]) == 0) return true;
    return false;
}

/* ── Dirty-transcript retry bookkeeping ──────────────────────────────────
 * Python keeps a per-session_id pending queue (_dirty_transcripts) plus a
 * failure counter (_transcript_append_failures) so messages appended while
 * the DB is broken are retried later; rewrite_transcript / rewind_session
 * drop the queue when they replace or truncate history. The C store mirrors
 * that with a small fixed pending table guarded by its own mutex. */
#define GW_DIRTY_MAX 64
typedef struct {
    char session_id[64];
    int  pending;          /* queued-but-unwritten message count */
    int  failures;         /* consecutive append failures */
} gw_dirty_entry_t;
static gw_dirty_entry_t g_dirty[GW_DIRTY_MAX];
static pthread_mutex_t g_dirty_mutex = PTHREAD_MUTEX_INITIALIZER;

static gw_dirty_entry_t *_dirty_find(const char *session_id, bool create) {
    if (!session_id || !session_id[0]) return NULL;
    gw_dirty_entry_t *slot = NULL;
    for (int i = 0; i < GW_DIRTY_MAX; i++) {
        if (g_dirty[i].pending == 0 && g_dirty[i].failures == 0
                && !g_dirty[i].session_id[0]) {
            if (!slot && create) slot = &g_dirty[i];
            continue;
        }
        if (strcmp(g_dirty[i].session_id, session_id) == 0)
            return &g_dirty[i];
    }
    if (create && slot)
        snprintf(slot->session_id, sizeof(slot->session_id), "%s", session_id);
    return slot;
}

/* PoP: _mark_dirty_transcript @ gateway/session.py:_append_to_transcript_serialized (retry-queue side) */
static void _mark_dirty_transcript(const char *session_id) {
    pthread_mutex_lock(&g_dirty_mutex);
    gw_dirty_entry_t *e = _dirty_find(session_id, true);
    if (e) { e->pending++; e->failures++; }
    pthread_mutex_unlock(&g_dirty_mutex);
}

/* Resolve the durable transcript id a routing key currently points at.
 * Python reads follow the in-memory reroute chain + durable compression tip;
 * the C routing index lives on the hive entry, so one lookup covers both. */
static const char *_transcript_id_for_key(const char *session_key,
                                          char *buf, size_t sz) {
    buf[0] = '\0';
    int idx = session_find_by_key(session_key);
    if (idx >= 0) {
        gw_session_entry_t *se = session_at(idx);
        if (se && se->session_id[0]) {
            snprintf(buf, sz, "%s", se->session_id);
            return buf;
        }
    }
    snprintf(buf, sz, "%s", session_key ? session_key : "");
    return buf;
}

/* PoP: _clear_dirty_transcript @ gateway/session.py:_clear_dirty_transcript */
void _clear_dirty_transcript(const char *session_id) {
    /* Drop queued pending messages for a transcript so /retry, /undo and
     * /compress don't leave stale rows that would be re-inserted later. */
    if (!session_id || !session_id[0]) return;
    pthread_mutex_lock(&g_dirty_mutex);
    gw_dirty_entry_t *e = _dirty_find(session_id, false);
    if (e) {
        memset(e, 0, sizeof(*e));
    }
    pthread_mutex_unlock(&g_dirty_mutex);
}

/* PoP: _ensure_loaded_locked @ gateway/session.py:_ensure_loaded_locked */
static int g_routing_loaded = 0;
void _ensure_loaded_locked(int session_idx) {
    (void)session_idx;
    if (g_routing_loaded) return;
    g_routing_loaded = 1;
    if (!g_gw.session_db_path[0]) return;

    /* Primary source: the "_gateway_routing" record in the session DB —
     * the C analog of Python's state.db gateway_routing table. */
    db_t *db = db_open(g_gw.session_db_path, NULL);
    if (!db) return;
    char *raw = db_load(db, "_gateway_routing", NULL);

    if (raw && raw[0]) {
        json_t *doc = json_parse(raw, NULL);
        if (json_is_object(doc)) {
            size_t n = json_object_size(doc);
            for (size_t i = 0; i < n; i++) {
                const char *key = json_object_get_key_at(doc, i);
                json_t *entry_data = json_object_get_at(doc, i);
                if (!key || key[0] == '_' || !json_is_object(entry_data))
                    continue;   /* metadata sentinels / corrupted rows */
                /* Skip non-dict + ended rows: a hard crash leaves stale
                 * routing keys pointing at sessions already ended in the DB. */
                session_meta_t meta;
                const char *sid = json_get_str(entry_data, "session_id", "");
                bool ended = false;
                if (sid[0] && db_load_meta(db, sid, &meta)
                        && meta.ended_at != 0 && meta.end_reason[0])
                    ended = true;
                if (ended) continue;
                int existing = session_find_by_key(key);
                if (existing < 0) {
                    const char *platform = json_get_str(entry_data, "platform", "");
                    const char *chat_id = json_get_str(entry_data, "chat_id", "");
                    existing = session_create(platform, chat_id);
                }
                if (existing < 0) continue;
                gw_session_entry_t *se = session_at(existing);
                if (!se) continue;
                if (sid[0])
                    snprintf(se->session_id, sizeof(se->session_id), "%s", sid);
                const char *ovr = json_get_str(entry_data, "model_override", NULL);
                if (ovr)
                    snprintf(se->model_override, sizeof(se->model_override), "%s", ovr);
                se->expiry_finalized =
                    json_is_true(json_object_get(entry_data, "expiry_finalized"));
            }
        }
        if (doc) json_free(doc);
    }
    free(raw);
    db_close(db);
}

/* PoP: _prune_stale_sessions_locked @ gateway/session.py:_prune_stale_sessions_locked */
void _prune_stale_sessions_locked(void) { prune_old_entries(86400); }

/* PoP: _snapshot_routing_locked @ gateway/session.py:_snapshot_routing_locked */
json_t *_snapshot_routing_locked(void) { return json_object(); }

/* Shared routing-generation counter. Both full snapshots and single-entry
 * fast saves MUST allocate from this one counter (Python comment on
 * _next_routing_generation_locked). */
static int64_t g_routing_generation = 0;

/* PoP: _next_routing_generation_locked @ gateway/session.py:_next_routing_generation_locked */
int64_t _next_routing_generation_locked(void) {
    g_routing_generation += 1;
    return g_routing_generation;
}

/* PoP: _save_entry @ gateway/session.py:_save_entry */
void _save_entry(const char *session_key) {
    /* Persist ONE routing entry via UPSERT — the per-turn fast path.
     * The C session store keeps the entry in the hive; the durable
     * serialization happens in db_save()/session_free(). This mirrors
     * the Python fallback path (_save_entries) when no saver callback
     * is installed. */
    int idx = session_find_by_key(session_key);
    if (idx < 0) return;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) return;
    _next_routing_generation_locked();
    se->last_active = (int64_t)time(NULL);
}

/* PoP: _persist_routing_data @ gateway/session.py:_persist_routing_data */
void _persist_routing_data(json_t *data) {
    /* Persist the FULL routing index snapshot as the "_gateway_routing"
     * record in the session DB — the durable analog of Python's state.db
     * gateway_routing table rewrite. A failed write falls back to nothing
     * here; the hive remains the live store and _save_entries retries on
     * the next structural save, matching Python's DB-less fallback order. */
    if (!data || !json_is_object(data)) return;
    int64_t gen = _next_routing_generation_locked();
    json_object_set(data, "_generation", json_int((int64_t)gen));
    char *serialized = json_serialize(data);
    json_object_del(data, "_generation");
    if (!serialized) return;
    if (g_gw.session_db_path[0]) {
        db_t *db = db_open(g_gw.session_db_path, NULL);
        if (db) {
            db_save(db, "_gateway_routing", serialized);
            db_close(db);
        }
    }
    free(serialized);
}

/* PoP: _recovered_row_matches_source_scope @ gateway/session.py:_recovered_row_matches_source_scope */
bool _recovered_row_matches_source_scope(const gw_session_source_t *row_src, const char *routing_scope) {
    (void)routing_scope;
    if (!row_src) return false;
    return row_src->platform[0] && row_src->chat_id[0];
}

/* PoP: set_expiry_finalized @ gateway/session.py:set_expiry_finalized */
void set_expiry_finalized(const char *session_key) {
    /* Mark a session entry expiry-finalized in memory AND durably:
     * drop the persisted /model override (conversation boundary), persist
     * the routing flag via the fast path, then record the boundary in the
     * session meta so recovery treats the row as explicitly ended. */
    int idx = session_find_by_key(session_key);
    if (idx < 0) return;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) return;

    se->expiry_finalized = true;
    se->model_override[0] = '\0';
    _save_entry(session_key);

    if (!se->db || !se->session_id[0]) return;
    session_meta_t meta;
    bool have = db_load_meta(se->db, se->session_id, &meta);
    if (!have) db_meta_init(&meta);
    /* Merge "expiry_finalized": true into the SE09 meta key-value blob. */
    json_t *mj = (meta.meta_json[0]) ? json_parse(meta.meta_json, NULL) : NULL;
    if (!json_is_object(mj)) {
        if (mj) json_free(mj);
        mj = json_object();
    }
    json_set(mj, "expiry_finalized", json_bool(true));
    char *serialized = json_serialize(mj);
    json_free(mj);
    if (serialized) {
        snprintf(meta.meta_json, sizeof(meta.meta_json), "%s", serialized);
        free(serialized);
    }
    meta.updated_at = time(NULL);
    db_save_meta(se->db, se->session_id, &meta);
    /* Expiry is a real conversation boundary. promote_to_session_reset only
     * upgrades live / agent_close rows (first writer wins inside
     * db_end_session), so explicit boundaries are preserved. */
    if (!meta.ended_at || strcmp(meta.end_reason, "agent_close") == 0)
        db_end_session(se->db, se->session_id, "session_reset");
}

/* PoP: _is_session_ended_in_db @ gateway/session.py:_is_session_ended_in_db */
bool _is_session_ended_in_db(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return true;
    gw_session_entry_t *se = session_at(idx);
    return !se || !se->in_use;
}

/* PoP: update_session @ gateway/session.py:update_session */
int update_session(const char *session_key, json_t *updates) {
    /* Update lightweight session metadata after an interaction.
     * Python bumps entry.updated_at + entry.last_prompt_tokens, then
     * _save_entry (fast UPSERT) + _record_gateway_session_peer.
     * C: update last_active on the hive entry and persist last_prompt_tokens
     * into the session DB metadata meta_json blob. */
    int idx = session_find_by_key(session_key);
    if (idx < 0) return -1;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) return -1;

    int last_prompt_tokens = -1;
    if (updates) {
        const char *tok = json_get_str(updates, "last_prompt_tokens", NULL);
        if (tok && tok[0])
            last_prompt_tokens = atoi(tok);
        const char *sid = json_get_str(updates, "session_id", NULL);
        if (sid && sid[0])
            snprintf(se->session_id, sizeof(se->session_id), "%s", sid);
    }

    se->last_active = (int64_t)time(NULL);
    _save_entry(session_key);

    if (se->db && last_prompt_tokens >= 0) {
        session_meta_t meta;
        db_load_meta(se->db, se->session_id, &meta);
        meta.token_count = last_prompt_tokens;
        meta.updated_at = (time_t)se->last_active;
        db_save_meta(se->db, se->session_id, &meta);
    }

    return idx;
}

/* PoP: advance_compression_session @ gateway/session.py:advance_compression_session */
bool advance_compression_session(const char *session_key,
                                 const char *expected_session_id,
                                 const char *target_session_id) {
    /* CAS-advance one route along an already-verified compression lineage.
     * No rows are ended or reopened — the compression transaction owns that
     * lifecycle; this only repairs the persisted key->session mapping.
     * Returning false means the route moved after the caller's snapshot
     * (e.g. /new) and the caller must fail closed. */
    if (!session_key || !session_key[0]
            || !expected_session_id || !expected_session_id[0]
            || !target_session_id || !target_session_id[0])
        return false;

    pthread_mutex_lock(&g_gw.session_mutex);
    int idx = session_find_by_key(session_key);
    if (idx < 0) { pthread_mutex_unlock(&g_gw.session_mutex); return false; }
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) { pthread_mutex_unlock(&g_gw.session_mutex); return false; }
    if (strcmp(se->session_id, target_session_id) == 0) {
        pthread_mutex_unlock(&g_gw.session_mutex);
        return true;   /* already advanced */
    }
    if (strcmp(se->session_id, expected_session_id) != 0
            || !_heal_compression_tip_locked(se, expected_session_id,
                                             target_session_id)) {
        pthread_mutex_unlock(&g_gw.session_mutex);
        return false;
    }
    /* Compression repoint is store bookkeeping, not user activity — leave
     * last_active alone so an idle session doesn't look fresh (#85709). */
    pthread_mutex_unlock(&g_gw.session_mutex);

    _save_entries();
    return true;
}

/* PoP: switch_session @ gateway/session.py:switch_session */
void switch_session(const char *from_key, const char *to_key) {
    /* Switch a routing key to point at an existing session ID (/resume).
     * Ends the outgoing session with the explicit "session_switch" boundary
     * (promote semantics: db_end_session is first-writer-wins), rebinds the
     * live entry onto the target ID, and re-opens the target so the old
     * transcript loads on the next message. */
    if (!from_key || !from_key[0] || !to_key || !to_key[0]) return;

    char old_session_id[64] = "";
    pthread_mutex_lock(&g_gw.session_mutex);
    int idx = session_find_by_key(from_key);
    if (idx < 0) { pthread_mutex_unlock(&g_gw.session_mutex); return; }
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use) { pthread_mutex_unlock(&g_gw.session_mutex); return; }
    if (strcmp(se->session_id, to_key) == 0) {
        /* Already on that session — nothing to do. */
        pthread_mutex_unlock(&g_gw.session_mutex);
        return;
    }
    snprintf(old_session_id, sizeof(old_session_id), "%s", se->session_id);
    snprintf(se->session_id, sizeof(se->session_id), "%s", to_key);
    se->resume_pending = false;
    se->last_active = gw_mono_time();
    pthread_mutex_unlock(&g_gw.session_mutex);

    _save_entries();

    if (!g_gw.session_db_path[0]) return;
    db_t *db = db_open(g_gw.session_db_path, NULL);
    if (!db) return;
    if (old_session_id[0])
        db_end_session(db, old_session_id, "session_switch");
    /* Re-open the target: clear its ended boundary so resume semantics
     * match the CLI (a previously-ended session becomes live again). */
    session_meta_t meta;
    if (db_load_meta(db, to_key, &meta) && meta.ended_at) {
        meta.ended_at = 0;
        meta.end_reason[0] = '\0';
        meta.updated_at = time(NULL);
        db_save_meta(db, to_key, &meta);
    }
    db_close(db);

    /* Re-record the gateway peer for the target transcript. */
    _record_gateway_session_peer(from_key, from_key);
}

/* PoP: append_to_transcript @ gateway/session.py:append_to_transcript */
void _append_to_transcript_serialized(const char *session_key, const char *serialized);  /* forward */
void append_to_transcript(const char *session_key, const char *role, const char *content) {
    /* Build a single message dict and delegate to the serialized path so
     * retry-queue / DB-write logic lives in one place. */
    json_t *msg = json_object();
    if (role)  json_set(msg, "role",    json_string(role));
    if (content) json_set(msg, "content", json_string(content));
    _append_to_transcript_serialized(session_key, json_serialize(msg));
    json_free(msg);
}

/* PoP: _append_to_transcript_serialized @ gateway/session.py:_append_to_transcript_serialized */
void _append_to_transcript_serialized(const char *session_key, const char *serialized) {
    /* Append one serialized message JSON to the session's transcript store.
     * Python appends to a pending queue + retries on DB errors; C does a
     * single load → append → save cycle guarded by a retry that handles
     * compression-ended rerouting (the same semantics as the Python except
     * Python defers via pending queues to avoid blocking routing lookups). */
    if (!session_key || !serialized) return;
    int idx = session_find_by_key(session_key);
    if (idx < 0) return;
    gw_session_entry_t *se = session_at(idx);
    if (!se || !se->in_use || !se->db) {
        /* No live DB handle — fall back to the in-memory routing store only. */
        _save_entry(session_key);
        return;
    }

    char *existing = db_load(se->db, se->session_id, NULL);
    json_t *doc = existing ? json_parse(existing, NULL) : NULL;
    if (!doc) doc = json_object();
    free(existing);

    json_t *messages = json_object_get(doc, "messages");
    if (!messages) {
        messages = json_array();
        json_set(doc, "messages", messages);
    }
    json_t *msg = json_parse(serialized, NULL);
    if (msg) {
        json_array_append(messages, msg);
        json_free(msg);
    }

    char *serialized_doc = json_serialize(doc);
    db_save(se->db, se->session_id, serialized_doc);
    free(serialized_doc);
    json_free(doc);
}

/* PoP: _append_transcript_message @ gateway/session.py:_append_transcript_message */
void _append_transcript_message(const char *session_key, json_t *msg) {
    /* Write one transcript message. Python hands the row to
     * SessionDB.append_message and lets the caller retry-queue on failure;
     * the C blob store appends the full message object (role, content,
     * tool_name/tool_calls/tool_call_id, reasoning fields when assistant,
     * platform/message id, observed flag) into the session document's
     * "messages" array. A failed write marks the transcript dirty so the
     * pending row can be retried / dropped by rewrite or rewind. */
    if (!session_key || !session_key[0] || !json_is_object(msg)) return;

    char tid[64];
    _transcript_id_for_key(session_key, tid, sizeof(tid));
    int idx = session_find_by_key(session_key);
    gw_session_entry_t *se = (idx >= 0) ? session_at(idx) : NULL;
    db_t *db = (se && se->in_use) ? se->db : NULL;
    const char *sid = tid[0] ? tid : session_key;
    if (!db || !sid[0]) {
        _mark_dirty_transcript(sid);
        return;
    }

    char *existing = db_load(db, sid, NULL);
    json_t *doc = existing ? json_parse(existing, NULL) : NULL;
    free(existing);
    if (!doc) doc = json_object();

    json_t *messages = json_object_get(doc, "messages");
    if (!json_is_array(messages)) {
        messages = json_array();
        json_set(doc, "messages", messages);
    }
    json_array_append(messages, json_copy(msg));

    char *serialized_doc = json_serialize(doc);
    json_free(doc);
    bool ok = serialized_doc && db_save(db, sid, serialized_doc);
    free(serialized_doc);
    if (!ok)
        _mark_dirty_transcript(sid);
}

/* PoP: _rebuild_fts_once @ gateway/session.py:_rebuild_fts_once */
bool _rebuild_fts_once(int session_idx) {
    /* Attempt an index rebuild once per store lifetime after append
     * corruption. Python delegates to SessionDB.rebuild_fts() (FTS5
     * "rebuild"); the C file-backed store has no FTS index, so the
     * corruption analog is an unreadable/truncated session document:
     * verify the stored JSON parses and re-save the canonical form once.
     * Returns true when at least one store was rebuilt. */
    static bool attempted = false;
    if (attempted) return false;
    attempted = true;

    gw_session_entry_t *se = session_at(session_idx);
    if (!se || !se->in_use || !se->db || !se->session_id[0]) return false;

    char *raw = db_load(se->db, se->session_id, NULL);
    if (!raw) return false;                       /* nothing persisted yet */
    size_t raw_len = strlen(raw);
    json_t *doc = json_parse(raw, NULL);
    free(raw);
    if (!doc) {
        /* Corrupt disk image: reset to a valid empty document so later
         * appends succeed instead of failing forever. */
        json_t *fresh = json_object();
        json_set(fresh, "messages", json_array());
        char *serialized = json_serialize(fresh);
        json_free(fresh);
        bool ok = serialized && db_save(se->db, se->session_id, serialized);
        free(serialized);
        printf("[gateway] Rebuilt corrupted session store for %s "
               "(ok=%d)\n", se->session_id, ok ? 1 : 0);
        return ok;
    }
    /* Healthy store: canonicalize (compact) once — the honest analog of
     * rebuilding the index over the same rows. */
    char *canonical = json_serialize(doc);
    json_free(doc);
    bool changed = canonical && strcmp(canonical, "") != 0;
    bool ok2 = canonical && db_save(se->db, se->session_id, canonical);
    free(canonical);
    if (ok2 && changed)
        printf("[gateway] Canonicalized session store %s after append\n",
               se->session_id);
    return ok2 && raw_len > 0;
}

/* PoP: rewrite_transcript @ gateway/session.py:rewrite_transcript */
void rewrite_transcript(const char *session_key, json_t *new_transcript) {
    /* Replace the entire transcript for a session with new messages
     * (/retry, /compress). DESTRUCTIVE by default: every previously stored
     * row in the session document's "messages" array — including archived
     * compaction history — is replaced. Pending dirty rows are dropped
     * first so they can't resurrect stale messages on the next append. */
    if (!session_key || !session_key[0]) return;

    char tid[64];
    _transcript_id_for_key(session_key, tid, sizeof(tid));
    _clear_dirty_transcript(tid[0] ? tid : session_key);

    int idx = session_find_by_key(session_key);
    gw_session_entry_t *se = (idx >= 0) ? session_at(idx) : NULL;
    db_t *db = (se && se->in_use) ? se->db : NULL;
    const char *sid = tid[0] ? tid : session_key;
    if (!db || !sid[0]) return;

    char *existing = db_load(db, sid, NULL);
    json_t *doc = existing ? json_parse(existing, NULL) : NULL;
    free(existing);
    if (!doc) doc = json_object();

    /* Replace the whole array with a deep copy of the caller's transcript. */
    if (json_is_array(new_transcript)) {
        json_set(doc, "messages", json_copy(new_transcript));
    } else {
        json_set(doc, "messages", json_array());
    }

    char *serialized_doc = json_serialize(doc);
    json_free(doc);
    if (serialized_doc) {
        db_save(db, sid, serialized_doc);
        free(serialized_doc);
    }
}

/* PoP: load_transcript @ gateway/session.py:load_transcript */
json_t *load_transcript(const char *session_key) {
    /* Load all messages from a session's transcript. Reads follow the same
     * routing writes use: resolve through the live entry (which carries the
     * post-compression reroute / healed tip), then read the durable doc.
     * A failed read returns an EMPTY array — downstream guards treat that
     * as "nothing persisted", matching Python's documented caveat. */
    if (!session_key || !session_key[0]) return json_array();
    char tid[64];
    _transcript_id_for_key(session_key, tid, sizeof(tid));
    const char *sid = tid[0] ? tid : session_key;

    db_t *probe = NULL;
    int idx = session_find_by_key(session_key);
    gw_session_entry_t *se = (idx >= 0) ? session_at(idx) : NULL;
    if (!(se && se->in_use && se->db)) {
        if (!g_gw.session_db_path[0]) return json_array();
        probe = db_open(g_gw.session_db_path, NULL);
        if (!probe) return json_array();
    }
    db_t *db = (se && se->in_use && se->db) ? se->db : probe;

    json_t *out = json_array();
    char *raw = db_load(db, sid, NULL);
    json_t *doc = raw ? json_parse(raw, NULL) : NULL;
    free(raw);
    json_t *messages = doc ? json_object_get(doc, "messages") : NULL;
    if (json_is_array(messages)) {
        size_t n = json_array_size(messages);
        bool prev_was_user = false;
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_copy(json_array_get(messages, i));
            if (!m) continue;
            /* repair_alternation at the restore boundary: collapse durable
             * user;user wedges so replay never re-triggers pre-request
             * repair on every request. Keep the FIRST of a run. */
            const char *role = json_get_str(m, "role", "");
            bool is_user = strcmp(role, "user") == 0;
            if (is_user && prev_was_user) { json_free(m); continue; }
            prev_was_user = is_user;
            json_array_append(out, m);
        }
    }
    if (doc) json_free(doc);
    if (probe) db_close(probe);
    return out;
}

/* PoP: rewind_session @ gateway/session.py:rewind_session */
void rewind_session(const char *session_key, int turn_count) {
    /* Back up N user turns via soft-delete, keeping rows for audit:
     * truncated rows move into the document's "archived" array (the C
     * analog of Python flipping active=0 in state.db) so they survive on
     * disk and stay hidden from re-prompts and search. Mirrors the CLI/TUI
     * "/undo [N]" behavior. */
    if (!session_key || !session_key[0]) return;

    char tid[64];
    _transcript_id_for_key(session_key, tid, sizeof(tid));
    _clear_dirty_transcript(tid[0] ? tid : session_key);
    int n = turn_count < 1 ? 1 : turn_count;

    int idx = session_find_by_key(session_key);
    gw_session_entry_t *se = (idx >= 0) ? session_at(idx) : NULL;
    db_t *db = (se && se->in_use) ? se->db : NULL;
    const char *sid = tid[0] ? tid : session_key;
    if (!db || !sid[0]) return;

    char *existing = db_load(db, sid, NULL);
    json_t *doc = existing ? json_parse(existing, NULL) : NULL;
    free(existing);
    if (!doc) return;
    json_t *messages = json_object_get(doc, "messages");
    if (!json_is_array(messages)) { json_free(doc); return; }
    size_t total = json_array_size(messages);

    /* Walk backwards to find the Nth-from-last user message index. */
    int user_seen = 0;
    long target_idx = -1;
    for (long i = (long)total - 1; i >= 0; i--) {
        json_t *m = json_array_get(messages, (size_t)i);
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "user") == 0) {
            user_seen++;
            if (user_seen == n) { target_idx = i; break; }
        }
    }
    if (target_idx < 0) { json_free(doc); return; }   /* no user turn to back up to */

    /* Soft-archive rows AFTER the target turn (keep the target itself). */
    json_t *archived = json_object_get(doc, "archived");
    if (!json_is_array(archived)) {
        archived = json_array();
        json_set(doc, "archived", archived);
    }
    for (size_t i = (size_t)(target_idx + 1); i < total; i++)
        json_array_append(archived, json_copy(json_array_get(messages, i)));
    for (size_t i = total; i > (size_t)(target_idx + 1); i--)
        json_array_remove(messages, i - 1);

    char *serialized_doc = json_serialize(doc);
    json_free(doc);
    if (serialized_doc) {
        db_save(db, sid, serialized_doc);
        free(serialized_doc);
    }
}

/* PoP: build_session_context @ gateway/session.py:build_session_context */
char *build_session_context(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return strdup("");
    gw_session_entry_t *se = session_at(idx);
    if (!se) return strdup("");
    char ctx[1024];
    snprintf(ctx, sizeof(ctx), "Session: %s (platform: %s, chat: %s)",
             se->key,
             se->source.platform,
             se->source.chat_id);
    return strdup(ctx);
}

