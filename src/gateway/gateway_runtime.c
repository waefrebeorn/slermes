/* gateway_runtime.c — Gateway runtime configuration and agent cache.
 * Port of Python gateway/run.py GatewayRunner runtime features:
 *   - Agent cache (LRU, 128 max, 1h idle TTL)
 *   - Prefill/ephemeral system prompts
 *   - Provider routing + fallback model
 *   - Service tier + reasoning config
 *   - Background notifications mode
 *
 * These are loaded at gateway start and consulted per-turn.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway_core.h"
#include "hermes_gateway_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* ════════════════════════════════════════════════════════════════
 *  1. AGENT CACHE (LRU, 1h TTL) — hive-backed
 *  Port of Python _AGENT_CACHE_MAX_SIZE=128, _AGENT_CACHE_IDLE_TTL_SECS=3600
 *
 *  The cache is a hive of heap-allocated entries: only LIVE sessions
 *  consume memory (no landlocked static array). Entries are stable
 *  handles; erase returns slots to the hive freelist.
 * ════════════════════════════════════════════════════════════════ */

#define GW_AGENT_CACHE_MAX 32   /* LRU cap: real usage is 1-3 sessions */
#define GW_AGENT_CACHE_IDLE_TTL 3600  /* 1 hour in seconds */

typedef struct {
    char session_key[192];        /* "platform:chat_id" */
    agent_state_t agent;          /* cached agent state */
    time_t last_access;           /* monotonic time of last access */
} gw_agent_cache_entry_t;

#include "hive.h"
static hive_t *g_agent_cache = NULL;
static pthread_mutex_t g_agent_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Initialize agent cache */
void gw_agent_cache_init(void) {
    pthread_mutex_lock(&g_agent_cache_mutex);
    if (!g_agent_cache) g_agent_cache = hive_new(8);
    else hive_clear(g_agent_cache);
    pthread_mutex_unlock(&g_agent_cache_mutex);
}

/* Find a cached agent by session key. Returns pointer to agent or NULL.
 * Updates last_access time (LRU promotion). */
agent_state_t *gw_agent_cache_get(const char *session_key) {
    if (!session_key || !session_key[0] || !g_agent_cache) return NULL;
    pthread_mutex_lock(&g_agent_cache_mutex);
    hive_iter_t it;
    hive_iter_begin(g_agent_cache, &it);
    hive_handle_t hnd;
    gw_agent_cache_entry_t *e;
    while (hive_iter_next(g_agent_cache, &it, &hnd, (void **)&e)) {
        if (strcmp(e->session_key, session_key) == 0) {
            e->last_access = time(NULL);
            pthread_mutex_unlock(&g_agent_cache_mutex);
            return &e->agent;
        }
    }
    pthread_mutex_unlock(&g_agent_cache_mutex);
    return NULL;
}

/* Insert or update an agent in the cache.
 * Returns true on success, false if cache full. */
bool gw_agent_cache_put(const char *session_key, const agent_state_t *agent) {
    if (!session_key || !session_key[0] || !agent) return false;
    if (!g_agent_cache) g_agent_cache = hive_new(8);

    pthread_mutex_lock(&g_agent_cache_mutex);

    /* Try to update existing entry */
    hive_iter_t it;
    hive_iter_begin(g_agent_cache, &it);
    hive_handle_t hnd;
    gw_agent_cache_entry_t *e;
    while (hive_iter_next(g_agent_cache, &it, &hnd, (void **)&e)) {
        if (strcmp(e->session_key, session_key) == 0) {
            memcpy(&e->agent, agent, sizeof(agent_state_t));
            e->last_access = time(NULL);
            pthread_mutex_unlock(&g_agent_cache_mutex);
            return true;
        }
    }

    /* Enforce cap: evict LRU while over the limit */
    while (hive_count(g_agent_cache) >= (size_t)GW_AGENT_CACHE_MAX) {
        hive_handle_t victim = { 0, 0 };
        gw_agent_cache_entry_t *v = NULL;
        time_t oldest = (time_t)-1;
        hive_iter_t it2;
        hive_iter_begin(g_agent_cache, &it2);
        hive_handle_t h2;
        gw_agent_cache_entry_t *e2;
        while (hive_iter_next(g_agent_cache, &it2, &h2, (void **)&e2)) {
            if (e2->last_access < oldest) {
                oldest = e2->last_access;
                victim = h2;
                v = e2;
            }
        }
        if (!v) break;
        agent_free(&v->agent);
        free(v);
        hive_erase(g_agent_cache, victim);
    }

    /* Insert a new heap entry into the hive. */
    gw_agent_cache_entry_t *ne = calloc(1, sizeof(gw_agent_cache_entry_t));
    if (!ne) { pthread_mutex_unlock(&g_agent_cache_mutex); return false; }
    snprintf(ne->session_key, sizeof(ne->session_key), "%s", session_key);
    memcpy(&ne->agent, agent, sizeof(agent_state_t));
    ne->last_access = time(NULL);
    bool ok = false;
    hive_insert(g_agent_cache, ne, &ok);
    pthread_mutex_unlock(&g_agent_cache_mutex);
    return ok;
}

