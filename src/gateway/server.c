/*
 * server.c — Multi-platform gateway server for Hermes C.
 * Supports Telegram, Discord, Slack, Matrix, Mattermost, Webhook, WhatsApp.
 * Platforms run concurrently via pthread. Each gets its own HTTP client.
 * Configured via --platform flag (single) or config.yaml gateway.platforms list.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "gateway_helpers.h"
#include "hermes_skill_commands.h"
#include "hermes_logger.h"
#include "hermes_telegram_filter.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

/* ================================================================
 *  Gateway state
 * ================================================================ */

gateway_state_t g_gw;

/* ================================================================
 *  P101: Monotonic time helper
 * ================================================================ */

static double gw_mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Forward declaration for gw_queue_drain_all */
static void process_update(const char *platform, const char *chat_id, const char *text);

/* GW13: Kanban notifier thread — polls kanban events and delivers to subscribers */
static void *thread_kanban_notifier(void *arg);

/* ================================================================
 *  P101: Message queue (thread-safe, bounded circular buffer)
 * ================================================================ */

void gw_queue_init(void) {
    g_gw.msg_queue_head = 0;
    g_gw.msg_queue_tail = 0;
    pthread_mutex_init(&g_gw.queue_mutex, NULL);
    pthread_cond_init(&g_gw.queue_cond, NULL);
}

bool gw_queue_push(const char *platform, const char *chat_id,
                    const char *text, const char *thread_id) {
    if (!platform || !chat_id || !text) return false;

    pthread_mutex_lock(&g_gw.queue_mutex);

    /* Check if queue is full */
    int next = (g_gw.msg_queue_head + 1) % GW_QUEUE_MAX;
    if (next == g_gw.msg_queue_tail) {
        /* Queue full — drop oldest */
        g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    }

    gateway_msg_t *slot = &g_gw.msg_queue[g_gw.msg_queue_head];
    snprintf(slot->platform, sizeof(slot->platform), "%s", platform);
    snprintf(slot->chat_id, sizeof(slot->chat_id), "%s", chat_id);
    snprintf(slot->text, sizeof(slot->text), "%s", text);
    if (thread_id)
        snprintf(slot->thread_id, sizeof(slot->thread_id), "%s", thread_id);
    else
        slot->thread_id[0] = '\0';
    slot->timestamp = gw_mono_time();

    g_gw.msg_queue_head = next;

    pthread_cond_signal(&g_gw.queue_cond);
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return true;
}

bool gw_queue_pop(gateway_msg_t *msg) {
    if (!msg) return false;

    pthread_mutex_lock(&g_gw.queue_mutex);
    if (g_gw.msg_queue_head == g_gw.msg_queue_tail) {
        pthread_mutex_unlock(&g_gw.queue_mutex);
        return false; /* empty */
    }

    *msg = g_gw.msg_queue[g_gw.msg_queue_tail];
    g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return true;
}

int gw_queue_depth(void) {
    pthread_mutex_lock(&g_gw.queue_mutex);
    int depth = (g_gw.msg_queue_head - g_gw.msg_queue_tail + GW_QUEUE_MAX) % GW_QUEUE_MAX;
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return depth;
}

/* Drain all queued messages — called periodically from polling threads.
   Each message goes through process_update() which re-checks rate limits.
   If still rate-limited, the message gets re-pushed and picked up next cycle. */
void gw_queue_drain_all(void) {
    gateway_msg_t msgs[GW_QUEUE_MAX];
    int count = 0;
    pthread_mutex_lock(&g_gw.queue_mutex);
    while (g_gw.msg_queue_head != g_gw.msg_queue_tail && count < GW_QUEUE_MAX) {
        msgs[count++] = g_gw.msg_queue[g_gw.msg_queue_tail];
        g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    }
    pthread_mutex_unlock(&g_gw.queue_mutex);
    for (int i = 0; i < count; i++)
        process_update(msgs[i].platform, msgs[i].chat_id, msgs[i].text);
}

/* ================================================================
 *  Gateway Clarify — async clarify prompt response collector
 * ================================================================ */

/* Pending clarify state — set when clarify prompt sent via gateway */
static struct {
    bool            pending;
    char            platform[32];
    char            chat_id[128];
    char            session_key[256];
    char            clarify_id[64];
    char            response[4096];
    pthread_mutex_t mutex;
    pthread_cond_t  cond;   /* signaled when response received */
    /* Optional: poll function for same-platform response capture */
    char *(*poll_fn)(const char *chat_id);
    int           poll_interval;
    /* Choices sent (for matching numeric replies like "1", "2") */
    char            choices[4][256];
    int             n_choices;
    bool            has_choices;
} g_gw_clarify = {false, "", "", "", "", "", PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0, {{0}}, 0, false};

/* Register a platform poll function to use during clarify wait */
void gw_clarify_set_poll(char *(*fn)(const char *chat_id), int interval_sec) {
    g_gw_clarify.poll_fn = fn;
    g_gw_clarify.poll_interval = interval_sec > 0 ? interval_sec : 1;
}

/* Begin waiting for clarify response — must be called before gw_clarify_wait_response.
   Sets the platform, chat_id, session_key context and marks pending. Thread-safe. */
void gw_clarify_begin(const char *platform, const char *chat_id,
                      const char *session_key, const char *clarify_id,
                      const char (*choices)[256], int n_choices) {
    pthread_mutex_lock(&g_gw_clarify.mutex);
    g_gw_clarify.pending = true;
    g_gw_clarify.response[0] = '\0';
    snprintf(g_gw_clarify.platform, sizeof(g_gw_clarify.platform), "%s", platform ? platform : "");
    snprintf(g_gw_clarify.chat_id, sizeof(g_gw_clarify.chat_id), "%s", chat_id ? chat_id : "");
    snprintf(g_gw_clarify.session_key, sizeof(g_gw_clarify.session_key), "%s", session_key ? session_key : "");
    snprintf(g_gw_clarify.clarify_id, sizeof(g_gw_clarify.clarify_id), "%s", clarify_id ? clarify_id : "");
    g_gw_clarify.n_choices = 0;
    g_gw_clarify.has_choices = (choices != NULL && n_choices > 0);
    if (choices && n_choices > 0) {
        for (int i = 0; i < n_choices && i < 4; i++) {
            snprintf(g_gw_clarify.choices[i], sizeof(g_gw_clarify.choices[i]), "%s", choices[i]);
            g_gw_clarify.n_choices = i + 1;
        }
    }
    pthread_mutex_unlock(&g_gw_clarify.mutex);
}

/* Internal: check if a message matches pending clarify and capture response.
   Returns true if consumed. Caller must hold g_gw_clarify.mutex. */
static bool gw_clarify_match(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text) return false;
    if (!g_gw_clarify.pending) return false;
    if (strcmp(g_gw_clarify.platform, platform) != 0 ||
        strcmp(g_gw_clarify.chat_id, chat_id) != 0) return false;

    const char *trimmed = text;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    /* If we have choices, accept a number (1-4) as a choice selection */
    if (g_gw_clarify.has_choices && trimmed[0] >= '1' && trimmed[0] <= '4' &&
        (trimmed[1] == '\0' || trimmed[1] == ' ' || trimmed[1] == '\t')) {
        int idx = trimmed[0] - '1';
        if (idx >= 0 && idx < g_gw_clarify.n_choices) {
            snprintf(g_gw_clarify.response, sizeof(g_gw_clarify.response), "%s", g_gw_clarify.choices[idx]);
            g_gw_clarify.pending = false;
            pthread_cond_signal(&g_gw_clarify.cond);
            printf("[gateway] Clarify response from %s/%s: %s (choice %d)\n", platform, chat_id, g_gw_clarify.response, idx + 1);
            return true;
        }
    }

    /* Any non-empty text is a valid clarify response (open-ended or free-form) */
    snprintf(g_gw_clarify.response, sizeof(g_gw_clarify.response), "%s", trimmed);
    g_gw_clarify.pending = false;
    pthread_cond_signal(&g_gw_clarify.cond);
    printf("[gateway] Clarify response from %s/%s: %.80s\n", platform, chat_id, trimmed);
    return true;
}

/* Check if an incoming message is a clarify response.
   Called from platform message handler threads.
   Returns true if consumed. */
static bool gw_clarify_check_response(const char *platform, const char *chat_id,
                                        const char *text) {
    pthread_mutex_lock(&g_gw_clarify.mutex);
    if (!g_gw_clarify.pending) {
        pthread_mutex_unlock(&g_gw_clarify.mutex);
        return false;
    }
    if (strcmp(g_gw_clarify.platform, platform) != 0 ||
        strcmp(g_gw_clarify.chat_id, chat_id) != 0) {
        pthread_mutex_unlock(&g_gw_clarify.mutex);
        return false;
    }
    bool consumed = gw_clarify_match(platform, chat_id, text);
    pthread_mutex_unlock(&g_gw_clarify.mutex);
    return consumed;
}

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
static struct {
    bool            pending;
    char            platform[32];
    char            chat_id[128];
    char            response[64];
    pthread_mutex_t mutex;
    pthread_cond_t  cond;   /* signaled when response received */
    /* Optional: poll function for same-platform response capture.
       Polls for a message from chat_id, returns response text or NULL.
       Must free returned string. */
    char *(*poll_fn)(const char *chat_id);
    int           poll_interval;
} g_gw_approval = {false, "", "", "", PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0};

/* Register a platform poll function to use during approval wait */
void gw_approval_set_poll(char *(*fn)(const char *chat_id), int interval_sec) {
    g_gw_approval.poll_fn = fn;
    g_gw_approval.poll_interval = interval_sec > 0 ? interval_sec : 1;
}

/* Set context for pending approval — called from approval.c */
void gw_approval_set_context(const char *platform, const char *chat_id) {
    pthread_mutex_lock(&g_gw_approval.mutex);
    snprintf(g_gw_approval.platform, sizeof(g_gw_approval.platform), "%s", platform ? platform : "");
    snprintf(g_gw_approval.chat_id, sizeof(g_gw_approval.chat_id), "%s", chat_id ? chat_id : "");
    pthread_mutex_unlock(&g_gw_approval.mutex);
}

/* Begin waiting for approval response — must be called before gw_approval_wait_response.
   Sets the platform, chat_id context and marks pending. Thread-safe. */
void gw_approval_begin(const char *platform, const char *chat_id) {
    pthread_mutex_lock(&g_gw_approval.mutex);
    g_gw_approval.pending = true;
    g_gw_approval.response[0] = '\0';
    snprintf(g_gw_approval.platform, sizeof(g_gw_approval.platform), "%s", platform ? platform : "");
    snprintf(g_gw_approval.chat_id, sizeof(g_gw_approval.chat_id), "%s", chat_id ? chat_id : "");
    pthread_mutex_unlock(&g_gw_approval.mutex);
}

/* Internal: check if a message matches pending approval and capture response.
   Returns true if consumed. Caller must hold g_gw_approval.mutex. */
