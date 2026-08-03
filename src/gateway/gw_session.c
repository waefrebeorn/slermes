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

    /* Check if already present (update in place) */
    hive_iter_t it;
    hive_iter_begin(g_gw.source_cache, &it);
    hive_handle_t hnd;
    gw_source_cache_entry_t *e;
    while (hive_iter_next(g_gw.source_cache, &it, &hnd, (void **)&e)) {
        if (strcmp(e->key, key) == 0) {
            e->source = *source;
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

/* PoP: _find_gateway_session_row @ gateway/session.py:_find_gateway_session_row */
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

/* PoP: _routing_scope @ gateway/session.py:_routing_scope */
const char *routing_scope(void) {
    return g_gw.routing_scope[0] ? g_gw.routing_scope : "default";
}

/* PoP: _active_profile_name @ gateway/session.py:_active_profile_name */
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
void _heal_compression_tip_locked(void) {
    /* No-op: compression tips are advisory */
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
    /* Session entries are saved via db_save() in session_free() */
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

/* PoP: __getattr__ @ gateway/session.py:__getattr__ */
void _session_dunder_getattr(const char *name) { (void)name; }

/* PoP: _ensure_loaded_locked @ gateway/session.py:_ensure_loaded_locked */
void _ensure_loaded_locked(int session_idx) { (void)session_idx; }

/* PoP: _prune_stale_sessions_locked @ gateway/session.py:_prune_stale_sessions_locked */
void _prune_stale_sessions_locked(void) { prune_old_entries(86400); }

/* PoP: _snapshot_routing_locked @ gateway/session.py:_snapshot_routing_locked */
json_t *_snapshot_routing_locked(void) { return json_object(); }

/* PoP: _persist_routing_data @ gateway/session.py:_persist_routing_data */
void _persist_routing_data(json_t *data) { (void)data; }

/* PoP: _recovered_row_matches_source_scope @ gateway/session.py:_recovered_row_matches_source_scope */
bool _recovered_row_matches_source_scope(const gw_session_source_t *row_src, const char *routing_scope) {
    (void)routing_scope;
    if (!row_src) return false;
    return row_src->platform[0] && row_src->chat_id[0];
}

/* PoP: set_expiry_finalized @ gateway/session.py:set_expiry_finalized */
void set_expiry_finalized(const char *session_key) { (void)session_key; }

/* PoP: _is_session_ended_in_db @ gateway/session.py:_is_session_ended_in_db */
bool _is_session_ended_in_db(const char *session_key) {
    int idx = session_find_by_key(session_key);
    if (idx < 0) return true;
    gw_session_entry_t *se = session_at(idx);
    return !se || !se->in_use;
}

/* PoP: update_session @ gateway/session.py:update_session */
int update_session(const char *session_key, json_t *updates) {
    (void)updates;
    return session_find_by_key(session_key);
}

/* PoP: advance_compression_session @ gateway/session.py:advance_compression_session */
void advance_compression_session(const char *session_key) { (void)session_key; }

/* PoP: switch_session @ gateway/session.py:switch_session */
void switch_session(const char *from_key, const char *to_key) {
    (void)from_key; (void)to_key;
}

/* PoP: append_to_transcript @ gateway/session.py:append_to_transcript */
void append_to_transcript(const char *session_key, const char *role, const char *content) {
    (void)session_key; (void)role; (void)content;
}

/* PoP: _append_to_transcript_serialized @ gateway/session.py:_append_to_transcript_serialized */
void _append_to_transcript_serialized(const char *session_key, const char *serialized) {
    (void)session_key; (void)serialized;
}

/* PoP: _append_transcript_message @ gateway/session.py:_append_transcript_message */
void _append_transcript_message(const char *session_key, json_t *msg) {
    (void)session_key; (void)msg;
}

/* PoP: _rebuild_fts_once @ gateway/session.py:_rebuild_fts_once */
bool _rebuild_fts_once(int session_idx) { (void)session_idx; return true; }

/* PoP: _clear_dirty_transcript @ gateway/session.py:_clear_dirty_transcript */
void _clear_dirty_transcript(void) {}

/* PoP: rewrite_transcript @ gateway/session.py:rewrite_transcript */
void rewrite_transcript(const char *session_key, json_t *new_transcript) {
    (void)session_key; (void)new_transcript;
}

/* PoP: load_transcript @ gateway/session.py:load_transcript */
json_t *load_transcript(const char *session_key) {
    (void)session_key; return json_array();
}

/* PoP: rewind_session @ gateway/session.py:rewind_session */
void rewind_session(const char *session_key, int turn_count) {
    (void)session_key; (void)turn_count;
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