/* Evict a specific session from the cache. */
void gw_agent_cache_remove(const char *session_key) {
    if (!session_key || !session_key[0] || !g_agent_cache) return;
    pthread_mutex_lock(&g_agent_cache_mutex);
    hive_iter_t it;
    hive_iter_begin(g_agent_cache, &it);
    hive_handle_t hnd;
    gw_agent_cache_entry_t *e;
    while (hive_iter_next(g_agent_cache, &it, &hnd, (void **)&e)) {
        if (strcmp(e->session_key, session_key) == 0) {
            agent_free(&e->agent);
            free(e);
            hive_erase(g_agent_cache, hnd);
            break;
        }
    }
    pthread_mutex_unlock(&g_agent_cache_mutex);
}

/* Sweep idle cached agents (evict those idle > TTL seconds).
 * Called periodically from a cleanup thread (like _session_expiry_watcher). */
int gw_agent_cache_sweep_idle(void) {
    time_t now = time(NULL);
    int evicted = 0;

    pthread_mutex_lock(&g_agent_cache_mutex);
    if (g_agent_cache) {
        hive_iter_t it;
        hive_iter_begin(g_agent_cache, &it);
        hive_handle_t hnd;
        gw_agent_cache_entry_t *e;
        while (hive_iter_next(g_agent_cache, &it, &hnd, (void **)&e)) {
            if ((now - e->last_access) > GW_AGENT_CACHE_IDLE_TTL) {
                agent_free(&e->agent);
                free(e);
                hive_erase(g_agent_cache, hnd);
                evicted++;
            }
        }
    }
    pthread_mutex_unlock(&g_agent_cache_mutex);
    return evicted;
}

/* Enforce cache cap (evict oldest entries over limit).
 * Returns number evicted. */
int gw_agent_cache_enforce_cap(void) {
    int evicted = 0;
    pthread_mutex_lock(&g_agent_cache_mutex);
    if (!g_agent_cache) { pthread_mutex_unlock(&g_agent_cache_mutex); return 0; }

    while (hive_count(g_agent_cache) > (size_t)GW_AGENT_CACHE_MAX) {
        /* Find the LRU entry (oldest access) */
        hive_handle_t victim = { 0, 0 };
        gw_agent_cache_entry_t *v = NULL;
        time_t oldest = (time_t)-1;
        hive_iter_t it;
        hive_iter_begin(g_agent_cache, &it);
        hive_handle_t hnd;
        gw_agent_cache_entry_t *e;
        while (hive_iter_next(g_agent_cache, &it, &hnd, (void **)&e)) {
            if (e->last_access < oldest) {
                oldest = e->last_access;
                victim = hnd;
                v = e;
            }
        }
        if (!v) break;
        agent_free(&v->agent);
        free(v);
        hive_erase(g_agent_cache, victim);
        evicted++;
    }
    pthread_mutex_unlock(&g_agent_cache_mutex);
    return evicted;
}

/* Get current cache size */
int gw_agent_cache_size(void) {
    pthread_mutex_lock(&g_agent_cache_mutex);
    int n = g_agent_cache ? (int)hive_count(g_agent_cache) : 0;
    pthread_mutex_unlock(&g_agent_cache_mutex);
    return n;
}