static bool gw_approval_match(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text) return false;

    const char *trimmed = text;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    /* Short responses only: y, n, a, yes, no, always */
    if (strlen(trimmed) > 16) return false;

    char lower[64];
    snprintf(lower, sizeof(lower), "%s", trimmed);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* Only consume if it looks like an approval response */
    if (lower[0] != 'y' && lower[0] != 'n' && lower[0] != 'a') return false;

    snprintf(g_gw_approval.response, sizeof(g_gw_approval.response), "%s", lower);
    g_gw_approval.pending = false;
    pthread_cond_signal(&g_gw_approval.cond);
    printf("[gateway] Approval response from %s/%s: %s\n", platform, chat_id, trimmed);
    return true;
}

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
static bool gw_approval_check_response(const char *platform, const char *chat_id,
                                         const char *text) {
    pthread_mutex_lock(&g_gw_approval.mutex);
    if (!g_gw_approval.pending) {
        pthread_mutex_unlock(&g_gw_approval.mutex);
        return false;
    }
    if (strcmp(g_gw_approval.platform, platform) != 0 ||
        strcmp(g_gw_approval.chat_id, chat_id) != 0) {
        pthread_mutex_unlock(&g_gw_approval.mutex);
        return false;
    }
    bool consumed = gw_approval_match(platform, chat_id, text);
    pthread_mutex_unlock(&g_gw_approval.mutex);
    return consumed;
}

/* ================================================================
 *  Gateway stderr log-to-file with rotation (B15)
 * ================================================================ */

#define GW_LOG_MAX_BYTES (10 * 1024 * 1024)  /* 10 MB before rotation */
#define GW_LOG_PATH_MAX 512

static FILE *g_gw_log_fp = NULL;
static char  g_gw_log_path[GW_LOG_PATH_MAX] = {0};

/** Open gateway log file, rotate if >10 MB, for persistent log capture. */
static void gw_log_open(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;

    snprintf(g_gw_log_path, sizeof(g_gw_log_path),
             "%s/.slermes/logs/gateway.log", home);

    struct stat st;
    if (stat(g_gw_log_path, &st) == 0 && st.st_size > GW_LOG_MAX_BYTES) {
        char old[GW_LOG_PATH_MAX];
        snprintf(old, sizeof(old), "%s.1", g_gw_log_path);
        rename(g_gw_log_path, old);
    }

    g_gw_log_fp = fopen(g_gw_log_path, "a");
}

static void gw_log_close(void) {
    if (g_gw_log_fp) { fclose(g_gw_log_fp); g_gw_log_fp = NULL; }
}

/* ================================================================
 *  P101: Rate limiter (token bucket)
 * ================================================================ */

void gw_rate_limit_init(int idx, double tokens_per_sec, double max_burst) {
    if (idx < 0 || idx >= GW_MAX_PLATFORMS) return;
    g_gw.rate_limiters[idx].tokens_per_sec = tokens_per_sec;
    g_gw.rate_limiters[idx].max_tokens = max_burst;
    g_gw.rate_limiters[idx].tokens = max_burst;
    g_gw.rate_limiters[idx].last_refill = gw_mono_time();
}

bool gw_rate_limit_check(int idx) {
    if (idx < 0 || idx >= GW_MAX_PLATFORMS) return true; /* no limit if out of range */

    gw_rate_limiter_t *rl = &g_gw.rate_limiters[idx];
    double now = gw_mono_time();

    /* Refill tokens based on elapsed time */
    double elapsed = now - rl->last_refill;
    rl->tokens += elapsed * rl->tokens_per_sec;
    if (rl->tokens > rl->max_tokens)
        rl->tokens = rl->max_tokens;
    rl->last_refill = now;

    if (rl->tokens >= 1.0) {
        rl->tokens -= 1.0;
        return true; /* allowed */
    }
    return false; /* rate-limited */
}

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

void gw_pool_return_client(http_client_t *client, const char *endpoint) {
    if (!client) return;

    pthread_mutex_lock(&g_gw.pool_mutex);

    for (int i = 0; i < g_gw.pool_count; i++) {
        if (g_gw.http_pool[i].client == client) {
            g_gw.http_pool[i].in_use = false;
            g_gw.http_pool[i].last_used = gw_mono_time();
            pthread_mutex_unlock(&g_gw.pool_mutex);
            return;
        }
    }

    /* Not found in pool — free it */
    pthread_mutex_unlock(&g_gw.pool_mutex);
    http_client_free(client);
}

void gw_pool_cleanup(void) {
    pthread_mutex_lock(&g_gw.pool_mutex);
    double now = gw_mono_time();
    double expiry = g_gw.pool_keepalive_expiry > 0 ? g_gw.pool_keepalive_expiry : 300.0;
    for (int i = 0; i < g_gw.pool_count; i++) {
        if (!g_gw.http_pool[i].in_use &&
            (now - g_gw.http_pool[i].last_used) > expiry) {
            http_client_free(g_gw.http_pool[i].client);
            if (i < g_gw.pool_count - 1) {
                g_gw.http_pool[i] = g_gw.http_pool[g_gw.pool_count - 1];
            }
            g_gw.pool_count--;
            i--;
        }
    }
    pthread_mutex_unlock(&g_gw.pool_mutex);
}

/* ================================================================
 *  E27: HTTP keepalive per-platform (set via config)
 * ================================================================ */

void gw_set_keepalive(int plat_idx, double keepalive_sec) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS)
        g_gw.platform_keepalive_sec[plat_idx] = keepalive_sec;
}

/* E28: Message deduplication (TTL-based ring buffer) */
/* Forward declaration for process_update (defined below) */
static void process_update(const char *platform, const char *chat_id, const char *text);

bool gw_dedup_check(const char *message_id) {
    if (!message_id || !*message_id) return false;
    double now = gw_mono_time();

    /* Prune expired entries */
    while (g_gw.dedup_count > 0 &&
           (now - g_gw.dedup_timestamps[g_gw.dedup_head]) > g_gw.dedup_ttl) {
        g_gw.dedup_head = (g_gw.dedup_head + 1) % 64;
        g_gw.dedup_count--;
    }

    /* Linear scan for match (small ring, <64 entries) */
    for (int i = 0; i < g_gw.dedup_count; i++) {
        int idx = (g_gw.dedup_head + i) % 64;
        if (strcmp(g_gw.dedup_ids[idx], message_id) == 0)
            return true; /* duplicate */
    }
    return false;
}

void gw_dedup_add(const char *message_id) {
    if (!message_id || !*message_id) return;
    if (g_gw.dedup_count >= 64) return; /* ring full, skip */

    int idx = (g_gw.dedup_head + g_gw.dedup_count) % 64;
    snprintf(g_gw.dedup_ids[idx], sizeof(g_gw.dedup_ids[idx]), "%s", message_id);
    g_gw.dedup_timestamps[idx] = gw_mono_time();
    g_gw.dedup_count++;
}

/* ================================================================
 *  E29: Batch aggregation — coalesce fragmented messages
 * ================================================================ */

void gw_batch_accumulate(const char *platform, const char *chat_id, const char *fragment) {
    if (!platform || !chat_id || !fragment) return;

    double now = gw_mono_time();
    double BATCH_TIMEOUT = 2.0; /* seconds to wait for more fragments */

    /* If no active batch or different source, flush first */
    if (g_gw.batch_active &&
        (strcmp(g_gw.batch_platform, platform) != 0 ||
         strcmp(g_gw.batch_chat_id, chat_id) != 0 ||
         (now - g_gw.batch_start_time) > BATCH_TIMEOUT)) {
        gw_batch_flush();
    }

    /* Start or continue batch */
    if (!g_gw.batch_active) {
        snprintf(g_gw.batch_platform, sizeof(g_gw.batch_platform), "%s", platform);
        snprintf(g_gw.batch_chat_id, sizeof(g_gw.batch_chat_id), "%s", chat_id);
        g_gw.batch_buf[0] = '\0';
        g_gw.batch_start_time = now;
        g_gw.batch_active = true;
    }

    size_t remaining = sizeof(g_gw.batch_buf) - strlen(g_gw.batch_buf) - 1;
    if (remaining > 0) {
        strncat(g_gw.batch_buf, fragment, remaining);
    }
}

void gw_batch_flush(void) {
    if (!g_gw.batch_active) return;
    if (g_gw.batch_buf[0]) {
        process_update(g_gw.batch_platform, g_gw.batch_chat_id, g_gw.batch_buf);
    }
    g_gw.batch_buf[0] = '\0';
    g_gw.batch_active = false;
}

/* ================================================================
 *  E30: Markdown stripping per-platform
 * ================================================================ */

static char *gw_strip_markdown(const char *text, bool strip_code, bool strip_bold,
                                bool strip_italic) {
    if (!text) return NULL;
    /* Simple in-place markdown stripping. Allocates for worst case. */
    char *out = (char *)malloc(strlen(text) + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '`' && strip_code) continue;
        if (text[i] == '*' && strip_bold) {
            /* Skip ** */
            if (text[i+1] == '*') i++;
            continue;
        }
        if (text[i] == '_' && strip_italic) continue;
        if (text[i] == '~' && text[i+1] == '~') { i++; continue; } /* strikethrough ~~ */
        if (text[i] == '#' && (i == 0 || text[i-1] == '\n')) continue; /* headers */
        if (text[i] == '>') { /* block quotes */
            if (i == 0 || text[i-1] == '\n') continue;
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  E31: Per-platform cooldown
 * ================================================================ */

double gw_cooldown_remaining(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return 0.0;
    double remaining = g_gw.platform_cooldown_sec[plat_idx] -
        (gw_mono_time() - g_gw.platform_last_action[plat_idx]);
    return remaining > 0.0 ? remaining : 0.0;
}

void gw_cooldown_mark(int plat_idx) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS)
        g_gw.platform_last_action[plat_idx] = gw_mono_time();
}

/* ================================================================
 *  E32: Reconnect backoff (exponential with jitter)
 * ================================================================ */

double gw_reconnect_delay(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return GW_RECONNECT_BASE_SEC;

    g_gw.reconnect_attempt[plat_idx]++;

    /* Exponential: base * 2 ^ (attempt - 1) with jitter */
    double base = GW_RECONNECT_BASE_SEC *
        (1 << (g_gw.reconnect_attempt[plat_idx] - 1));
    if (base > GW_RECONNECT_MAX_SEC) base = GW_RECONNECT_MAX_SEC;

    /* Add random jitter ±10% */
    double jitter = ((double)rand() / RAND_MAX) * 2.0 * GW_RECONNECT_JITTER * base
        - GW_RECONNECT_JITTER * base;
    double delay = base + jitter;
    if (delay < GW_RECONNECT_BASE_SEC) delay = GW_RECONNECT_BASE_SEC;

    g_gw.reconnect_delay_sec[plat_idx] = delay;
    return delay;
}

void gw_reconnect_reset(int plat_idx) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS) {
        g_gw.reconnect_attempt[plat_idx] = 0;
        g_gw.reconnect_delay_sec[plat_idx] = 0.0;
    }
}

/* ================================================================
 *  E33: Proxy support per-platform
 * ================================================================ */

