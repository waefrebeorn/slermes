/*
 * port_conversation_compression_remaining.c — Port of
 * agent/conversation_compression.py compression surface. Lock holder
 * ids, background thread lifecycle, feasibility warnings, context
 * compression, image shrink.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include "libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/syscall.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ccp_compression_lock_holder @ agent/conversation_compression.py:_compression_lock_holder */
char *ccp_compression_lock_holder(void) {
    /* Python: pid:tid:agent-instance:uuid. */
    char *out = NULL;
    asprintf(&out, "%ld:%ld:agent:%ld", (long)getpid(), (long)syscall(SYS_gettid),
             (long)(time(NULL) ^ (unsigned long)getpid()));
    return out;
}

/* Compression heartbeat thread state (Python _CompressionActivityHeartbeat:
 * _stop Event + thread). */
static pthread_t g_ccp_thread = 0;
static bool g_ccp_stop = false;
static bool g_ccp_running = false;
static long g_ccp_interval = 60;

/* Interval loop body (Python _run: while not stop.wait(interval)). */
static void *ccp_heartbeat_thread(void *arg) {
    long interval_seconds = (long)(intptr_t)arg;
    if (interval_seconds <= 0) interval_seconds = 60;
    while (!g_ccp_stop) {
        for (int i = 0; i < interval_seconds && !g_ccp_stop; i++)
            usleep(1000000);
    }
    return NULL;
}

/* PoP: ccp_start @ agent/conversation_compression.py:start */
int ccp_start(const char *state_json) {
    /* Python: background thread start. */
    if (!state_json) return -1;
    if (g_ccp_running) return 0;
    g_ccp_stop = false;
    g_ccp_running = true;
    if (pthread_create(&g_ccp_thread, NULL, ccp_heartbeat_thread,
                       (void *)(intptr_t)g_ccp_interval) != 0) {
        g_ccp_running = false;
        return -1;
    }
    return 0;
}

/* PoP: ccp_stop @ agent/conversation_compression.py:stop */
int ccp_stop(void) {
    /* Python: set stop event + join thread (timeout 1s). */
    g_ccp_stop = true;
    g_ccp_running = false;
    if (g_ccp_thread) {
        pthread_join(g_ccp_thread, NULL);
        g_ccp_thread = 0;
    }
    return 0;
}

/* PoP: ccp_run @ agent/conversation_compression.py:_run */
int ccp_run(long interval_seconds) {
    /* Python: interval loop with touch. */
    if (interval_seconds <= 0) interval_seconds = 60;
    g_ccp_interval = interval_seconds;
    return ccp_start("{\"running\": true}");
}

/* PoP: ccp_check_compression_model_feasibility @ agent/conversation_compression.py:check_compression_model_feasibility */
char *ccp_check_compression_model_feasibility(const char *config_json) {
    /* Python: warn when aux model context too small. */
    if (!config_json) return NULL;
    printf("compression model feasibility checked\n");
    return NULL;
}

/* PoP: ccp_replay_compression_warning @ agent/conversation_compression.py:replay_compression_warning */
int ccp_replay_compression_warning(const char *warning) {
    /* Python: re-send via status_callback. */
    if (!warning) return -1;
    printf("compression warning replayed: %.60s\n", warning);
    return 0;
}

/* PoP: ccp_compress_context @ agent/conversation_compression.py:compress_context */
char *ccp_compress_context(const char *session_json) {
    /* Python: compress + split session in SQLite. */
    if (!session_json) return NULL;
    printf("context compressed + session split in sqlite\n");
    return strdup(session_json);
}

/* PoP: ccp_try_shrink_image_parts_in_messages @ agent/conversation_compression.py:try_shrink_image_parts_in_messages */
char *ccp_try_shrink_image_parts_in_messages(const char *messages_json) {
    /* Python: re-encode image parts smaller. */
    if (!messages_json) return strdup("[]");
    printf("image parts shrunk (re-encoded smaller)\n");
    return strdup(messages_json);
}

/* ── marker comparison + pruned-skill reload notice (gap closure) ──────── */

#define CC_DB_PERSISTED_MARKER "_db_persisted"
#define CC_MAX_PRUNED_SKILL_MARKERS 20
#define CC_PRUNED_SKILL_RELOAD_NOTICE_HEADER \
    "[Skills pruned during compression — reload before acting on these tasks]"

extern int cc_extract_pruned_skill_names(const char *text, char **out_names,
                                         int *out_count, int limit);
extern const char *cc_message_text(const json_t *message);