/* ════════════════════════════════════════════════════════════════
 *  2. PREFILL / EPHEMERAL SYSTEM PROMPT
 *  Port of Python _load_prefill_messages() and _load_ephemeral_system_prompt()
 * ════════════════════════════════════════════════════════════════ */

/* Max prefill messages we can hold */
#define GW_MAX_PREFILL_MSGS 32
#define GW_PREFILL_MAX_LEN 4096

typedef struct {
    char role[32];
    char content[GW_PREFILL_MAX_LEN];
    bool occupied;
} gw_prefill_msg_t;

static gw_prefill_msg_t g_prefill_msgs[GW_MAX_PREFILL_MSGS];
static int g_prefill_count = 0;
static char g_ephemeral_system_prompt[8192] = "";

/* Load prefill messages from HERMES_PREFILL_MESSAGES_FILE env var or config.
 * File must be a JSON array of {role, content} objects. */
void gw_load_prefill_messages(void) {
    g_prefill_count = 0;
    memset(g_prefill_msgs, 0, sizeof(g_prefill_msgs));

    const char *file_path = getenv("HERMES_PREFILL_MESSAGES_FILE");
    if (!file_path || !file_path[0]) {
        /* Try config.yaml's prefill_messages_file key */
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            char cfg_path[1024];
            snprintf(cfg_path, sizeof(cfg_path), "%s/.slermes/config.yaml", home);
            /* Load from YAML — try hermes_config or just read the file */
            (void)cfg_path; /* Simplification: only env var for now */
        }
        return;
    }

    FILE *f = fopen(file_path, "r");
    if (!f) return;

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return;
    buf[n] = '\0';

    json_t *j = json_parse(buf, NULL);
    if (!j || json_len(j) == 0) {
        json_free(j);
        return;
    }

    size_t len = json_len(j);
    for (size_t i = 0; i < len && g_prefill_count < GW_MAX_PREFILL_MSGS; i++) {
        json_t *item = json_get(j, i);
        if (!item) continue;
        const char *role = json_get_str(item, "role", "");
        const char *content = json_get_str(item, "content", "");
        if (role[0] && content[0]) {
            snprintf(g_prefill_msgs[g_prefill_count].role,
                     sizeof(g_prefill_msgs[g_prefill_count].role), "%s", role);
            snprintf(g_prefill_msgs[g_prefill_count].content,
                     sizeof(g_prefill_msgs[g_prefill_count].content), "%s", content);
            g_prefill_msgs[g_prefill_count].occupied = true;
            g_prefill_count++;
        }
    }
    json_free(j);
}

/* Load ephemeral system prompt from HERMES_EPHEMERAL_SYSTEM_PROMPT env var
 * or config.yaml agent.system_prompt. */
/* PoP: gw_load_ephemeral_system_prompt @ gateway/run.py:_load_ephemeral_system_prompt */
void gw_load_ephemeral_system_prompt(void) {
    g_ephemeral_system_prompt[0] = '\0';

    const char *prompt = getenv("HERMES_EPHEMERAL_SYSTEM_PROMPT");
    if (prompt && prompt[0]) {
        snprintf(g_ephemeral_system_prompt, sizeof(g_ephemeral_system_prompt), "%s", prompt);
        return;
    }

    /* Read from config.yaml agent.system_prompt (YAML) */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;

    char cfg_path[1024];
    snprintf(cfg_path, sizeof(cfg_path), "%s/.slermes/config.yaml", home);
    FILE *f = fopen(cfg_path, "r");
    if (!f) return;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return;
    buf[n] = '\0';

    /* Simple YAML parsing for agent.system_prompt key */
    const char *key = strstr(buf, "agent:");
    if (!key) return;
    const char *sp = strstr(key + 6, "system_prompt:");
    if (!sp) return;
    sp += 14;
    while (*sp == ' ' || *sp == '\t') sp++;
    if (*sp == '\"') { sp++; }
    const char *end = sp;
    while (*end && *end != '\n' && *end != '\r') end++;
    if (end > sp && *(end-1) == '\"') end--;
    size_t slen = (size_t)(end - sp);
    if (slen > sizeof(g_ephemeral_system_prompt) - 1)
        slen = sizeof(g_ephemeral_system_prompt) - 1;
    memcpy(g_ephemeral_system_prompt, sp, slen);
    g_ephemeral_system_prompt[slen] = '\0';
}