bool gw_set_proxy(int plat_idx, const char *proxy_url) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return false;
    if (!proxy_url || !*proxy_url) {
        g_gw.proxy_enabled[plat_idx] = false;
        g_gw.platform_proxy[plat_idx][0] = '\0';
        return true;
    }
    snprintf(g_gw.platform_proxy[plat_idx], sizeof(g_gw.platform_proxy[plat_idx]),
             "%s", proxy_url);
    g_gw.proxy_enabled[plat_idx] = true;
    return true;
}

/* ================================================================
 *  E34: Group observe — observe unmentioned group messages
 * ================================================================ */

/* Forward declarations for functions defined later */
static void gateway_send(const char *platform, const char *target, const char *text);
static void gateway_send_fallback(const char *platform, const char *target,
                                   const char *text);

void gw_set_group_observe(const char *prefix, bool enabled) {
    if (prefix)
        snprintf(g_gw.group_observe_prefix, sizeof(g_gw.group_observe_prefix), "%s", prefix);
    g_gw.group_observe_enabled = enabled;
}

/* L08: Append message to observe buffer (thread-safe, rolling). */
void gw_observe_append(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text || !*text) return;
    pthread_mutex_lock(&g_gw.observe_mutex);
    size_t cur = strlen(g_gw.observe_buffer);
    size_t add = strlen(platform) + 1 + strlen(chat_id) + 2 + strlen(text) + 3;
    if (cur + add >= sizeof(g_gw.observe_buffer)) {
        /* Buffer full — trim from front */
        char *nl = strchr(g_gw.observe_buffer, '\n');
        if (nl) {
            size_t remain = strlen(nl + 1);
            memmove(g_gw.observe_buffer, nl + 1, remain + 1);
            cur = remain;
        } else {
            g_gw.observe_buffer[0] = '\0';
            cur = 0;
        }
    }
    char entry[2048];
    snprintf(entry, sizeof(entry), "[%s:%s] %s\n", platform, chat_id, text);
    strncat(g_gw.observe_buffer, entry,
            sizeof(g_gw.observe_buffer) - strlen(g_gw.observe_buffer) - 1);
    pthread_mutex_unlock(&g_gw.observe_mutex);
}

/* L08: Consume and clear observe buffer for a given platform+chat. */
char *gw_observe_consume(const char *platform, const char *chat_id) {
    if (!platform || !chat_id) return NULL;
    pthread_mutex_lock(&g_gw.observe_mutex);
    if (g_gw.observe_buffer[0] == '\0') {
        pthread_mutex_unlock(&g_gw.observe_mutex);
        return NULL;
    }
    char *result = strdup(g_gw.observe_buffer);
    g_gw.observe_buffer[0] = '\0';
    pthread_mutex_unlock(&g_gw.observe_mutex);
    return result;
}

/* ================================================================
 *  E35-E39: Gateway hooks/middleware system
 * ================================================================ */

/* Hook function types */
typedef json_node_t *(*gw_hook_t)(json_node_t *data, void *userdata);

#define GW_HOOKS_MAX 16

static struct {
    gw_hook_t pre_send[GW_HOOKS_MAX];      /* E35: transform outgoing messages */
    void     *pre_send_data[GW_HOOKS_MAX];
    int       pre_send_count;

    gw_hook_t post_receive[GW_HOOKS_MAX];  /* E36: process incoming */
    void     *post_receive_data[GW_HOOKS_MAX];
    int       post_receive_count;

    gw_hook_t interceptor[GW_HOOKS_MAX];   /* E37: censor/modify in transit */
    void     *interceptor_data[GW_HOOKS_MAX];
    int       interceptor_count;
} gw_hooks;

void gw_register_pre_send(gw_hook_t hook, void *userdata) {
    if (gw_hooks.pre_send_count >= GW_HOOKS_MAX) return;
    gw_hooks.pre_send[gw_hooks.pre_send_count] = hook;
    gw_hooks.pre_send_data[gw_hooks.pre_send_count] = userdata;
    gw_hooks.pre_send_count++;
}

void gw_register_post_receive(gw_hook_t hook, void *userdata) {
    if (gw_hooks.post_receive_count >= GW_HOOKS_MAX) return;
    gw_hooks.post_receive[gw_hooks.post_receive_count] = hook;
    gw_hooks.post_receive_data[gw_hooks.post_receive_count] = userdata;
    gw_hooks.post_receive_count++;
}

void gw_register_interceptor(gw_hook_t hook, void *userdata) {
    if (gw_hooks.interceptor_count >= GW_HOOKS_MAX) return;
    gw_hooks.interceptor[gw_hooks.interceptor_count] = hook;
    gw_hooks.interceptor_data[gw_hooks.interceptor_count] = userdata;
    gw_hooks.interceptor_count++;
}

/* E38: Event bus — broadcast a JSON event to all registered listeners */
#define GW_EVENT_LISTENERS_MAX 16

typedef void (*gw_event_listener_t)(const char *event_type, json_node_t *data, void *userdata);

static struct {
    gw_event_listener_t listeners[GW_EVENT_LISTENERS_MAX];
    void               *data[GW_EVENT_LISTENERS_MAX];
    int                 count;
} gw_event_bus;

void gw_event_register(gw_event_listener_t listener, void *userdata) {
    if (gw_event_bus.count >= GW_EVENT_LISTENERS_MAX) return;
    gw_event_bus.listeners[gw_event_bus.count] = listener;
    gw_event_bus.data[gw_event_bus.count] = userdata;
    gw_event_bus.count++;
}

void gw_event_emit(const char *event_type, json_node_t *data) {
    for (int i = 0; i < gw_event_bus.count; i++) {
        gw_event_bus.listeners[i](event_type, data, gw_event_bus.data[i]);
    }
}

/* E35: Apply pre-send hooks to a message before sending */
static char *gw_apply_pre_send_hooks(const char *platform, const char *text) {
    if (!text) return NULL;

    json_node_t *data = json_new_object();
    json_object_set(data, "platform", json_new_string(platform));
    json_object_set(data, "text", json_new_string(text));

    for (int i = 0; i < gw_hooks.pre_send_count; i++) {
        json_node_t *result = gw_hooks.pre_send[i](data, gw_hooks.pre_send_data[i]);
        if (result) {
            const char *new_text = json_object_get_string(result, "text", NULL);
            if (new_text) {
                json_object_set(data, "text", json_new_string(new_text));
            }
            json_free(result);
        }
    }

    const char *final_text = json_object_get_string(data, "text", "");
    char *out = strdup(final_text);
    json_free(data);
    return out;
}

/* E36: Apply post-receive hooks on incoming message */
static char *gw_apply_post_receive_hooks(const char *platform, const char *chat_id,
                                          const char *text) {
    if (!text) return NULL;

    json_node_t *data = json_new_object();
    json_object_set(data, "platform", json_new_string(platform));
    json_object_set(data, "chat_id", json_new_string(chat_id));
    json_object_set(data, "text", json_new_string(text));

    for (int i = 0; i < gw_hooks.post_receive_count; i++) {
        json_node_t *result = gw_hooks.post_receive[i](data, gw_hooks.post_receive_data[i]);
        if (result) {
            const char *new_text = json_object_get_string(result, "text", NULL);
            if (new_text)
                json_object_set(data, "text", json_new_string(new_text));
            json_free(result);
        }
    }

    const char *final_text = json_object_get_string(data, "text", "");
    char *out = strdup(final_text);
    json_free(data);
    return out;
}

/* E37: Apply interceptors — can return NULL to drop message */
static char *gw_apply_interceptors(const char *platform, const char *chat_id,
                                    const char *text) {
    if (!text) return NULL;

    json_node_t *data = json_new_object();
    json_object_set(data, "platform", json_new_string(platform));
    json_object_set(data, "chat_id", json_new_string(chat_id));
    json_object_set(data, "text", json_new_string(text));

    for (int i = 0; i < gw_hooks.interceptor_count; i++) {
        json_node_t *result = gw_hooks.interceptor[i](data, gw_hooks.interceptor_data[i]);
        if (!result) {
            /* Interceptor dropped the message */
            json_free(data);
            return NULL;
        }
        const char *new_text = json_object_get_string(result, "text", NULL);
        if (new_text)
            json_object_set(data, "text", json_new_string(new_text));
        json_free(result);
    }

    const char *final_text = json_object_get_string(data, "text", "");
    char *out = strdup(final_text);
    json_free(data);
    return out;
}

/* E39: Cooldown manager — enforce min interval between sends */
__attribute__((unused)) static bool gw_cooldown_allow(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return true;
    double remaining = gw_cooldown_remaining(plat_idx);
    if (remaining > 0.0) return false;
    gw_cooldown_mark(plat_idx);
    return true;
}

/* ================================================================
 *  E40-E43: Gateway message formatting
 * ================================================================ */

/* Port of Python gateway/platforms/matrix.py:_markdown_to_html(). */
/* E40: Convert markdown to HTML for platforms that support it.
 * Simple conversion: **bold** → <b>bold</b>, *italic* → <i>italic</i>,
 * `code` → <code>code</code> */
