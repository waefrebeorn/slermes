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

#include "hive.h"
static hive_t *_dedup_seen = NULL;   /* of DedupEntry* (heap) */
static int _dedup_count = 0;
static double _dedup_ttl = 300.0;
static int _dedup_max_size = 2000;

/* PoP: helpers_is_duplicate @ gateway/platforms/helpers.py:is_duplicate */

/* Port of Python gateway/platforms/helpers.py:is_duplicate
 * Hive-backed seen-map: only live msg_ids consume memory (was
 * _dedup_seen[2000] = 265KB of .bss). */
/* PoP: _is_duplicate @ gateway/platforms/qqbot/adapter.py:_is_duplicate */
int helpers_is_duplicate(const char *msg_id)
{
    if (!msg_id || !msg_id[0]) return 0;

    double now = (double)time(NULL);
    if (!_dedup_seen) _dedup_seen = hive_new(32);

    /* Check if seen */
    hive_iter_t it;
    hive_iter_begin(_dedup_seen, &it);
    hive_handle_t hnd;
    DedupEntry *e;
    while (hive_iter_next(_dedup_seen, &it, &hnd, (void **)&e)) {
        if (strcmp(e->msg_id, msg_id) == 0) {
            if (now - e->timestamp < _dedup_ttl) {
                return 1; /* Duplicate */
            }
            /* Expired — remove */
            free(e);
            hive_erase(_dedup_seen, hnd);
            _dedup_count--;
            break;
        }
    }

    /* Add new entry */
    if (_dedup_count < _dedup_max_size) {
        DedupEntry *ne = calloc(1, sizeof(DedupEntry));
        if (ne) {
            strncpy(ne->msg_id, msg_id, 127);
            ne->msg_id[127] = '\0';
            ne->timestamp = now;
            bool ok = false;
            hive_insert(_dedup_seen, ne, &ok);
            if (ok) _dedup_count++;
            else free(ne);
        }
    }

    /* Prune if over max size */
    if (_dedup_count > _dedup_max_size) {
        double cutoff = now - _dedup_ttl;
        hive_iter_t it2;
        hive_iter_begin(_dedup_seen, &it2);
        hive_handle_t h2;
        DedupEntry *e2;
        while (hive_iter_next(_dedup_seen, &it2, &h2, (void **)&e2)) {
            if (e2->timestamp <= cutoff) {
                free(e2);
                hive_erase(_dedup_seen, h2);
                _dedup_count--;
            }
        }
        /* Python: if TTL pruning alone does not cap the cache (every entry
         * still fresh), keep the NEWEST max_size entries — the bound must
         * hold under sustained traffic. Evict oldest live entries. */
        while (_dedup_count > _dedup_max_size) {
            hive_handle_t oldest_h = { 0, 0 };
            DedupEntry *oldest = NULL;
            double oldest_ts = 1e300;
            hive_iter_t it3;
            hive_iter_begin(_dedup_seen, &it3);
            hive_handle_t h3;
            DedupEntry *e3;
            while (hive_iter_next(_dedup_seen, &it3, &h3, (void **)&e3)) {
                if (e3->timestamp < oldest_ts) {
                    oldest_ts = e3->timestamp;
                    oldest_h = h3;
                    oldest = e3;
                }
            }
            if (!oldest) break;
            free(oldest);
            hive_erase(_dedup_seen, oldest_h);
            _dedup_count--;
        }
    }

    return 0;
}

/* ─── Text Batch Aggregation ───────────────────────────────────────────── */

/* Real pending batch: each enqueued (key, text) fragment is appended to a
 * hive; flush/cancel operate on the live entries. */
typedef struct {
    char key[128];
    char text[2048];
    int text_len;
} BatchEntry;

#include "hive.h"
static hive_t *_batch = NULL;   /* of BatchEntry* (heap) */
static pthread_mutex_t _batch_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: helpers_enqueue @ gateway/platforms/helpers.py:enqueue */

/* Port of Python gateway/platforms/helpers.py:enqueue
 * Python: pending is a dict keyed by `key`; a second event for the same
 * key APPENDS to the existing text with a newline (never a duplicate
 * entry). Mirror that exactly: find-or-insert by key, merge text. */