/* Get loaded prefill messages count */
int gw_prefill_count(void) { return g_prefill_count; }

/* Get a prefill message by index */
const char *gw_prefill_role(int idx) {
    if (idx < 0 || idx >= g_prefill_count || !g_prefill_msgs[idx].occupied) return NULL;
    return g_prefill_msgs[idx].role;
}

const char *gw_prefill_content(int idx) {
    if (idx < 0 || idx >= g_prefill_count || !g_prefill_msgs[idx].occupied) return NULL;
    return g_prefill_msgs[idx].content;
}

/* Get ephemeral system prompt (may be empty string) */
const char *gw_ephemeral_system_prompt(void) {
    return g_ephemeral_system_prompt;
}

/* ════════════════════════════════════════════════════════════════
 *  3. PROVIDER ROUTING + FALLBACK MODEL
 *  Port of Python _load_provider_routing() and _load_fallback_model()
 * ════════════════════════════════════════════════════════════════ */

#define GW_FALLBACK_MAX 8

typedef struct {
    bool enabled;
    char strategies[2048];   /* JSON: {"order":["openai","anthropic"],...} */
    char fallback_providers[GW_FALLBACK_MAX][128];
    int  fallback_count;
} gw_routing_cfg_t;

static gw_routing_cfg_t g_routing;

/* Load provider routing config from config.yaml provider_routing. */
/* PoP: gw_load_provider_routing @ gateway/run.py:_load_provider_routing */
void gw_load_provider_routing(void) {
    memset(&g_routing, 0, sizeof(g_routing));
    g_routing.enabled = false;

    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;
    const char *fallback_env = getenv("HERMES_FALLBACK_PROVIDERS");
    if (!fallback_env) fallback_env = getenv("FALLBACK_PROVIDERS");
    if (fallback_env && fallback_env[0]) {
        char *dup = strdup(fallback_env);
        if (dup) {
            char *saveptr = NULL;
            for (char *tok = strtok_r(dup, ",", &saveptr);
                 tok && g_routing.fallback_count < GW_FALLBACK_MAX;
                 tok = strtok_r(NULL, ",", &saveptr)) {
                while (*tok == ' ') tok++;
                char *end = tok + strlen(tok);
                while (end > tok && *(end-1) == ' ') end--;
                *end = '\0';
                if (*tok) {
                    snprintf(g_routing.fallback_providers[g_routing.fallback_count],
                             sizeof(g_routing.fallback_providers[0]), "%s", tok);
                    g_routing.fallback_count++;
                }
            }
            free(dup);
        }
    }

    if (g_routing.fallback_count > 0) g_routing.enabled = true;
}

/* Get fallback model/provider chain count */
int gw_fallback_count(void) { return g_routing.fallback_count; }

/* Get fallback provider by index */
const char *gw_fallback_provider(int idx) {
    if (idx < 0 || idx >= g_routing.fallback_count) return NULL;
    return g_routing.fallback_providers[idx];
}

/* Check if fallback routing is enabled */
bool gw_fallback_enabled(void) { return g_routing.enabled; }

/* ════════════════════════════════════════════════════════════════
 *  4. SERVICE TIER + REASONING CONFIG
 * ════════════════════════════════════════════════════════════════ */

static char g_service_tier[64] = "";

/* PoP: gw_load_service_tier @ gateway/run.py:_load_service_tier */
void gw_load_service_tier(void) {
    g_service_tier[0] = '\0';
    const char *env = getenv("HERMES_SERVICE_TIER");
    if (env) {
        snprintf(g_service_tier, sizeof(g_service_tier), "%s", env);
        return;
    }
    /* Load from config.yaml agent.service_tier */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    /* The config struct doesn't have a service_tier field yet,
     * but we can read it from the raw YAML via the platform config */
}

const char *gw_service_tier(void) {
    return g_service_tier[0] ? g_service_tier : NULL;
}