char *gw_markdown_to_html(const char *text) {
    if (!text) return NULL;
    char *out = (char *)malloc(strlen(text) * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '*' && text[i+1] == '*') {
            out[j++] = '<'; out[j++] = 'b'; out[j++] = '>';
            i++;
            while (text[i+1] && !(text[i+1] == '*' && text[i+2] == '*')) {
                out[j++] = text[++i];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'b'; out[j++] = '>';
            i += 2;
        } else if (text[i] == '*' && text[i+1] != '*') {
            out[j++] = '<'; out[j++] = 'i'; out[j++] = '>';
            i++;
            while (text[i] && text[i] != '*') {
                out[j++] = text[i++];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'i'; out[j++] = '>';
        } else if (text[i] == '`') {
            out[j++] = '<'; out[j++] = 'c'; out[j++] = 'o'; out[j++] = 'd';
            out[j++] = 'e'; out[j++] = '>';
            i++;
            while (text[i] && text[i] != '`') {
                if (text[i] == '\\' && text[i+1] == '`') i++;
                out[j++] = text[i++];
            }
            out[j++] = '<'; out[j++] = '/'; out[j++] = 'c'; out[j++] = 'o';
            out[j++] = 'd'; out[j++] = 'e'; out[j++] = '>';
        } else {
            /* Escape HTML entities */
            if (text[i] == '<') { out[j++] = '&'; out[j++] = 'l'; out[j++] = 't'; out[j++] = ';'; }
            else if (text[i] == '>') { out[j++] = '&'; out[j++] = 'g'; out[j++] = 't'; out[j++] = ';'; }
            else if (text[i] == '&') { out[j++] = '&'; out[j++] = 'a'; out[j++] = 'm'; out[j++] = 'p'; out[j++] = ';'; }
            else out[j++] = text[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* E41: Telegram MarkdownV2 escaping — escape reserved chars */
char *gw_markdown_v2_escape(const char *text) {
    if (!text) return NULL;
    char *out = (char *)malloc(strlen(text) * 2 + 1);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; text[i]; i++) {
        /* Characters that need escaping in MarkdownV2: _ * [ ] ( ) ~ ` > # + - = | { } . ! */
        if (strchr("_*[]()~`>#+-=|{}.!", text[i])) {
            out[j++] = '\\';
        }
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* E42: Strip all formatting for plain text platforms */
static char *gw_strip_all_formatting(const char *text) {
    return gw_strip_markdown(text, true, true, true);
}

/* E43: Smart message truncation with ellipsis.
 * Truncates at word boundary if possible. */
char *gw_truncate_message(const char *text, size_t max_len) {
    if (!text || max_len == 0) return NULL;
    size_t len = strlen(text);
    if (len <= max_len) return strdup(text);

    char *out = (char *)malloc(max_len + 4);
    if (!out) return NULL;
    memcpy(out, text, max_len);

    /* Try to break at word boundary (space) */
    int break_at = (int)max_len;
    while (break_at > 0 && out[break_at - 1] != ' ') break_at--;

    if (break_at > (int)max_len / 2) {
        out[break_at] = '\0';
        strcat(out, "...");
    } else {
        out[max_len] = '\0';
        strcat(out, "...");
    }
    return out;
}


/* E44: Retry an API call with exponential backoff on 429/5xx.
 * Returns true if at least one attempt succeeded. */
bool gw_retry_with_backoff(bool (*api_call)(void *ctx), void *ctx,
                                   int max_retries, int base_delay_ms) {
    for (int attempt = 0; attempt <= max_retries; attempt++) {
        if (api_call(ctx)) return true;
        if (attempt < max_retries) {
            int delay = base_delay_ms * (1 << attempt); /* exponential */
            /* Add jitter ±20% */
            delay += (int)(((double)rand() / RAND_MAX) * 2.0 * 0.2 * delay - 0.2 * delay);
            usleep(delay * 1000);
        }
    }
    return false;
}

/* E45: Token refresh — re-init platform when token expires.
 * Checks platform state and re-runs setup. */
bool gw_refresh_token(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return false;
    /* Re-initialize the platform's HTTP client */
    if (g_gw.platform_http[plat_idx]) {
        http_client_free(g_gw.platform_http[plat_idx]);
    }
    g_gw.platform_http[plat_idx] = http_client_new(30);
    /* Apply proxy if configured */
    if (g_gw.proxy_enabled[plat_idx] && g_gw.platform_proxy[plat_idx][0]) {
        http_client_set_proxy(g_gw.platform_http[plat_idx],
                              g_gw.platform_proxy[plat_idx]);
    }
    gw_reconnect_reset(plat_idx);
    return true;
}

/* E47: Send a plain text fallback when rich formatting fails */
static void gateway_send_fallback(const char *platform, const char *target,
                                   const char *text) {
    if (!platform || !target || !text) return;
    /* Strip all formatting and truncate */
    char *plain = gw_strip_all_formatting(text);
    char *truncated = gw_truncate_message(plain ? plain : text, 4000);
    if (truncated) {
        gateway_send(platform, target, truncated);
        free(truncated);
    }
    free(plain);
}

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
static int session_find(const char *platform, const char *chat_id);

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

/* Populate session source struct with the given values.
 * Strings are truncated to fit their fixed-size fields.
 * v306: added chat_topic, user_id_alt, chat_id_alt, guild_id, parent_chat_id, message_id */
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

/* GW15: forward declarations for static LRU cache functions */
static gw_session_source_t *source_cache_get(const char *key);
static void source_cache_put(const char *key, const gw_session_source_t *source);

/* Thread-safe: set session source metadata for an existing session.
 * Platform threads call this after session_get_or_create() when they
 * have the metadata (chat_name, user_id, etc.) from their poll data.
 * Returns true if session was found and updated. */
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

/* ================================================================
 * GW15: Session sources LRU cache
 * Mirrors Python gateway/run.py _session_sources OrderedDict.
 * Fixed-size array: MRU at high indices, LRU at low indices.
 * ================================================================ */

/* Look up a source by key. Returns pointer to cached entry or NULL.
 * On hit, moves the entry to MRU position (swap to end of occupied range). */
static gw_session_source_t *source_cache_get(const char *key) {
    if (!key) return NULL;
    pthread_mutex_lock(&g_gw.source_cache_mutex);
    int i;
    for (i = 0; i < g_gw.source_cache_count; i++) {
        if (g_gw.source_cache[i].occupied &&
            strcmp(g_gw.source_cache[i].key, key) == 0) {
            /* Move to MRU: swap with last occupied slot */
            if (i != g_gw.source_cache_count - 1) {
                /* Find last occupied slot */
                int last = g_gw.source_cache_count - 1;
                /* Swap */
                struct { char key[192]; gw_session_source_t source; bool occupied; }
                    tmp = {0};
                memcpy(&tmp, &g_gw.source_cache[i], sizeof(tmp));
                memcpy(&g_gw.source_cache[i], &g_gw.source_cache[last], sizeof(tmp));
                memcpy(&g_gw.source_cache[last], &tmp, sizeof(tmp));
                i = last;
            }
            gw_session_source_t *result = &g_gw.source_cache[i].source;
            pthread_mutex_unlock(&g_gw.source_cache_mutex);
            return result;
        }
    }
    pthread_mutex_unlock(&g_gw.source_cache_mutex);
    return NULL;
}

/* Insert or update a source in the cache. Evicts LRU entry if full. */
static void source_cache_put(const char *key, const gw_session_source_t *source) {
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
        source_cache_put(key, &g_gw.sessions[idx].source);
        pthread_mutex_unlock(&g_gw.session_mutex);
        /* Return from cache (now MRU) */
        return source_cache_get(key);
    }
    pthread_mutex_unlock(&g_gw.session_mutex);
    return NULL;
}

/* PII-safe hash helper: deterministic 8-char hex via FNV-1a */
static void pii_hash(const char *input, char *out, size_t out_sz) {
    if (!input || !*input) { out[0] = '\0'; return; }
    uint32_t hash = 2166136261u;
    while (*input) {
        hash ^= (unsigned char)*input++;
        hash *= 16777619u;
    }
    snprintf(out, out_sz, "%08x", hash);
}

/* Port of Python gateway/session.py:_discord_tools_loaded
 * Returns true when Discord tools (discord/discord_admin) are available.
 * Checks: DISCORD_BOT_TOKEN in env + discord in gateway_platforms. */
static bool discord_tools_loaded(void) {
    const char *token = getenv("DISCORD_BOT_TOKEN");
    if (!token || !token[0]) return false;
    for (int i = 0; i < g_gw.platform_count; i++) {
        if (strcasecmp(g_gw.platforms[i], "discord") == 0)
            return true;
    }
    return false;
}

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
static bool session_should_reset(double session_sec) {
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

/* Free a session entry (save, close DB, free agent, zero out).
 * Does NOT update session_count. */
static void session_free(int idx) {
    if (idx < 0 || idx >= GW_SESSIONS_MAX) return;
    if (!g_gw.sessions[idx].in_use) return;
    if (g_gw.sessions[idx].db) {
        db_save(g_gw.sessions[idx].db, g_gw.sessions[idx].session_id, NULL);
        db_close(g_gw.sessions[idx].db);
    }
    agent_free(&g_gw.sessions[idx].agent);
    memset(&g_gw.sessions[idx], 0, sizeof(g_gw.sessions[idx]));
}

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
static bool __attribute__((unused)) is_shared_multi_user_session(const gw_session_source_t *src,
                                          bool group_sessions_per_user,
                                          bool thread_sessions_per_user) {
    if (!src) return false;
    if (strcmp(src->chat_type, "dm") == 0) return false;
    if (src->thread_id[0]) return !thread_sessions_per_user;
    return !group_sessions_per_user;
}

/* Port of Python gateway/session.py:build_session_key(). */
static void build_session_key(char *buf, size_t sz,
                               const char *platform, const char *chat_id) {
    snprintf(buf, sz, "%s:%s", platform ? platform : "?", chat_id ? chat_id : "?");
}

/* Find existing session entry by platform+chat_id. Returns index or -1. */
static int session_find(const char *platform, const char *chat_id) {
    char key[192];
    build_session_key(key, sizeof(key), platform, chat_id);
    for (int i = 0; i < g_gw.session_count; i++) {
        if (strcmp(g_gw.sessions[i].key, key) == 0 && g_gw.sessions[i].in_use)
            return i;
    }
    return -1;
}

/* Port of Python agent/auxiliary_client.py:create(). */
/* Create a new session for a platform:chat_id pair. Returns index or -1. */
static int session_create(const char *platform, const char *chat_id) {
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

/* Get or create a session for platform:chat_id. Returns index or -1. */
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

/* Auto-save all active sessions */
static void session_save_all(void) {
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use && g_gw.sessions[i].db) {
            agent_save_session(&g_gw.sessions[i].agent);
        }
    }
}

/* Clean up expired sessions based on configured reset policy.
 * Called by cleanup thread. Replaces hardcoded 30-min idle TTL. */
static void session_cleanup_idle(void) {
    double now = gw_mono_time();
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use) {
            double idle = now - g_gw.sessions[i].last_active;
            if (session_should_reset(idle))
                session_free(i);
        }
    }
}

/* ================================================================
 *  P186: MEDIA: prefix handling — route file paths to platform media APIs
 * ================================================================ */

/* Try to send a file via MEDIA: prefix. Returns true if handled. */
static bool gw_try_send_media(const char *platform, const char *target, const char *text) {
    if (!text || strncmp(text, "MEDIA:", 6) != 0) return false;

    const char *path = text + 6;
    if (!path[0]) return false;

    /* Determine file type from extension */
    const char *ext = strrchr(path, '.');
    if (!ext) return false;

    /* Check if file exists */
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return false;

    if (strcmp(platform, "telegram") == 0) {
        /* Image extensions */
        if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".webp") == 0) {
            return telegram_send_photo(g_gw.http, target, path, NULL, NULL);
        }
        /* Audio extensions */
        if (strcasecmp(ext, ".ogg") == 0 || strcasecmp(ext, ".opus") == 0) {
            return telegram_send_voice(g_gw.http, target, path, NULL, NULL);
        }
        /* Video extensions */
        if (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".mov") == 0 ||
            strcasecmp(ext, ".avi") == 0 || strcasecmp(ext, ".mkv") == 0) {
            return telegram_send_video(g_gw.http, target, path, NULL, NULL);
        }
        /* GIF/animation */
        if (strcasecmp(ext, ".gif") == 0) {
            return telegram_send_animation(g_gw.http, target, path, NULL, NULL);
        }
        /* Default: send as document */
        return telegram_send_document(g_gw.http, target, path, NULL, NULL);
    }

    /* For platforms without media APIs, fall back to text with path info */
    return false;
}

/* ================================================================
 *  Platform-aware message send
 * ================================================================ */