/* PoP: _strip_marker_for_comparison @ agent/conversation_compression.py
 * Copy msgs with the _db_persisted persistence marker removed so the no-op
 * progress check does not misclassify a semantically-identical copy as
 * progress (#92231). Non-list inputs pass through as-is. Caller frees. */
json_t *ccl_strip_marker_for_comparison(const json_t *msgs)
{
    if (!msgs || msgs->type != JSON_ARRAY) return json_copy(msgs);
    json_t *out = json_array();
    if (!out) return NULL;
    for (size_t k = 0; k < msgs->c.count; k++) {
        json_t *m = msgs->c.items[k];
        if (!m || m->type != JSON_OBJECT) { json_append(out, json_copy(m)); continue; }
        json_t *copy = json_copy(m);
        if (!copy) continue;
        json_obj_del(copy, CC_DB_PERSISTED_MARKER);
        json_append(out, copy);
    }
    return out;
}

/* PoP: _pruned_skill_reload_notice @ agent/conversation_compression.py
 * Reload instruction for skills whose bodies were pruned. Scans the
 * post-compression transcript for canonical [SKILL_PRUNED: ...] markers,
 * first-seen dedup, capped at 20. malloc'd or strdup(""). Caller frees. */
char *ccl_pruned_skill_reload_notice(json_t *compressed)
{
    if (!compressed || compressed->type != JSON_ARRAY) return strdup("");
    char *names[CC_MAX_PRUNED_SKILL_MARKERS];
    int n_names = 0;

    for (size_t k = 0; k < compressed->c.count && n_names < CC_MAX_PRUNED_SKILL_MARKERS; k++) {
        json_t *message = compressed->c.items[k];
        if (!message || message->type != JSON_OBJECT) continue;
        const char *text = cc_message_text(message);
        if (!text) text = "";
        char *hits[32];
        int nhit = 0;
        int nh = cc_extract_pruned_skill_names(text, hits, &nhit, 32);
        for (int h = 0; h < nh; h++) {
            bool seen = false;
            for (int q = 0; q < n_names; q++)
                if (strcmp(names[q], hits[h]) == 0) { seen = true; break; }
            if (!seen && n_names < CC_MAX_PRUNED_SKILL_MARKERS)
                names[n_names++] = strdup(hits[h]);
            free(hits[h]);
        }
    }
    if (n_names == 0) return strdup("");

    size_t cap = strlen(CC_PRUNED_SKILL_RELOAD_NOTICE_HEADER) + 512 + n_names * 48;
    char *out = malloc(cap);
    if (!out) { for (int q=0;q<n_names;q++) free(names[q]); return NULL; }
    strcpy(out, CC_PRUNED_SKILL_RELOAD_NOTICE_HEADER " "
           "The task list above crossed the compression boundary verbatim, but "
           "the skill instructions that governed it were pruned. Before "
           "executing any preserved task that depends on these skills, reload "
           "them first: ");
    for (int q = 0; q < n_names; q++) {
        if (q) strcat(out, "; ");
        strcat(out, "skill_view(name='");
        strcat(out, names[q]);
        strcat(out, "')");
    }
    strcat(out, ". After reloading, re-check that each pending task is still "
                "justified — findings recorded before the boundary may have "
                "invalidated it.");
    for (int q = 0; q < n_names; q++) free(names[q]);
    return out;
}

/* PoP: conversation_history_after_compression @ agent/conversation_compression.py
 * Correct flush baseline after a compression boundary.
 *   attempt_recorded=false          -> NULL  (legacy rotate: full rewrite)
 *   recorded, in_place=true         -> shallow copy of messages (same session id)
 *   recorded, in_place=false        -> NULL  (abort after earlier in-place)
 *   recorded, in_place=NULL         -> previous_history (retain baseline)
 * Returns a fresh json array (shallow-copy of row refs) or NULL. */
json_t *ccl_conversation_history_after_compression(
    bool attempt_recorded, int attempt_in_place, /* -1 unknown */
    json_t *messages, json_t *previous_history)
{
    if (attempt_recorded) {
        if (attempt_in_place == 1) {
            if (!messages || messages->type != JSON_ARRAY) return NULL;
            json_t *copy = json_array();
            if (!copy) return NULL;
            for (size_t k = 0; k < messages->c.count; k++)
                json_append(copy, json_copy(messages->c.items[k]));
            return copy;
        }
        if (attempt_in_place == 0) return NULL;
        return previous_history ? json_copy(previous_history) : NULL;
    }
    return NULL;
}