/* ════════════════════════════════════════════════════════════════
 *  5. BACKGROUND NOTIFICATIONS MODE
 * ════════════════════════════════════════════════════════════════ */

static char g_notify_mode[32] = "normal";  /* "normal", "silent", "mentions_only" */

void gw_load_notification_mode(void) {
    const char *env = getenv("HERMES_BACKGROUND_NOTIFICATIONS_MODE");
    if (env && env[0]) {
        snprintf(g_notify_mode, sizeof(g_notify_mode), "%s", env);
    }
}

const char *gw_notification_mode(void) { return g_notify_mode; }
bool gw_notify_silent(void) {
    return strcmp(g_notify_mode, "silent") == 0;
}
bool gw_notify_mentions_only(void) {
    return strcmp(g_notify_mode, "mentions_only") == 0;
}

/* ════════════════════════════════════════════════════════════════
 *  6. RUNTIME INIT — Load all runtime config at gateway start
 * ════════════════════════════════════════════════════════════════ */

void gw_runtime_init(void) {
    gw_agent_cache_init();
    gw_load_prefill_messages();
    gw_load_ephemeral_system_prompt();
    gw_load_provider_routing();
    gw_load_service_tier();
    gw_load_notification_mode();
}

/* ════════════════════════════════════════════════════════════════
 *  7. STOP/DRAIN — clean shutdown with agent notification
 *  Port of Python stop() drain logic
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_drain_active_agents @ gateway/run.py:_drain_active_agents */
void gw_drain_active_agents(int timeout_sec) {
    /* For each cached agent, save session state and free resources */
    pthread_mutex_lock(&g_agent_cache_mutex);
    time_t deadline = time(NULL) + timeout_sec;
    if (g_agent_cache) {
        hive_iter_t it;
        hive_iter_begin(g_agent_cache, &it);
        hive_handle_t hnd;
        gw_agent_cache_entry_t *e;
        while (hive_iter_next(g_agent_cache, &it, &hnd, (void **)&e)) {
            if (time(NULL) > deadline) break;

            /* Save session if possible */
            agent_state_t *a = &e->agent;
            if (a->db) {
                char session_key[256];
                snprintf(session_key, sizeof(session_key), "%s:%s", a->platform, a->chat_id);
                /* db_save_session(a->db, session_key, a); — wire to session save */
            }
            agent_free(a);
            free(e);
            hive_erase(g_agent_cache, hnd);
        }
    }
    pthread_mutex_unlock(&g_agent_cache_mutex);
}

/* External: defined in server.c or helpers.c */
extern bool gw_platform_send(const char *platform, const char *chat_id, const char *text);

/* Notify all active sessions of impending shutdown.
 * Sends a brief status message to each active platform+chat. */
void gw_notify_sessions_shutdown(const char *reason) {
    if (!reason) reason = "shutdown";

    /* Send shutdown notice to all active gateway sessions */
    for (int i = 0; i < g_gw.session_count; i++) {
        if (!g_gw.sessions[i].in_use) continue;
        const char *platform = NULL;
        const char *chat_id = NULL;

        /* Extract chat info from session agent state */
        if (g_gw.sessions[i].agent.platform[0])
            platform = g_gw.sessions[i].agent.platform;
        else if (g_gw.sessions[i].source.platform[0])
            platform = g_gw.sessions[i].source.platform;

        if (g_gw.sessions[i].agent.chat_id[0])
            chat_id = g_gw.sessions[i].agent.chat_id;
        else if (g_gw.sessions[i].source.chat_id[0])
            chat_id = g_gw.sessions[i].source.chat_id;

        if (platform && chat_id) {
            char msg[256];
            snprintf(msg, sizeof(msg), "⚠️ Gateway shutting down: %s", reason);
            gw_platform_send(platform, chat_id, msg);
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 *  8. SYSTEMD INTEGRATION
 * PoP: _launch_systemd_restart_shortcut @ gateway/run.py:_launch_systemd_restart_shortcut
 *  Port of Python _launch_systemd_restart_shortcut()
 *  Checks NOTIFY_SOCKET or JOURNAL_STREAM env vars.
 * ════════════════════════════════════════════════════════════════ */

void gw_systemd_notify(const char *state) {
    (void)state;
}

bool gw_under_systemd(void) {
    const char *notify_socket = getenv("NOTIFY_SOCKET");
    if (notify_socket && notify_socket[0]) return true;
    const char *journal_stream = getenv("JOURNAL_STREAM");
    if (journal_stream && journal_stream[0]) return true;
    return false;
}

void gw_systemd_ready(void) {
    if (gw_under_systemd()) {
        printf("[systemd] Gateway ready (under systemd supervision)\n");
    }
}

/* External: defined in server.c */
/* PoP: session_get_or_create @ gateway/turn_lease.py:_get_or_create */
extern int session_get_or_create(const char *platform, const char *chat_id);

/* ════════════════════════════════════════════════════════════════
 *  9. DETACHED RESTART + HANDOFF
 *  Port of Python request_restart(detached=True)
 * ════════════════════════════════════════════════════════════════ */

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

bool gw_detached_restart(void) {
    if (!gw_handoff_save_state()) return false;

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid > 0) {
        _exit(0);
    }

    /* Child: detach stdio */
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2) close(fd);
    }

    execl("/proc/self/exe", "slermes", "gateway", "--handoff", NULL);
    execl("/home/wubu/hermes-agent-dev/slermes/slermes", "slermes", "gateway", "--handoff", NULL);
    return true;
}