static void gateway_send(const char *platform, const char *target, const char *text) {
    if (!platform || !target || !text) return;

    /* Apply registered message interceptors */
    char *intercepted = gw_apply_interceptors(platform, target, text);
    const char *send_text = intercepted ? intercepted : text;

    /* P186: Try MEDIA: prefix for file/media sends */
    if (gw_try_send_media(platform, target, send_text)) {
        free(intercepted);
        return;
    }

    /* P103: Try registered platform interface first */
    if (gw_platform_send(platform, target, send_text)) {
        free(intercepted);
        return;
    }

    /* Legacy fallback for unregistered platforms */
    bool sent = false;
    if (strcmp(platform, "telegram") == 0) {
        size_t len = strlen(send_text);
        if (len > 4000) {
            char chunk[4001];
            memcpy(chunk, send_text, 4000);
            chunk[4000] = '\0';
            telegram_send_message(g_gw.http, target, chunk, "Markdown", NULL, false, false, NULL);
            if (len > 4000)
                telegram_send_message(g_gw.http, target, send_text + 4000, "Markdown", NULL, false, false, NULL);
        } else {
            telegram_send_message(g_gw.http, target, send_text, "Markdown", NULL, false, false, NULL);
        }
        sent = true;
    } else if (strcmp(platform, "discord") == 0) {
        discord_send_message(g_gw.http, send_text);
        sent = true;
    } else if (strcmp(platform, "mattermost") == 0) {
        mattermost_send_message(g_gw.http, send_text);
        sent = true;
    }

    /* Fallback: if primary platform send failed, use plain-text fallback */
    if (!sent && send_text && *send_text)
        gateway_send_fallback(platform, target, send_text);

    free(intercepted);
}

/* Port of Python gateway/platforms/base.py:send_typing().
 * AG26: Port of Python gateway/platforms/base.py:_send_typing().
 */
static void gateway_send_typing(const char *platform, const char *target) {
    if (!platform) return;

    /* P103: Try registered platform interface first */
    gw_platform_send_typing(platform, target);

    /* Legacy fallback */
    if (strcmp(platform, "telegram") == 0)
        telegram_send_chat_action(g_gw.http, target, "typing");
    else if (strcmp(platform, "discord") == 0)
        discord_send_typing(g_gw.http);
}

/* ================================================================
 *  P103: Platform interface implementation
 * ================================================================ */

void gw_platform_register(const gw_platform_t *plat) {
    if (!plat || !plat->name) return;
    if (g_gw.platform_def_count >= GW_MAX_PLATFORMS) return;
    g_gw.platform_defs[g_gw.platform_def_count++] = *plat;
}

int gw_platform_get_count(void) {
    return g_gw.platform_def_count;
}

static gw_platform_t *gw_platform_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_gw.platform_def_count; i++) {
        if (strcasecmp(g_gw.platform_defs[i].name, name) == 0)
            return &g_gw.platform_defs[i];
    }
    return NULL;
}

bool gw_platform_send(const char *platform_name, const char *chat_id, const char *text) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (!p || !p->send) return false;
    /* Apply pre-send hooks (may modify text) */
    if (gw_hooks.pre_send_count > 0) {
        char *modified = gw_apply_pre_send_hooks(platform_name, text);
        if (modified) {
            bool ok = p->send(chat_id, modified);
            free(modified);
            return ok;
        }
    }
    return p->send(chat_id, text);
}

void gw_platform_send_typing(const char *platform_name, const char *chat_id) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (p && p->send_typing)
        p->send_typing(chat_id);
}

/* 5C-252: Send emoji reaction (optional — NULL if platform doesn't support) */
bool gw_platform_send_reaction(const char *platform_name, const char *chat_id,
                                const char *message_id, const char *emoji) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (p && p->send_reaction)
        return p->send_reaction(chat_id, message_id, emoji);
    return false;
}

/* P103: Vtable wrappers — these bridge the gw_platform_t signature
   (no http_client_t parameter) to the platform-specific functions
   (which need http). The http client is captured from g_gw.http. */

static bool telegram_vtable_send_reaction(const char *chat_id,
                                           const char *message_id,
                                           const char *emoji) {
    return telegram_set_message_reaction(g_gw.http, chat_id, message_id, emoji);
}

/* Generic shutdown for polling-based platforms.
   Threads have already exited via g_gw.running flag + pthread_join by the
   time this is called.  Per-platform cleanup (HTTP pool, sessions) is
   handled by the global cleanup path — this just logs the event. */
static void poll_platform_shutdown(void) {
    printf("[gateway] Polling platform shutdown\n");
}

void gw_platform_shutdown_all(void) {
    for (int i = 0; i < g_gw.platform_def_count; i++) {
        if (g_gw.platform_defs[i].shutdown)
            g_gw.platform_defs[i].shutdown();
    }
}

/* Context for tool event callback — passes platform + chat_id */
/* Also carries stream callback state for progressive response delivery */
typedef struct {
    const char *platform;
    const char *chat_id;
    double      last_status_ts; /* monotonic time of last status send */
    double      last_stream_ts; /* monotonic time of last stream update */
    char        stream_buf[512]; /* accumulated stream tokens (truncated) */
    int         stream_len;      /* total chars accumulated so far */
} gw_status_ctx_t;

/* Gateway tool event callback — sends status messages during agent processing */
static int gateway_tool_event_cb(const char *event_type, const char *tool_name,
                                  const char *tool_args, void *user_data) {
    (void)tool_args;
    gw_status_ctx_t *ctx = (gw_status_ctx_t *)user_data;
    if (!ctx || !ctx->platform || !ctx->chat_id) return 0;

    if (strcmp(event_type, "tool.started") == 0) {
        /* Throttle: don't send more than one status per 2 seconds */
        double now = gw_mono_time();
        if (now - ctx->last_status_ts < 2.0) return 0;
        ctx->last_status_ts = now;

        char msg[512];
        snprintf(msg, sizeof(msg), "⚙️ Running *%s*... ", tool_name ? tool_name : "tool");
        /* P161: Filter/sanitize status messages before platform delivery.
           Mirrors Python _prepare_gateway_status_message(). */
        char *filtered = gateway_prepare_status_message(ctx->platform, msg);
        if (filtered) {
            gateway_send(ctx->platform, ctx->chat_id, filtered);
            free(filtered);
        }
    }
    return 0;
}

/* Gateway stream callback — accumulates tokens and periodically
 * updates typing indicator to show the agent is generating.
 * Port of Python gateway/stream_consumer.py (minimal sync version). */
static int gateway_stream_cb(const char *token, void *user_data) {
    gw_status_ctx_t *ctx = (gw_status_ctx_t *)user_data;
    if (!ctx || !ctx->platform || !ctx->chat_id || !token) return 0;

    /* Accumulate token into buffer (truncated) */
    size_t tlen = strlen(token);
    if (ctx->stream_len + (int)tlen < (int)sizeof(ctx->stream_buf) - 1) {
        memcpy(ctx->stream_buf + ctx->stream_len, token, tlen);
        ctx->stream_len += (int)tlen;
        ctx->stream_buf[ctx->stream_len] = '\0';
    }
    ctx->stream_len += (int)tlen; /* always count total */

    /* Throttle: send typing indicator every ~5 seconds during streaming */
    double now = gw_mono_time();
    if (now - ctx->last_stream_ts >= 5.0) {
        ctx->last_stream_ts = now;
        gw_platform_send_typing(ctx->platform, ctx->chat_id);
    }
    return 0;
}

/* ================================================================
 *  Process a single update (called from platform threads)
 * ================================================================ */

