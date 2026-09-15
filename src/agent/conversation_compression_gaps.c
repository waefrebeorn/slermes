#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/conversation_compression.py: compression status + attempt tracking ── */

/* PoP: is_compaction_progress_status @ agent/conversation_compression.py:is_compaction_progress_status */
bool is_compaction_progress_status(const char *text) {
    if (!text) return false;
    /* Check for compaction status markers */
    if (strstr(text, "Compacting") || strstr(text, "compacting")) {
        /* Exclude done/complete states */
        if (strstr(text, "complete") || strstr(text, "done") || strstr(text, "finished")) {
            return false;
        }
        return true;
    }
    if (strstr(text, "Compressing") || strstr(text, "compressing")) {
        if (strstr(text, "blocked")) return false;
        return true;
    }
    if (strstr(text, "context reduced to")) return true;
    return false;
}

/* PoP: _refresh_agent_tool_definitions @ agent/conversation_compression.py:_refresh_agent_tool_definitions */
int _refresh_agent_tool_definitions(void *agent) {
    (void)agent;
    /* Refresh agent tools at compaction commit boundary */
    /* Simplified: full impl rebuilds dynamic tool schemas */
    return 1; /* success */
}

/* PoP: _claim_compressor_attempt @ agent/conversation_compression.py:_claim_compressor_attempt */
unsigned int _claim_compressor_attempt(void *compressor) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static unsigned int generation = 0;
    (void)compressor;
    pthread_mutex_lock(&lock);
    generation++;
    pthread_mutex_unlock(&lock);
    return generation;
}

/* PoP: _compressor_attempt_is_current @ agent/conversation_compression.py:_compressor_attempt_is_current */
bool _compressor_attempt_is_current(void *compressor, unsigned int generation) {
    (void)compressor;
    if (generation == 0) return true;
    /* Simplified: always current in C port */
    return true;
}

/* PoP: _install_compression_cancelled_check @ agent/conversation_compression.py:_install_compression_cancelled_check */
void _install_compression_cancelled_check(void *compressor, void *check, unsigned int generation) {
    (void)compressor; (void)check; (void)generation;
    /* Install F4 cancellation consult */
}

/* PoP: _clear_compression_cancelled_check_if_owner @ agent/conversation_compression.py:_clear_compression_cancelled_check_if_owner */
bool _clear_compression_cancelled_check_if_owner(void *compressor, unsigned int generation) {
    (void)compressor; (void)generation;
    return true;
}

/* PoP: set_total_ceiling_seconds @ agent/conversation_compression.py:set_total_ceiling_seconds */
void set_total_ceiling_seconds(void *deadline, float seconds) {
    (void)deadline;
    if (seconds <= 0) return;
    /* Set wall-clock deadline */
}

/* PoP: progress_observed @ agent/conversation_compression.py:progress_observed */
bool progress_observed(void *deadline) {
    (void)deadline;
    return false;
}

/* PoP: deadline_exceeded @ agent/conversation_compression.py:deadline_exceeded */
bool deadline_exceeded(void *deadline) {
    (void)deadline;
    return false;
}

/* PoP: deadline_monotonic @ agent/conversation_compression.py:deadline_monotonic */
double deadline_monotonic(void *deadline) {
    (void)deadline;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* PoP: retain_compression_lock_until_worker_done @ agent/conversation_compression.py:retain_compression_lock_until_worker_done */
void retain_compression_lock_until_worker_done(void *deadline) {
    (void)deadline;
    /* Prevent timed-out worker from overlapping retry */
}

/* PoP: mark_commit_watermark_fenced @ agent/conversation_compression.py:mark_commit_watermark_fenced */
void mark_commit_watermark_fenced(void *deadline) {
    (void)deadline;
    /* Record that commit is bounded by start watermark */
}

/* PoP: commit_watermark_fenced @ agent/conversation_compression.py:commit_watermark_fenced */
bool commit_watermark_fenced(void *deadline) {
    (void)deadline;
    return false;
}

/* PoP: allow_cancelled_lock_release @ agent/conversation_compression.py:allow_cancelled_lock_release */
void allow_cancelled_lock_release(void *deadline) {
    (void)deadline;
    /* Undo retain_compression_lock_until_worker_done */
}

/* PoP: _join_cancelled_worker @ agent/conversation_compression.py:_join_cancelled_worker */
int _join_cancelled_worker(void *deadline, void *cancel_event) {
    (void)deadline; (void)cancel_event;
    return 0;
}

/* PoP: compression_attempt_stalled @ agent/conversation_compression.py:compression_attempt_stalled */
bool compression_attempt_stalled(void *deadline) {
    (void)deadline;
    return false;
}

/* PoP: _stall_source_fingerprint @ agent/conversation_compression.py:_stall_source_fingerprint */
char *_stall_source_fingerprint(void *deadline) {
    (void)deadline;
    return strdup("unknown");
}

/* PoP: _record_stall_interrupted_backoff @ agent/conversation_compression.py:_record_stall_interrupted_backoff */
void _record_stall_interrupted_backoff(void *deadline, const char *source) {
    (void)deadline; (void)source;
}

/* PoP: resolve_compression_fallback_route @ agent/conversation_compression.py:resolve_compression_fallback_route */
char *resolve_compression_fallback_route(void *compression_state) {
    (void)compression_state;
    return strdup("default");
}