void helpers_enqueue(const char *key, const char *text, int text_len)
{
    if (!key || !key[0] || !text || text_len <= 0) return;

    pthread_mutex_lock(&_batch_lock);
    if (!_batch) _batch = hive_new(16);

    BatchEntry *e = NULL;
    hive_handle_t hnd = { 0, 0 };
    hive_iter_t it;
    hive_iter_begin(_batch, &it);
    hive_handle_t h;
    BatchEntry *cand;
    while (hive_iter_next(_batch, &it, &h, (void **)&cand)) {
        if (strcmp(cand->key, key) == 0) { e = cand; hnd = h; break; }
    }

    if (e) {
        /* existing: text = f"{existing.text}\n{event.text}" */
        int copy = text_len < (int)sizeof(e->text) - 1 ? text_len : (int)sizeof(e->text) - 1;
        int used = e->text_len;
        if (used > 0 && used + 1 + copy < (int)sizeof(e->text)) {
            e->text[used] = '\n';
            memcpy(e->text + used + 1, text, copy);
            e->text[used + 1 + copy] = '\0';
            e->text_len = used + 1 + copy;
        } else if (used == 0) {
            memcpy(e->text, text, copy);
            e->text[copy] = '\0';
            e->text_len = copy;
        }
        /* else: full — keep as-is (Python has no cap; C buffer-bound) */
        pthread_mutex_unlock(&_batch_lock);
        return;
    }

    /* new entry for this key */
    e = calloc(1, sizeof(BatchEntry));
    if (e) {
        strncpy(e->key, key, sizeof(e->key) - 1);
        e->key[sizeof(e->key) - 1] = '\0';
        int copy = text_len < (int)sizeof(e->text) - 1 ? text_len : (int)sizeof(e->text) - 1;
        memcpy(e->text, text, copy);
        e->text[copy] = '\0';
        e->text_len = copy;
        bool ok = false;
        hive_insert(_batch, e, &ok);
        if (!ok) free(e);
    }
    pthread_mutex_unlock(&_batch_lock);

    hermes_log(LOG_DEBUG, "helpers", "Enqueued text for key=%s len=%d (batch=%zu)", key, text_len, _batch ? hive_count(_batch) : 0);
}

/* PoP: helpers_cancel_all @ gateway/platforms/helpers.py:cancel_all */

/* Port of Python gateway/platforms/helpers.py:cancel_all */
/* Cancel all pending flush tasks by clearing the batch hive. */
void helpers_cancel_all(void)
{
    pthread_mutex_lock(&_batch_lock);
    int cleared = 0;
    if (_batch) {
        cleared = (int)hive_count(_batch);
        hive_iter_t it;
        hive_iter_begin(_batch, &it);
        hive_handle_t hnd;
        BatchEntry *e;
        while (hive_iter_next(_batch, &it, &hnd, (void **)&e)) {
            free(e);
            hive_erase(_batch, hnd);
        }
    }
    pthread_mutex_unlock(&_batch_lock);

    hermes_log(LOG_DEBUG, "helpers", "Cancelled all pending flush tasks (%d cleared)", cleared);
}

/* ─── Thread Participation Tracking ─────────────────────────────────────── */

typedef struct {
    char thread_id[128];
} ThreadEntry;

static hive_t *_threads = NULL;   /* of ThreadEntry* (heap) */
static int _thread_count = 0;

/* PoP: helpers_mark @ gateway/platforms/helpers.py:mark */

/* Port of Python gateway/platforms/helpers.py:mark */
/* Mark thread_id as participated and persist. */
void helpers_mark(const char *thread_id)
{
    if (!thread_id || !thread_id[0]) return;

    /* Check if already tracked */
    if (_threads) {
        hive_iter_t it;
        hive_iter_begin(_threads, &it);
        ThreadEntry *t;
        while (hive_iter_next(_threads, &it, NULL, (void **)&t)) {
            if (strcmp(t->thread_id, thread_id) == 0) return;
        }
    }

    /* Add new thread */
    if (!_threads) _threads = hive_new(16);
    ThreadEntry *t = calloc(1, sizeof(ThreadEntry));
    if (t) {
        strncpy(t->thread_id, thread_id, 127);
        t->thread_id[127] = '\0';
        bool ok = false;
        hive_insert(_threads, t, &ok);
        if (ok) {
            _thread_count++;
            hermes_log(LOG_DEBUG, "helpers", "Marked thread %s (total: %d)", thread_id, _thread_count);
        } else {
            free(t);
        }
    }

    /* Persist to file (simplified) */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[512];
    snprintf(path, sizeof(path), "%s/.hermes/threads.json", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "[");
        int first = 1;
        if (_threads) {
            hive_iter_t it;
            hive_iter_begin(_threads, &it);
            ThreadEntry *t;
            while (hive_iter_next(_threads, &it, NULL, (void **)&t)) {
                if (!first) fprintf(f, ",");
                fprintf(f, "\"%s\"", t->thread_id);
                first = 0;
            }
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
    if (!_threads) return 0;

    hive_iter_t it;
    hive_iter_begin(_threads, &it);
    ThreadEntry *t;
    while (hive_iter_next(_threads, &it, NULL, (void **)&t)) {
        if (strcmp(t->thread_id, thread_id) == 0) return 1;
    }
    return 0;
}