static void process_update(const char *platform, const char *chat_id, const char *text) {
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
    agent_state_t *session_agent = &g_gw.sessions[sess_idx].agent;
    pthread_mutex_unlock(&g_gw.session_mutex);

    /* M12: Populate agent session context from source metadata */
    gw_session_source_t *src = &g_gw.sessions[sess_idx].source;
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
        if (g_gw.sessions[sess_idx].last_resolved_model[0]) {
            snprintf(session_agent->llm.model,
                     sizeof(session_agent->llm.model),
                     "%s", g_gw.sessions[sess_idx].last_resolved_model);
            if (g_gw.sessions[sess_idx].last_resolved_provider[0]) {
                snprintf(session_agent->llm.provider,
                         sizeof(session_agent->llm.provider),
                         "%s", g_gw.sessions[sess_idx].last_resolved_provider);
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
        char *ctx_prompt = build_session_context_prompt(&g_gw.sessions[sess_idx].source);
        if (ctx_prompt) {
            context_set_system(session_agent, ctx_prompt);
            free(ctx_prompt);
        }
    }

    /* P109: Send typing indicator with 30s debounce */
    double now = gw_mono_time();
    if (now - g_gw.sessions[sess_idx].last_busy_ack > 30.0) {
        gateway_send_typing(platform, chat_id);
        g_gw.sessions[sess_idx].last_busy_ack = now;
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
            g_gw.sessions[sess_idx].last_resolved_model[0] = '\0';
            g_gw.sessions[sess_idx].last_resolved_provider[0] = '\0';
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

    /* Run agent on per-chat session */
    char *resp = agent_chat(session_agent, text);
    /* Clear callbacks after agent_chat returns — context is stack-local */
    session_agent->tool_event_cb = NULL;
    session_agent->tool_event_data = NULL;
    session_agent->stream_cb = NULL;
    session_agent->stream_data = NULL;
    if (resp) {
        /* P159: Redact secrets before sending response to chat.
           hermes_redact() handles API keys, tokens, JWTs, and
           configured patterns via key:value and free-text prefix matching. */
        char *redacted = hermes_redact(resp);
        /* P160: Sanitize provider errors for Telegram.
           Detects provider failure envelopes and rewrites them to
           user-safe short replies. Mirrors Python _sanitize_gateway_final_response(). */
        char *sanitized = gateway_sanitize_response(platform, redacted ? redacted : resp);
        gateway_send(platform, chat_id, sanitized ? sanitized : (redacted ? redacted : resp));
        free(sanitized);
        free(redacted);
        free(resp);
        /* GW12: Cache the resolved model/provider for fallback recovery */
        if (session_agent->llm.model[0]) {
            snprintf(g_gw.sessions[sess_idx].last_resolved_model,
                     sizeof(g_gw.sessions[sess_idx].last_resolved_model),
                     "%s", session_agent->llm.model);
            snprintf(g_gw.sessions[sess_idx].last_resolved_provider,
                     sizeof(g_gw.sessions[sess_idx].last_resolved_provider),
                     "%s", session_agent->llm.provider);
        }
    }
}

/* ================================================================
 *  Per-platform thread functions
 * ================================================================ */

/* Telegram-specific: poll for a response from a specific chat_id.
   Called during approval wait to short-poll Telegram for user's y/n/a response.
   Returns strdup'd text or NULL. */
static char *telegram_poll_for_response(const char *target_chat_id) {
    if (!target_chat_id) return NULL;

    json_node_t *root = telegram_get_updates(g_gw.http, g_gw.tg_offset, 5);
    if (!root) return NULL;

    json_node_t *result = json_obj_get(root, "result");
    char *response = NULL;
    if (result && json_len(result) > 0) {
        size_t n = json_len(result);
        for (size_t i = 0; i < n; i++) {
            json_node_t *update = json_get(result, i);
            double update_id = json_get_num(update, "update_id", 0);
            if (update_id > 0)
                g_gw.tg_offset = (int)update_id + 1;

            const char *chat_id = telegram_get_chat_id(update);
            const char *text = telegram_get_text(update);
            if (chat_id && text && strcmp(chat_id, target_chat_id) == 0) {
                response = strdup(text);
                break;
            }
        }
    }
    json_free(root);
    return response;
}

static void *thread_poll_telegram(void *arg) {
    (void)arg;
    printf("[gateway] Telegram polling (interval: %ds)\n", g_gw.poll_interval);

    /* L08: Fetch bot identity on first run for @mention detection */
    telegram_get_me(g_gw.http);
    if (telegram_get_username()[0])
        printf("[gateway] Telegram bot: @%s\n", telegram_get_username());

    /* Register approval poll function for Telegram.
       During approval wait, the gateway will short-poll Telegram for the response. */
    gw_approval_set_poll(telegram_poll_for_response, 1);

    while (g_gw.running) {
        json_node_t *root = telegram_get_updates(g_gw.http, g_gw.tg_offset, 30);

        if (root) {
            json_node_t *result = json_obj_get(root, "result");
            if (result && json_len(result) > 0) {
                size_t n = json_len(result);
                for (size_t i = 0; i < n; i++) {
                    json_node_t *update = json_get(result, i);
                    double update_id = json_get_num(update, "update_id", 0);
                    if (update_id > 0)
                        g_gw.tg_offset = (int)update_id + 1;

                    const char *chat_id = telegram_get_chat_id(update);
                    const char *text = telegram_get_text(update);
                    const char *thread_id = telegram_get_message_thread_id(update);
                    const char *message_id = NULL;
                    if (!chat_id || !text) continue;

                    /* ── Extract message_id from the update for source metadata ── */
                    json_node_t *msg = json_obj_get(update, "message");
                    if (!msg) msg = json_obj_get(update, "edited_message");
                    if (msg) {
                        const char *mid = json_get_str(msg, "message_id", NULL);
                        if (mid) message_id = mid;
                    }

                    /* ── Telegram message filtering (port of Python TelegramAdapter) ── */
                    bool is_group = telegram_is_group(update);
                    bool is_mentioned = telegram_is_mentioned(update);
                    const char *bot_username = telegram_get_username();
                    bool is_reply = false; /* TODO: detect reply_to_bot */

                    /* Determine observe vs process */
                    bool should_observe = tg_should_observe_message(
                        telegram_get_chat_type(update), chat_id, text, thread_id,
                        is_group, is_mentioned, is_reply, bot_username);

                    bool should_process = tg_should_process_message(
                        telegram_get_chat_type(update), chat_id, text, thread_id,
                        is_group, is_mentioned, is_reply, bot_username);

                    if (!should_process && !should_observe) {
                        /* Silently skip (not in allowed_chats, guest mode off, etc.) */
                        if (g_gw.group_observe_enabled)
                            printf("[gateway] Skipped (filtered): %s %s\n", chat_id, text);
                        continue;
                    }

                    /* L08: Group observe — silently accumulate unmentioned group messages */
                    if (should_observe) {
                        /* Build attributed text: [username|user_id]\ntext */
                        const char *user_id = telegram_get_user_id(update);
                        const char *user_name = telegram_get_user_name(update);
                        char observe_buf[8192];
                        if (user_name && user_id) {
                            snprintf(observe_buf, sizeof(observe_buf),
                                     "[%s|%s]\n%s", user_name, user_id, text);
                        } else {
                            snprintf(observe_buf, sizeof(observe_buf), "%s", text);
                        }
                        gw_observe_append("telegram", chat_id, observe_buf);
                        printf("[gateway] Observed (no trigger): %s: %s\n", chat_id, observe_buf);
                        continue;
                    }

                    process_update("telegram", chat_id, text);

                    /* P102a: Populate session source metadata from Telegram update */
                    gw_session_source_t src;
                    session_source_set(&src, "telegram", chat_id,
                                       telegram_get_chat_name(update),
                                       telegram_get_chat_type(update),
                                       telegram_get_user_id(update),
                                       telegram_get_user_name(update),
                                       thread_id, /* thread_id for topics */
                                       NULL, /* chat_topic */
                                       NULL, /* user_id_alt */
                                       NULL, /* chat_id_alt */
                                       NULL, /* guild_id */
                                       NULL, /* parent_chat_id */
                                       message_id, /* message_id */
                                       telegram_is_bot(update));
                    gw_session_set_source("telegram", chat_id, &src);
                }
            }
            json_free(root);
            /* Successful poll — reset reconnect backoff */
            gw_reconnect_reset(0);
        } else {
            /* Poll failed — exponential backoff reconnect */
            double delay = gw_reconnect_delay(0);
            printf("[gateway] Poll failed, reconnecting in %.0fs\n", delay);
            if (g_gw.running) sleep((int)delay);
        }

        /* Drain queued messages (e.g., from rate-limiting on prior cycles) */
        if (gw_queue_depth() > 0)
            gw_queue_drain_all();

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_discord(void *arg) {
    (void)arg;
    printf("[gateway] Discord polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = discord_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("discord",
                               discord_get_chat_id(update),
                               discord_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_slack(void *arg) {
    (void)arg;
    printf("[gateway] Slack polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = slack_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("slack",
                               slack_get_chat_id(update),
                               slack_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_matrix(void *arg) {
    (void)arg;
    printf("[gateway] Matrix polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = matrix_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("matrix",
                               matrix_get_chat_id(update),
                               matrix_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_poll_mattermost(void *arg) {
    (void)arg;
    printf("[gateway] Mattermost polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = mattermost_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("mattermost",
                               mattermost_get_chat_id(update),
                               mattermost_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

static void *thread_webhook(void *arg) {
    int port = *(int *)arg;
    printf("[gateway] Webhook HTTP API on port %d\n", port);
    webhook_server_run(port);
    return NULL;
}

/* ================================================================
 *  Signal handler
 * ================================================================ */

static void handle_signal(int sig) {
    (void)sig;
    printf("\n[gateway] Shutting down...\n");
    g_gw.running = false;
}

/* ================================================================
 *  Platform setup helpers
 * ================================================================ */

typedef struct {
    const char *name;
    bool (*setup)(void);
    void *(*thread_fn)(void *);
    int arg_int; /* For port numbers etc. */
} platform_def_t;

static bool setup_telegram(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("telegram");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("TELEGRAM_BOT_TOKEN");
    if (!token) token = getenv("HERMES_TELEGRAM_TOKEN");
    if (!token) { fprintf(stderr, "Warning: TELEGRAM_BOT_TOKEN not set (set gateway.platforms.telegram.token in config.yaml or TELEGRAM_BOT_TOKEN env)\\n"); return false; }
    telegram_set_token(token);
    return true;
}

static bool setup_discord(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("discord");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("DISCORD_BOT_TOKEN");
    const char *channel = getenv("DISCORD_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: DISCORD_BOT_TOKEN or DISCORD_CHANNEL_ID not set\n");
        return false;
    }
    discord_set_token(token);
    discord_set_channel(channel);
    return true;
}

static bool setup_slack(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("slack");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("SLACK_BOT_TOKEN");
    const char *channel = getenv("SLACK_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: SLACK_BOT_TOKEN or SLACK_CHANNEL_ID not set\n");
        return false;
    }
    slack_set_token(token);
    slack_set_channel(channel);
    return true;
}

static bool setup_matrix(void) {
    const char *hs = getenv("MATRIX_HOMESERVER");
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("matrix");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("MATRIX_ACCESS_TOKEN");
    const char *room = getenv("MATRIX_ROOM_ID");
    if (!token) { fprintf(stderr, "Warning: MATRIX_ACCESS_TOKEN not set\n"); return false; }
    matrix_set_homeserver(hs && hs[0] ? hs : "https://matrix.org");
    matrix_set_token(token);
    if (room) matrix_set_room(room);
    return true;
}

static bool setup_mattermost(void) {
    const char *url = getenv("MATTERMOST_URL");
    const char *token = getenv("MATTERMOST_TOKEN");
    const char *channel = getenv("MATTERMOST_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: MATTERMOST_TOKEN or MATTERMOST_CHANNEL_ID not set\n");
        return false;
    }
    mattermost_set_url(url && url[0] ? url : "http://localhost:8065");
    mattermost_set_token(token);
    mattermost_set_channel(channel);
    return true;
}

static bool setup_webhook(void) {
    const char *secret = getenv("WEBHOOK_SECRET");
    if (secret && *secret) {
        webhook_set_verify_secret(secret);
        printf("[webhook] HMAC verification: enabled\n");
    } else {
        printf("[webhook] HMAC verification: disabled (no WEBHOOK_SECRET)\n");
    }
    return true;
}

/* Port of Python hermes_cli/gateway.py:_setup_whatsapp(). */
static bool setup_whatsapp(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("whatsapp");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("WHATSAPP_TOKEN");
    const char *phone = getenv("WHATSAPP_PHONE_NUMBER_ID");
    const char *verify = getenv("WHATSAPP_VERIFY_TOKEN");
    if (!token || !phone) {
        fprintf(stderr, "Warning: WHATSAPP_TOKEN or WHATSAPP_PHONE_NUMBER_ID not set\n");
        return false;
    }
    whatsapp_set_token(token);
    whatsapp_set_phone_id(phone);
    if (verify) whatsapp_set_verify_token(verify);
    return true;
}

static bool setup_email(void) {
    const char *from = getenv("EMAIL_FROM");
    if (from) email_set_from(from);

    /* Validate: email needs IMAP (for incoming) or SMTP/sendmail (for outgoing) */
    const char *imap = getenv("EMAIL_IMAP_SERVER");
    const char *smtp = getenv("EMAIL_SMTP_SERVER");
    const char *cmd = getenv("EMAIL_SEND_CMD");
    if (!imap && !smtp && !cmd) {
        fprintf(stderr, "Warning: neither EMAIL_IMAP_SERVER nor EMAIL_SMTP_SERVER nor"
                        " EMAIL_SEND_CMD set. Email will not function.\n");
        return false;
    }
    return true;
}

/* Port of Python hermes_cli/gateway.py:_setup_signal(). */
static bool setup_signal(void) {
    const char *number = getenv("SIGNAL_NUMBER");
    const char *cli_path = getenv("SIGNAL_CLI_PATH");
    if (!number) {
        fprintf(stderr, "Warning: SIGNAL_NUMBER not set\n");
        return false;
    }
    signal_set_number(number);
    if (cli_path) signal_set_cli_path(cli_path);
    return true;
}

/* Setup for API Server platform */
static bool setup_api_server(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("api_server");
    const char *api_key = NULL;
    if (pc && pc->api_key[0]) api_key = pc->api_key;
    if (!api_key) api_key = getenv("API_SERVER_KEY");
    if (!api_key) {
        fprintf(stderr, "Warning: API_SERVER_KEY not set (set gateway.platforms.api_server.key in config.yaml or API_SERVER_KEY env)\n");
        return false;
    }
    /* The API server adapter is now registered via register_api_server_platform()
     * in api_server_adapter.c. The connect() call in init will start the server thread. */
    return true;
}

/* Email poll thread */
static void *thread_poll_email(void *arg) {
    (void)arg;
    int poll_int = g_gw.poll_interval * 3; /* Email polls less frequently */
    printf("[gateway] Email polling (interval: %ds)\n", poll_int);
    while (g_gw.running) {
        json_node_t *updates = email_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("email",
                               email_get_chat_id(update),
                               email_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(poll_int);
    }
    return NULL;
}

/* Signal poll thread */
static void *thread_poll_signal(void *arg) {
    (void)arg;
    /* Check if signal-cli is available */
    if (!signal_check_available()) {
        printf("[gateway] signal-cli not found. Signal platform disabled.\n");
        return NULL;
    }
    printf("[gateway] Signal polling (interval: %ds)\n", g_gw.poll_interval);
    while (g_gw.running) {
        json_node_t *updates = signal_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("signal",
                               signal_get_chat_id(update),
                               signal_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

/* HomeAssistant setup + thread */
static bool setup_ha(void) {
    const char *url = getenv("HA_URL");
    const char *token = getenv("HA_TOKEN");
    if (!url || !token) {
        fprintf(stderr, "Warning: HA_URL and HA_TOKEN must be set\n");
        return false;
    }
    ha_set_url(url);
    ha_set_token(token);
    const char *entity = getenv("HA_NOTIFY_ENTITY");
    if (entity) ha_set_notify_entity(entity);
    return true;
}

static void *thread_poll_ha(void *arg) {
    (void)arg;
    printf("[gateway] HomeAssistant polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = ha_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("homeassistant",
                               ha_get_chat_id(update),
                               ha_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* SMS setup + thread */
static bool setup_sms(void) {
    const char *sid = getenv("TWILIO_ACCOUNT_SID");
    const char *token = getenv("TWILIO_AUTH_TOKEN");
    const char *from = getenv("TWILIO_FROM_NUMBER");
    if (!sid || !from) {
        fprintf(stderr, "Warning: TWILIO_ACCOUNT_SID and TWILIO_FROM_NUMBER must be set\n");
        return false;
    }
    sms_set_twilio(sid, token, from);

    /* P111: Optional status callback URL for delivery status */
    const char *cb = getenv("TWILIO_STATUS_CALLBACK");
    if (cb) {
        sms_set_status_callback(cb);
        printf("[gateway] SMS status callbacks configured\n");
    }

    /* P111: Optional webhook path (default /sms-webhook on the webhook server) */
    const char *wh = getenv("TWILIO_WEBHOOK_PATH");
    if (wh) {
        sms_set_webhook_url(wh);
    }
    return true;
}

static void *thread_poll_sms(void *arg) {
    (void)arg;
    printf("[gateway] SMS/Twilio polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = sms_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("sms",
                               sms_get_chat_id(update),
                               sms_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* Port of Python hermes_cli/gateway.py:_setup_feishu(). */
/* Feishu setup */
static bool setup_feishu(void) {
    const char *url = getenv("FEISHU_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: FEISHU_WEBHOOK_URL not set\n");
        return false;
    }
    feishu_set_webhook(url);
    return true;
}

static void *thread_poll_feishu(void *arg) {
    (void)arg;
    printf("[gateway] Feishu platform (webhook-based). Idle.\n");
    while (g_gw.running) sleep(g_gw.poll_interval * 10);
    return NULL;
}

/* Port of Python hermes_cli/gateway.py:_setup_wecom(). */
/* WeCom setup */
static bool setup_wecom(void) {
    const char *url = getenv("WECOM_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: WECOM_WEBHOOK_URL not set\n");
        return false;
    }
    wecom_set_webhook(url);
    return true;
}

static void *thread_poll_wecom(void *arg) {
    (void)arg;
    printf("[gateway] WeCom polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = wecom_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("wecom",
                               wecom_get_chat_id(update),
                               wecom_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* Port of Python hermes_cli/gateway.py:_setup_dingtalk(). */
/* DingTalk setup */
static bool setup_dingtalk(void) {
    const char *url = getenv("DINGTALK_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: DINGTALK_WEBHOOK_URL not set\n");
        return false;
    }
    dingtalk_set_webhook(url);
    return true;
}

static void *thread_poll_dingtalk(void *arg) {
    (void)arg;
    printf("[gateway] DingTalk polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = dingtalk_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("dingtalk",
                               dingtalk_get_chat_id(update),
                               dingtalk_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* QQ Bot setup */
static bool setup_qqbot(void) {
    const char *url = getenv("QQ_BOT_WEBHOOK_URL");
    const char *token = getenv("QQ_BOT_TOKEN");
    if (!url) {
        fprintf(stderr, "Warning: QQ_BOT_WEBHOOK_URL not set\n");
        return false;
    }
    qqbot_set_webhook(url);
    if (token) qqbot_set_token(token);
    return true;
}

static void *thread_poll_qqbot(void *arg) {
    (void)arg;
    printf("[gateway] QQ Bot polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = qqbot_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("qqbot",
                               qqbot_get_chat_id(update),
                               qqbot_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

/* BlueBubbles setup */
static bool setup_bluebubbles(void) {
    const char *url = getenv("BLUEBUBBLES_URL");
    const char *pwd = getenv("BLUEBUBBLES_PASSWORD");
    if (!url || !pwd) {
        fprintf(stderr, "Warning: BLUEBUBBLES_URL and BLUEBUBBLES_PASSWORD must be set\n");
        return false;
    }
    bluebubbles_set_url(url);
    bluebubbles_set_password(pwd);
    return true;
}

static void *thread_poll_bluebubbles(void *arg) {
    (void)arg;
    printf("[gateway] BlueBubbles platform (iMessage). Polling every %ds.\\n", g_gw.poll_interval);

    /* Check for optional poll GUID env var */
    const char *poll_guid = getenv("BLUEBUBBLES_POLL_GUID");
    if (poll_guid) {
        bluebubbles_set_poll_guid(poll_guid);
        printf("[gateway] BlueBubbles polling chat GUID: %s\\n", poll_guid);
    } else {
        bluebubbles_set_poll_guid(NULL);
        printf("[gateway] BlueBubbles is webhook-driven. Set BLUEBUBBLES_POLL_GUID for polling.\\n");
    }

    while (g_gw.running) {
        json_node_t *updates = bluebubbles_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                const char *chat_id = bluebubbles_get_chat_id(update);
                const char *text = bluebubbles_get_text(update);
                if (chat_id && text && text[0]) {
                    process_update("bluebubbles", chat_id, text);
                }
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

/* ================================================================
 *  Get port from env with HERMES_ or SLERMES_ prefix
 * ================================================================ */

static int get_webhook_port(void) {
    /* 1. Config value (from YAML) takes priority */
    if (g_gw.config.webhook_port > 0 && g_gw.config.webhook_port <= 65535)
        return g_gw.config.webhook_port;
    /* 2. Env vars */
    const char *port_str = getenv("SLERMES_WEBHOOK_PORT");
    if (!port_str) port_str = getenv("HERMES_WEBHOOK_PORT");
    if (!port_str) port_str = getenv("WEBHOOK_PORT");
    int port = port_str ? atoi(port_str) : 8080;
    if (port <= 0 || port > 65535) port = 8080;
    return port;
}

/* ================================================================
 *  Gateway entry point
 * ================================================================ */

static bool setup_msgraph_webhook(void) {
    const char *port_str = getenv("MSGRAPH_WEBHOOK_PORT");
    int port = port_str ? atoi(port_str) : 8646;
    if (port <= 0 || port > 65535) port = 8646;
    msgraph_webhook_init(NULL, NULL, port);
    return true;
}

static void *thread_msgraph_webhook(void *arg) {
    (void)arg;
    msgraph_webhook_run();
    return NULL;
}

/* weixin setup + thread */
extern bool weixin_init(const char *token, const char *account_id);
extern void weixin_start(void);
extern void weixin_stop(void);

/* Port of Python hermes_cli/gateway.py:_setup_weixin(). */
static bool setup_weixin(void) {
    const char *token = getenv("WEIXIN_TOKEN");
    const char *account_id = getenv("WEIXIN_ACCOUNT_ID");
    if (!token || !account_id) {
        fprintf(stderr, "Warning: WEIXIN_TOKEN and WEIXIN_ACCOUNT_ID must be set\n");
        return false;
    }
    weixin_init(token, account_id);
    return true;
}

static void *thread_weixin(void *arg) {
    (void)arg;
    weixin_start();
    return NULL;
}

/* yuanbao setup + thread */
extern bool yuanbao_init(const char *app_id, const char *app_secret,
                         const char *bot_id, const char *ws_url,
                         const char *api_domain);
extern void yuanbao_start(void);
extern void yuanbao_stop(void);

static bool setup_yuanbao(void) {
    const char *app_id = getenv("YUANBAO_APP_ID");
    const char *app_secret = getenv("YUANBAO_APP_SECRET");
    const char *bot_id = getenv("YUANBAO_BOT_ID");
    const char *ws_url = getenv("YUANBAO_WS_URL");
    const char *api_domain = getenv("YUANBAO_API_DOMAIN");
    if (!app_id || !app_secret) {
        fprintf(stderr, "Warning: YUANBAO_APP_ID and YUANBAO_APP_SECRET must be set\n");
        return false;
    }
    return yuanbao_init(app_id, app_secret, bot_id, ws_url, api_domain);
}

static void *thread_yuanbao(void *arg) {
    (void)arg;
    yuanbao_start();
    return NULL;
}

/* Port of Python hermes_cli/dump.py:_gateway_status(). */
/* ── Gateway subcommand: status ───────────────────────────────── */
static int cmd_gateway_status(void) {
    hermes_config_t cfg;
    if (!hermes_config_load(&cfg, NULL)) {
        printf("No config loaded\n");
        return 1;
    }

    printf("=== Gateway Status ===\n\n");

    printf("Configured platforms: ");
    if (cfg.gateway_platforms[0])
        printf("%s\n", cfg.gateway_platforms);
    else
        printf("(none in config)\n");

    printf("Env HERMES_GATEWAY_PLATFORMS: ");
    const char *env = getenv("HERMES_GATEWAY_PLATFORMS");
    if (env) printf("%s\n", env); else printf("(not set)\n");

    printf("Default platform: telegram\n");

    /* Check key env vars per platform type */
    static const char *platform_keys[][2] = {
        {"telegram", "TELEGRAM_BOT_TOKEN"},
        {"discord",  "DISCORD_BOT_TOKEN"},
        {"slack",    "SLACK_BOT_TOKEN"},
        {"signal",   "SIGNAL_NUMBER"},
        {"sms",      "TWILIO_ACCOUNT_SID"},
        {"matrix",   "MATRIX_HOMESERVER"},
        {NULL, NULL}
    };

    printf("\nCredentials check:\n");
    for (int i = 0; platform_keys[i][0]; i++) {
        const char *val = getenv(platform_keys[i][1]);
        printf("  %-12s %s %s\n", platform_keys[i][0],
               val ? "✅" : "❌", val ? "(found)" : "missing");
    }

    printf("\nGateway: ready to start with `slermes gateway start`\n");
    return 0;
}

/* Port of Python hermes_cli/gateway.py:_gateway_list(). */
/* ── Gateway subcommand: list ─────────────────────────────────── */
static int cmd_gateway_list(void) {
    static const char *platforms[] = {
        "telegram", "discord", "slack", "matrix", "mattermost",
        "webhook", "whatsapp", "email", "signal", "homeassistant",
        "sms", "api_server", "feishu", "wecom", "dingtalk",
        "qqbot", "bluebubbles", "msgraph_webhook", "weixin", "yuanbao",
        NULL
    };
    static const char *descriptions[] = {
        "Telegram bot API polling", "Discord gateway bot", "Slack RTM/Events API",
        "Matrix client-server API", "Mattermost webhooks",
        "Generic HTTP webhook receiver", "WhatsApp Cloud API webhook",
        "IMAP/SMTP email client", "Signal CLI over dbus",
        "Home Assistant long-lived token API",
        "Twilio SMS gateway", "REST API server",
        "Feishu/Lark bot API", "WeCom (WeChat Work) bot API",
        "DingTalk bot API",
        "QQ Bot API (OneBot/QQ Guild)", "BlueBubbles iMessage API",
        "Microsoft Graph API webhook", "Weixin Official Account",
        "Yuanbao (Tencent) protobuf protocol",
        NULL
    };

    printf("=== Available Gateway Platforms ===\n\n");
    for (int i = 0; platforms[i]; i++)
        printf("  %-20s %s\n", platforms[i], descriptions[i]);
    printf("\n%d platform types available\n", 20);
    printf("Usage: slermes gateway [start|--platform <name>]\n");
    return 0;
}

/* Periodic cleanup thread — evicts idle sessions every 60s */
/* GW13: Kanban notifier thread function
 * Polls kanban board JSON files for pending notification events and
 * delivers them to subscribed platform/chat/thread targets via the
 * gateway's platform send function. Mirrors Python's _kanban_notifier_watcher.
 *
 * For each board on disk (~/.hermes/kanban/boards/<slug>.json):
 *   1. Read notify_subs and find active subscriptions owned by this profile
 *   2. For each sub, find terminal events (completed/blocked/gave_up/crashed/timed_out)
 *      with id > last_event_id for the subscribed task
 *   3. Send notification message to (platform, chat_id, thread_id)
 *   4. Advance cursor on success, increment fail_count on failure
 *   5. Remove sub after max consecutive failures (dead chat detection)
 *   6. Auto-unsubscribe when task reaches terminal state (done/archived)
 */
static void *thread_kanban_notifier(void *arg) {
    (void)arg;

    /* Initial delay so gateway finishes wiring platform adapters */
    sleep(5);

    fprintf(stderr, "[kanban-notifier] started (profile=%s)\n",
            g_gw.kanban_notifier_profile[0] ? g_gw.kanban_notifier_profile : "(default)");

    while (g_gw.running) {
        sleep(g_gw.kanban_notifier_interval_sec);
        if (!g_gw.running) break;

        /* Scan board files on disk */
        char boards_dir[4096];
        snprintf(boards_dir, sizeof(boards_dir), "%s/.hermes/kanban/boards",
                 getenv("HOME") ? getenv("HOME") : "/tmp");

        DIR *dir = opendir(boards_dir);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!strstr(entry->d_name, ".json")) continue;

            char board_path[4096];
            snprintf(board_path, sizeof(board_path), "%s/%s", boards_dir, entry->d_name);

            /* Read board JSON file */
            FILE *f = fopen(board_path, "r");
            if (!f) continue;

            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            if (fsize > 1024 * 1024 || fsize <= 0) { fclose(f); continue; }
            fseek(f, 0, SEEK_SET);

            char *json_buf = (char *)malloc((size_t)fsize + 1);
            if (!json_buf) { fclose(f); continue; }
            size_t nread = fread(json_buf, 1, (size_t)fsize, f);
            json_buf[nread] = '\0';
            fclose(f);

            /* Parse board JSON — extract slug, tasks, events, notify_subs */
            json_t *board = json_parse(json_buf, NULL);
            free(json_buf);
            if (!board) continue;

            const char *slug = json_get_str(board, "slug", "default");
            (void)slug;

            /* Process notify_subs */
            json_t *subs = json_obj_get(board, "notify_subs");
            json_t *events = json_obj_get(board, "events");

            if (subs && events) {
                size_t nsubs = json_len(subs);
                size_t nevents = json_len(events);

                for (size_t si = 0; si < nsubs; si++) {
                    json_t *sub = json_get(subs, si);
                    if (!sub) continue;
                    if (!json_get_bool(sub, "active", false)) continue;

                    const char *task_id = json_get_str(sub, "task_id", "");
                    const char *platform = json_get_str(sub, "platform", "");
                    const char *chat_id = json_get_str(sub, "chat_id", "");
                    const char *thread_id = json_get_str(sub, "thread_id", "");
                    const char *sub_profile = json_get_str(sub, "notifier_profile", "");
                    int64_t last_event_id = (int64_t)json_get_num(sub, "last_event_id", 0);

                    if (!task_id[0] || !platform[0] || !chat_id[0]) continue;

                    /* Profile ownership check */
                    if (sub_profile[0] && g_gw.kanban_notifier_profile[0] &&
                        strcmp(sub_profile, g_gw.kanban_notifier_profile) != 0) {
                        continue;  /* owned by different profile */
                    }

                    /* Check if platform adapter is connected */
                    gw_platform_t *plat = gw_platform_find(platform);
                    if (!plat) continue;

                    /* Collect unseen terminal events for this task */
                    int64_t max_id = last_event_id;
                    int event_count = 0;
                    char event_kinds[64][64];
                    char event_payloads[64][2048];

                    for (size_t ei = 0; ei < nevents; ei++) {
                        json_t *ev = json_get(events, ei);
                        if (!ev) continue;

                        int64_t ev_id = (int64_t)json_get_num(ev, "id", 0);
                        if (ev_id <= last_event_id) continue;

                        const char *ev_task = json_get_str(ev, "task_id", "");
                        if (strcmp(ev_task, task_id) != 0) continue;

                        const char *kind = json_get_str(ev, "kind", "");
                        /* Only terminal kinds */
                        bool is_terminal = false;
                        const char *terminal_kinds[] = {
                            "completed", "blocked", "gave_up", "crashed", "timed_out", NULL
                        };
                        for (int tk = 0; terminal_kinds[tk]; tk++) {
                            if (strcmp(kind, terminal_kinds[tk]) == 0) {
                                is_terminal = true;
                                break;
                            }
                        }
                        if (!is_terminal) continue;

                        if (event_count < 64) {
                            snprintf(event_kinds[event_count], 64, "%s", kind);
                            const char *payload = json_get_str(ev, "payload", "{}");
                            snprintf(event_payloads[event_count], 2048, "%s", payload);
                            event_count++;
                        }
                        if (ev_id > max_id) max_id = ev_id;
                    }

                    if (event_count == 0) continue;

                    /* Build notification message */
                    char msg[4096];
                    if (event_count == 1) {
                        snprintf(msg, sizeof(msg),
                                 "📋 Kanban task %s: %s\n%s",
                                 task_id, event_kinds[0], event_payloads[0]);
                    } else {
                        int written = snprintf(msg, sizeof(msg),
                                               "📋 Kanban task %s: %d new events\n",
                                               task_id, event_count);
                        for (int ei = 0; ei < event_count && ei < 5; ei++) {
                            size_t len = strlen(msg);
                            snprintf(msg + len, sizeof(msg) - len,
                                     "  • %s\n", event_kinds[ei]);
                        }
                        (void)written;
                    }

                    /* Deliver via platform adapter */
                    char full_chat[256];
                    if (thread_id && thread_id[0]) {
                        snprintf(full_chat, sizeof(full_chat), "%s:%s", chat_id, thread_id);
                    } else {
                        snprintf(full_chat, sizeof(full_chat), "%s", chat_id);
                    }

                    int sent = gw_platform_send(platform, full_chat, msg) ? 0 : -1;

                    if (sent == 0) {
                        /* Success: advance cursor in the JSON file */
                        json_set(sub, "last_event_id", json_number((double)max_id));

                        /* Check if task is terminal — auto-unsubscribe */
                        json_t *task_list = json_obj_get(board, "tasks");
                        if (task_list) {
                            size_t ntasks = json_len(task_list);
                            for (size_t ti = 0; ti < ntasks; ti++) {
                                json_t *t = json_get(task_list, ti);
                                if (!t) continue;
                                const char *tid = json_get_str(t, "id", "");
                                if (strcmp(tid, task_id) == 0) {
                                    const char *col = json_get_str(t, "column", "");
                                    if (strcmp(col, "done") == 0 || strcmp(col, "archived") == 0) {
                                        json_set(sub, "active", json_bool(false));
                                    }
                                    break;
                                }
                            }
                        }

                        fprintf(stderr, "[kanban-notifier] delivered %d event(s) for %s to %s:%s\n",
                                event_count, task_id, platform, full_chat);
                    } else {
                        /* Failure: increment fail count */
                        int fail_count = (int)json_get_num(sub, "fail_count", 0) + 1;
                        json_set(sub, "fail_count", json_number((double)fail_count));

                        if (fail_count >= g_gw.kanban_notifier_max_fail) {
                            json_set(sub, "active", json_bool(false));
                            fprintf(stderr, "[kanban-notifier] removed dead sub for %s on %s (fail_count=%d)\n",
                                    task_id, platform, fail_count);
                        } else {
                            fprintf(stderr, "[kanban-notifier] send failed for %s on %s (fail_count=%d/%d)\n",
                                    task_id, platform, fail_count, g_gw.kanban_notifier_max_fail);
                        }
                    }
                }
            }

            /* Write updated board back to disk */
            char *updated = json_serialize(board);
            if (updated) {
                FILE *out = fopen(board_path, "w");
                if (out) {
                    fputs(updated, out);
                    fclose(out);
                }
                free(updated);
            }

            json_free(board);
        }
        closedir(dir);
    }

    fprintf(stderr, "[kanban-notifier] stopped\n");
    return NULL;
}

static void *thread_cleanup_sessions(void *arg) {
    (void)arg;
    while (g_gw.running) {
        sleep(60);
        pthread_mutex_lock(&g_gw.session_mutex);
        session_cleanup_idle();
        pthread_mutex_unlock(&g_gw.session_mutex);
    }
    return NULL;
}

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
    /* Shutdown all platforms */
    gw_platform_shutdown_all();
    /* P102: Save and free all sessions */
    session_save_all();
    pthread_mutex_destroy(&g_gw.session_mutex);
    for (int i = 0; i < g_gw.session_count; i++) {
        if (g_gw.sessions[i].in_use) {
            if (g_gw.sessions[i].db) db_close(g_gw.sessions[i].db);
            agent_free(&g_gw.sessions[i].agent);
        }
    }
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