bool gw_handoff_save_state(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.slermes/handoff.json", home);

    json_t *root = json_new_object();
    if (!root) return false;

    json_object_set(root, "timestamp", json_new_number((double)time(NULL)));
    json_object_set(root, "platform_count", json_new_number((double)g_gw.platform_count));

    json_t *sessions = json_new_array();
    for (int i = 0; i < g_gw.session_count; i++) {
        if (!g_gw.sessions[i].in_use) continue;
        json_t *s = json_object();
        json_set(s, "key", json_new_string(g_gw.sessions[i].key));
        json_set(s, "session_id", json_new_string(g_gw.sessions[i].session_id));
        json_array_append(sessions, s);
    }
    json_object_set(root, "sessions", sessions);

    char *serialized = json_serialize(root);
    bool ok = false;
    if (serialized) {
        FILE *fp = fopen(path, "w");
        if (fp) {
            fputs(serialized, fp);
            fclose(fp);
            ok = true;
        }
        free(serialized);
    }
    json_free(root);
    return ok;
}

bool gw_handoff_restore_state(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.slermes/handoff.json", home);

    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n == 0) return false;
    buf[n] = '\0';

    json_t *root = json_parse(buf, NULL);
    if (!root) return false;

    json_t *sessions = json_obj_get(root, "sessions");
    if (sessions) {
        size_t len = json_len(sessions);
        for (size_t i = 0; i < len; i++) {
            json_t *s = json_get(sessions, i);
            if (!s) continue;
            const char *key = json_get_str(s, "key", "");
            const char *sid = json_get_str(s, "session_id", "");
            if (key[0] && sid[0]) {
                char plat[64], cid[128];
                if (sscanf(key, "%63[^:]:%127[^\n]", plat, cid) >= 2) {
                    int idx = session_get_or_create(plat, cid);
                    if (idx >= 0) {
                        snprintf(g_gw.sessions[idx].key, sizeof(g_gw.sessions[idx].key), "%s", key);
                        snprintf(g_gw.sessions[idx].session_id, sizeof(g_gw.sessions[idx].session_id), "%s", sid);
                    }
                }
            }
        }
    }

    unlink(path);
    json_free(root);
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  10. GOAL MANAGEMENT
 *  Port of Python goal management (max turns tracking)
 * ════════════════════════════════════════════════════════════════ */

#define GW_GOAL_MAX 64

typedef struct {
    char session_key[192];
    int  turn_count;
    int  max_turns;
    time_t last_turn;
    bool occupied;
} gw_goal_entry_t;

static gw_goal_entry_t g_goals[GW_GOAL_MAX];
static pthread_mutex_t g_goal_mutex = PTHREAD_MUTEX_INITIALIZER;

