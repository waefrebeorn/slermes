/*
 * port_gateway_platforms_helpers.c — C port of gateway/platforms/helpers.py
 *
 * Shared helper classes for gateway platform adapters.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_DEDUP_SEEN 2000
#define MAX_THREADS 500

/* ─── Message Deduplication ─────────────────────────────────────────────── */

typedef struct {
    char msg_id[128];
    double timestamp;
} DedupEntry;

static DedupEntry _dedup_seen[MAX_DEDUP_SEEN];
static int _dedup_count = 0;
static double _dedup_ttl = 300.0;
static int _dedup_max_size = 2000;

/* PoP: helpers_is_duplicate @ gateway/platforms/helpers.py:is_duplicate */

/* Port of Python gateway/platforms/helpers.py:is_duplicate */
/* Return 1 if msg_id was already seen within the TTL window. */
/* PoP: _is_duplicate @ gateway/platforms/qqbot/adapter.py:_is_duplicate */
int helpers_is_duplicate(const char *msg_id)
{
    if (!msg_id || !msg_id[0]) return 0;

    double now = (double)time(NULL);

    /* Check if seen */
    for (int i = 0; i < _dedup_count; i++) {
        if (strcmp(_dedup_seen[i].msg_id, msg_id) == 0) {
            if (now - _dedup_seen[i].timestamp < _dedup_ttl) {
                return 1; /* Duplicate */
            }
            /* Expired — remove */
            for (int j = i; j < _dedup_count - 1; j++) {
                _dedup_seen[j] = _dedup_seen[j + 1];
            }
            _dedup_count--;
            break;
        }
    }

    /* Add new entry */
    if (_dedup_count < _dedup_max_size) {
        strncpy(_dedup_seen[_dedup_count].msg_id, msg_id, 127);
        _dedup_seen[_dedup_count].msg_id[127] = '\0';
        _dedup_seen[_dedup_count].timestamp = now;
        _dedup_count++;
    }

    /* Prune if over max size */
    if (_dedup_count > _dedup_max_size) {
        double cutoff = now - _dedup_ttl;
        int write = 0;
        for (int i = 0; i < _dedup_count; i++) {
            if (_dedup_seen[i].timestamp > cutoff) {
                if (write != i) _dedup_seen[write] = _dedup_seen[i];
                write++;
            }
        }
        _dedup_count = write;
    }

    return 0;
}

/* ─── Text Batch Aggregation ───────────────────────────────────────────── */

/* Real pending batch: each enqueued (key, text) fragment is appended to a
 * bounded ring buffer; flush/cancel operate on this buffer. */
#define HELPERS_BATCH_MAX 256
typedef struct {
    char key[128];
    char text[2048];
    int text_len;
} BatchEntry;

static BatchEntry _batch[HELPERS_BATCH_MAX];
static int _batch_count = 0;
static pthread_mutex_t _batch_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: helpers_enqueue @ gateway/platforms/helpers.py:enqueue */

/* Port of Python gateway/platforms/helpers.py:enqueue */
/* Append event text to the pending batch for key. */
void helpers_enqueue(const char *key, const char *text, int text_len)
{
    if (!key || !key[0] || !text || text_len <= 0) return;

    pthread_mutex_lock(&_batch_lock);
    if (_batch_count < HELPERS_BATCH_MAX) {
        BatchEntry *e = &_batch[_batch_count++];
        strncpy(e->key, key, sizeof(e->key) - 1);
        e->key[sizeof(e->key) - 1] = '\0';
        int copy = text_len < (int)sizeof(e->text) - 1 ? text_len : (int)sizeof(e->text) - 1;
        memcpy(e->text, text, copy);
        e->text[copy] = '\0';
        e->text_len = copy;
    }
    pthread_mutex_unlock(&_batch_lock);

    hermes_log(LOG_DEBUG, "helpers", "Enqueued text for key=%s len=%d (batch=%d)", key, text_len, _batch_count);
}

/* PoP: helpers_cancel_all @ gateway/platforms/helpers.py:cancel_all */

/* Port of Python gateway/platforms/helpers.py:cancel_all */
/* Cancel all pending flush tasks by clearing the batch buffer. */
void helpers_cancel_all(void)
{
    pthread_mutex_lock(&_batch_lock);
    int cleared = _batch_count;
    _batch_count = 0;
    pthread_mutex_unlock(&_batch_lock);

    hermes_log(LOG_DEBUG, "helpers", "Cancelled all pending flush tasks (%d cleared)", cleared);
}

/* ─── Thread Participation Tracking ─────────────────────────────────────── */

typedef struct {
    char thread_id[128];
} ThreadEntry;

static ThreadEntry _threads[MAX_THREADS];
static int _thread_count = 0;

/* PoP: helpers_mark @ gateway/platforms/helpers.py:mark */

/* Port of Python gateway/platforms/helpers.py:mark */
/* Mark thread_id as participated and persist. */
void helpers_mark(const char *thread_id)
{
    if (!thread_id || !thread_id[0]) return;

    /* Check if already tracked */
    for (int i = 0; i < _thread_count; i++) {
        if (strcmp(_threads[i].thread_id, thread_id) == 0) return;
    }

    /* Add new thread */
    if (_thread_count < MAX_THREADS) {
        strncpy(_threads[_thread_count].thread_id, thread_id, 127);
        _threads[_thread_count].thread_id[127] = '\0';
        _thread_count++;
        hermes_log(LOG_DEBUG, "helpers", "Marked thread %s (total: %d)", thread_id, _thread_count);
    }

    /* Persist to file (simplified) */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[512];
    snprintf(path, sizeof(path), "%s/.hermes/threads.json", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "[");
        for (int i = 0; i < _thread_count; i++) {
            if (i > 0) fprintf(f, ",");
            fprintf(f, "\"%s\"", _threads[i].thread_id);
        }
        fprintf(f, "]");
        fclose(f);
    }
}

/* PoP: helpers___contains__ @ gateway/platforms/helpers.py:__contains__ */

/* Port of Python gateway/platforms/helpers.py:__contains__ */
/* Check if thread_id is in the tracked set. Returns 1 if found, 0 otherwise. */
int helpers___contains__(const char *thread_id)
{
    if (!thread_id || !thread_id[0]) return 0;

    for (int i = 0; i < _thread_count; i++) {
        if (strcmp(_threads[i].thread_id, thread_id) == 0) return 1;
    }
    return 0;
}