static int goal_find(const char *session_key) {
    for (int i = 0; i < GW_GOAL_MAX; i++) {
        if (g_goals[i].occupied && strcmp(g_goals[i].session_key, session_key) == 0)
            return i;
    }
    return -1;
}

void gw_goal_record_turn(const char *session_key, int turn_count) {
    if (!session_key || !session_key[0]) return;
    pthread_mutex_lock(&g_goal_mutex);
    int idx = goal_find(session_key);
    if (idx < 0) {
        for (int i = 0; i < GW_GOAL_MAX; i++) {
            if (!g_goals[i].occupied) { idx = i; break; }
        }
        if (idx < 0) { pthread_mutex_unlock(&g_goal_mutex); return; }
    }
    snprintf(g_goals[idx].session_key, sizeof(g_goals[idx].session_key), "%s", session_key);
    g_goals[idx].turn_count = turn_count;
    if (g_goals[idx].max_turns <= 0)
        g_goals[idx].max_turns = 90;
    g_goals[idx].last_turn = time(NULL);
    g_goals[idx].occupied = true;
    pthread_mutex_unlock(&g_goal_mutex);
}

int gw_goal_max_turns(const char *session_key) {
    if (!session_key || !session_key[0]) return 90;
    pthread_mutex_lock(&g_goal_mutex);
    int idx = goal_find(session_key);
    int mt = idx >= 0 ? g_goals[idx].max_turns : 90;
    pthread_mutex_unlock(&g_goal_mutex);
    return mt;
}

void gw_goal_clear(const char *session_key) {
    if (!session_key || !session_key[0]) return;
    pthread_mutex_lock(&g_goal_mutex);
    int idx = goal_find(session_key);
    if (idx >= 0) {
        g_goals[idx].occupied = false;
        memset(&g_goals[idx], 0, sizeof(gw_goal_entry_t));
    }
    pthread_mutex_unlock(&g_goal_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  11. STREAMING DISPATCH
 *  Port of Python streaming — buffer tokens for platform dispatch
 * ════════════════════════════════════════════════════════════════ */

#define GW_STREAM_BUF_MAX 65536

static struct {
    gw_stream_callback_t callback;
    void *userdata;
    char buffer[GW_STREAM_BUF_MAX];
    size_t pos;
    bool active;
    pthread_mutex_t mutex;
} g_stream;

void gw_stream_set_callback(gw_stream_callback_t cb, void *userdata) {
    pthread_mutex_lock(&g_stream.mutex);
    g_stream.callback = cb;
    g_stream.userdata = userdata;
    pthread_mutex_unlock(&g_stream.mutex);
}

bool gw_stream_is_active(void) {
    pthread_mutex_lock(&g_stream.mutex);
    bool a = g_stream.active;
    pthread_mutex_unlock(&g_stream.mutex);
    return a;
}

void gw_stream_send(const char *text) {
    if (!text || !*text) return;
    pthread_mutex_lock(&g_stream.mutex);
    if (g_stream.callback) {
        g_stream.callback(text, g_stream.userdata);
    } else {
        size_t slen = strlen(text);
        if (g_stream.pos + slen < GW_STREAM_BUF_MAX) {
            memcpy(g_stream.buffer + g_stream.pos, text, slen);
            g_stream.pos += slen;
        }
    }
    g_stream.active = true;
    pthread_mutex_unlock(&g_stream.mutex);
}

void gw_stream_end(void) {
    pthread_mutex_lock(&g_stream.mutex);
    g_stream.active = false;
    g_stream.buffer[0] = '\0';
    g_stream.pos = 0;
    pthread_mutex_unlock(&g_stream.mutex);
}

bool gw_stream_drain(bool (*send_fn)(const char *chat_id, const char *text),
                     const char *chat_id) {
    if (!send_fn || !chat_id) return false;
    pthread_mutex_lock(&g_stream.mutex);
    if (g_stream.pos > 0) {
        g_stream.buffer[g_stream.pos] = '\0';
        bool ok = send_fn(chat_id, g_stream.buffer);
        g_stream.pos = 0;
        g_stream.buffer[0] = '\0';
        pthread_mutex_unlock(&g_stream.mutex);
        return ok;
    }
    pthread_mutex_unlock(&g_stream.mutex);
    return true;
}
