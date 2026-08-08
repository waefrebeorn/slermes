/*
 * port_streaming_tts_consumer.c — C11 port of gateway/streaming_tts_consumer.py
 *
 * Bridges synchronous LLM text deltas (fired from the agent worker thread)
 * to a platform adapter's streaming-audio contract, so PCM playback begins
 * while the LLM is still generating.
 *
 * Design:
 * - on_delta() is sync, non-blocking; feeds deltas into a stateful
 *   SentenceChunker (stts_chunker_t from port_tts_streaming_helpers.h) and
 *   pushes completed clauses onto a thread-safe bounded queue (pthread).
 * - A daemon drain thread replaces the asyncio task; drains the queue,
 *   synthesises each clause via a StreamingTTSProvider, writes PCM to the
 *   adapter via the vtable.
 * - Per-turn state is isolated: each consumer owns its chunker, queue, handle.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "hermes_logger.h"
#include "libjson/json.h"
#include "port_streaming_tts_consumer.h"
#include "port_tts_streaming_helpers.h"

/* ── Sentinel values (mirror Python _ABORT, _DONE) ───────────────────── */
typedef enum {
    STTS_ITEM_CLAUSE,
    STTS_ITEM_DONE,
    STTS_ITEM_ABORT,
} stts_queue_item_type;

typedef struct {
    stts_queue_item_type type;
    char *clause;
} stts_queue_item_t;

#define STTS_QUEUE_MAXSIZE 256

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
    stts_queue_item_t *items[STTS_QUEUE_MAXSIZE];
    size_t head, tail, count;
    bool closed;
} stts_queue_t;

/* ── StreamingTTSConsumer ──────────────────────────────────────────────── */
/* PoP: __init__ @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.__init__ */
struct stts_consumer {
    void *adapter;
    stts_adapter_vtable_t *vtable;
    char *chat_id;
    json_t *tts_config;
    json_t *metadata;

    stts_provider_t *provider;
    stts_audio_format_t audio_format;

    stts_queue_t queue;
    stts_tts_handle_t *handle;

    /* Per-turn flags */
    bool started, completed, partial, aborted, finished, dropped;
    bool suppress_whole_file;

    pthread_t drain_thread;
    bool drain_thread_running;
    pthread_mutex_t lock;  /* abort() idempotency */

    /* Stateful sentence chunker */
    stts_chunker_t *chunker;
    long chunker_min_len;

    /* Strip-markdown helper */
    char *(*strip_markdown_fn)(const char *text);

    /* Drain thread control */
    bool drain_done;
    pthread_cond_t drain_done_cond;
    pthread_mutex_t drain_done_lock;
};

/* ── Queue operations ──────────────────────────────────────────────────── */
static int stts_queue_init(stts_queue_t *q) {
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    q->head = q->tail = q->count = 0;
    q->closed = false;
    return 0;
}

static void stts_queue_destroy(stts_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    for (size_t i = 0; i < q->count; i++) {
        stts_queue_item_t *item = q->items[(q->head + i) % STTS_QUEUE_MAXSIZE];
        if (item->clause) free(item->clause);
        free(item);
    }
    q->head = q->tail = q->count = 0;
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

static bool stts_queue_put_nowait(stts_queue_t *q, stts_queue_item_type type,
                                   const char *clause) {
    pthread_mutex_lock(&q->lock);
    if (q->count >= STTS_QUEUE_MAXSIZE) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    stts_queue_item_t *item = malloc(sizeof(*item));
    if (!item) { pthread_mutex_unlock(&q->lock); return false; }
    item->type = type;
    item->clause = clause ? strdup(clause) : NULL;
    size_t idx = q->tail;
    q->items[idx] = item;
    q->tail = (idx + 1) % STTS_QUEUE_MAXSIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return true;
}

static stts_queue_item_t *stts_queue_get_timed(stts_queue_t *q,
                                                const struct timespec *abs_timeout) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->closed) {
        int rc = pthread_cond_timedwait(&q->not_empty, &q->lock, abs_timeout);
        if (rc == ETIMEDOUT) { pthread_mutex_unlock(&q->lock); return NULL; }
    }
    if (q->count == 0) { pthread_mutex_unlock(&q->lock); return NULL; }
    stts_queue_item_t *item = q->items[q->head];
    q->head = (q->head + 1) % STTS_QUEUE_MAXSIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return item;
}

static void stts_queue_drain(stts_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    for (size_t i = 0; i < q->count; i++) {
        stts_queue_item_t *item = q->items[(q->head + i) % STTS_QUEUE_MAXSIZE];
        if (item->clause) free(item->clause);
        free(item);
    }
    q->head = q->tail = q->count = 0;
    pthread_mutex_unlock(&q->lock);
}

/* ── Forward declarations ──────────────────────────────────────────────── */
static void stts_safe_abort(stts_consumer_t *c, const char *reason);
static long stts_message_char_len(const char *text);

/* ── Audio format helpers ──────────────────────────────────────────────── */
/* PoP: AudioFormat.__init__ @ gateway/platforms/base.py:AudioFormat.__init__ */
static stts_audio_format_t stts_audio_format_default(void) {
    stts_audio_format_t fmt = {24000, 1, 2};
    return fmt;
}

static stts_audio_format_t stts_audio_format_from_provider(stts_provider_t *p,
                                                            stts_audio_format_t fb) {
    if (!p) return fb;
    stts_audio_format_t fmt;
    fmt.sample_rate = p->sample_rate > 0 ? p->sample_rate : fb.sample_rate;
    fmt.channels = p->channels > 0 ? p->channels : fb.channels;
    fmt.sample_width = p->sample_width > 0 ? p->sample_width : fb.sample_width;
    return fmt;
}

/* ── Strip markdown for TTS (lazy) ─────────────────────────────────────── */
/* PoP: _strip_markdown_for_tts @ gateway/streaming_tts_consumer.py:_strip_markdown_for_tts */
static char *stts_strip_markdown_for_tts(stts_consumer_t *c, const char *text) {
    if (!text) return NULL;
    char *stripped = c && c->strip_markdown_fn
        ? c->strip_markdown_fn(text) : NULL;
    if (!stripped) stripped = strdup(text);
    /* .strip() */
    char *s = stripped;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\n' || end[-1] == '\r')) end--;
    char *result = strndup(s, end - s);
    free(stripped);
    return result;
}

/* ── Properties ───────────────────────────────────────────────────────── */
/* PoP: active @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.active */
bool stts_consumer_active(const stts_consumer_t *c) {
    return c && c->provider != NULL;
}
/* PoP: completed @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.completed */
bool stts_consumer_completed(const stts_consumer_t *c) { return c && c->completed; }
/* PoP: partial @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.partial */
bool stts_consumer_partial(const stts_consumer_t *c) { return c && c->partial; }
/* PoP: started @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.started */
bool stts_consumer_started(const stts_consumer_t *c) { return c && c->started; }
/* PoP: audible @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.audible */
bool stts_consumer_audible(const stts_consumer_t *c) { return c && c->handle && c->handle->audible; }
/* PoP: dropped @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.dropped */
bool stts_consumer_dropped(const stts_consumer_t *c) { return c && c->dropped; }
/* PoP: suppress_whole_file @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.suppress_whole_file */
bool stts_consumer_suppress_whole_file(const stts_consumer_t *c) { return c && c->suppress_whole_file; }
/* PoP: done @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.done */
bool stts_consumer_done(const stts_consumer_t *c) {
    return c && !c->drain_thread_running && c->drain_done;
}

/* ── Sync callbacks (agent worker thread) ─────────────────────────────── */

/* PoP: on_delta @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.on_delta */
void stts_consumer_on_delta(stts_consumer_t *c, const char *text) {
    if (!c || !text) return;
    if (c->aborted || !stts_consumer_active(c) || c->finished) return;
    char buf_state[64];
    char *clauses = stts_chunker_feed(c->chunker, text, (char **)&buf_state);
    if (!clauses || !clauses[0]) { free(clauses); return; }
    /* Each \n-separated line is a completed clause */
    char *dup = strdup(clauses);
    free(clauses);
    char *saveptr = NULL;
    char *line = strtok_r(dup, "\n", &saveptr);
    while (line) {
        if (strlen(line) > 0) {
            if (!stts_queue_put_nowait(&c->queue, STTS_ITEM_CLAUSE, line))
                c->dropped = true;
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(dup);
}

/* PoP: finish @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.finish */
void stts_consumer_finish(stts_consumer_t *c) {
    if (!c || c->finished) return;
    c->finished = true;
    if (c->aborted || !stts_consumer_active(c)) return;
    /* Flush chunker tail */
    char *clauses = stts_chunker_flush(c->chunker);
    if (clauses && clauses[0]) {
        char *dup = strdup(clauses);
        char *saveptr = NULL;
        char *line = strtok_r(dup, "\n", &saveptr);
        while (line) {
            if (strlen(line) > 0) {
                if (!stts_queue_put_nowait(&c->queue, STTS_ITEM_CLAUSE, line))
                    c->dropped = true;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
        free(dup);
    }
    free(clauses);
    stts_consumer_enqueue_done(c);
}

/* PoP: _enqueue_done @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer._enqueue_done */
void stts_consumer_enqueue_done(stts_consumer_t *c) {
    if (!c) return;
    while (true) {
        if (stts_queue_put_nowait(&c->queue, STTS_ITEM_DONE, NULL)) return;
        /* queue full — drain one to make room */
        pthread_mutex_lock(&c->queue.lock);
        if (c->queue.count > 0) {
            stts_queue_item_t *old = c->queue.items[c->queue.head];
            c->queue.head = (c->queue.head + 1) % STTS_QUEUE_MAXSIZE;
            c->queue.count--;
            if (old->clause) free(old->clause);
            free(old);
            c->dropped = true;
        }
        pthread_mutex_unlock(&c->queue.lock);
    }
}

/* PoP: start @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.start */
void stts_consumer_start(stts_consumer_t *c) {
    if (!c || c->drain_thread_running) return;
    c->drain_thread_running = true;
    c->drain_done = false;
    if (pthread_create(&c->drain_thread, NULL, stts_drain_thread, c) != 0) {
        c->drain_thread_running = false;
        hermes_log(LOG_WARNING, "streaming_tts_consumer",
                   "Failed to start streaming TTS drain thread");
        return;
    }
    pthread_detach(c->drain_thread);
}

/* ── Drain thread ──────────────────────────────────────────────────────── */

/* PoP: _synthesise_and_write @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer._synthesise_and_write */
void stts_synthesise_and_write(stts_consumer_t *c, const char *clause) {
    if (!c || !c->handle || c->handle->aborted || !c->provider) return;

    char *cleaned = stts_strip_markdown_for_tts(c, clause);
    if (!cleaned || !cleaned[0] || cleaned[0] == '\n') {
        free(cleaned); return;
    }
    bool only_ws = true;
    for (char *p = cleaned; *p; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            only_ws = false; break;
        }
    }
    if (only_ws) { free(cleaned); return; }

    void *chunks[64];
    size_t lens[64];
    memset(chunks, 0, sizeof(chunks));
    memset(lens, 0, sizeof(lens));
    int n = c->provider->stream(c->provider->provider_ctx, cleaned,
                                chunks, lens, 64);
    free(cleaned);
    if (n < 0) return;

    for (int i = 0; i < n; i++) {
        if (c->aborted || (c->handle && c->handle->aborted)) {
            for (int j = i; j < n; j++) free(chunks[j]);
            return;
        }
        if (lens[i] == 0) { free(chunks[i]); continue; }
        bool was_audible = c->handle->audible;
        if (c->vtable->write_streaming_tts)
            c->vtable->write_streaming_tts(c->adapter, c->handle, chunks[i], lens[i]);
        if (!was_audible) {
            c->handle->audible = true;
            c->suppress_whole_file = true;
        }
        free(chunks[i]);
    }
}

/* PoP: _run @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer._run */
void *stts_drain_thread(void *arg) {
    stts_consumer_t *c = (stts_consumer_t *)arg;
    if (!c || !stts_consumer_active(c)) {
        pthread_mutex_lock(&c->drain_done_lock);
        c->drain_thread_running = false;
        c->drain_done = true;
        pthread_cond_broadcast(&c->drain_done_cond);
        pthread_mutex_unlock(&c->drain_done_lock);
        return NULL;
    }

    if (!c->vtable->supports_streaming_tts(c->adapter, c->chat_id, &c->audio_format)) {
        pthread_mutex_lock(&c->drain_done_lock);
        c->drain_thread_running = false;
        c->drain_done = true;
        pthread_cond_broadcast(&c->drain_done_cond);
        pthread_mutex_unlock(&c->drain_done_lock);
        return NULL;
    }

    c->handle = c->vtable->begin_streaming_tts(c->adapter, c->chat_id,
                                                &c->audio_format, c->metadata);
    if (!c->handle) {
        pthread_mutex_lock(&c->drain_done_lock);
        c->drain_thread_running = false;
        c->drain_done = true;
        pthread_cond_broadcast(&c->drain_done_cond);
        pthread_mutex_unlock(&c->drain_done_lock);
        return NULL;
    }
    c->started = true;
    c->suppress_whole_file = false;

    bool got_normal_exit = false;
    while (true) {
        if (c->aborted) break;

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000;  /* 0.1s */
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }

        stts_queue_item_t *item = stts_queue_get_timed(&c->queue, &ts);
        if (!item) continue;

        if (item->type == STTS_ITEM_ABORT) {
            free(item->clause); free(item); break;
        }
        if (item->type == STTS_ITEM_DONE) {
            free(item->clause); free(item); got_normal_exit = true; break;
        }
        if (item->type != STTS_ITEM_CLAUSE || !item->clause) {
            free(item->clause); free(item); continue;
        }

        char *clause = item->clause;
        free(item);

        if (c->aborted) { free(clause); break; }

        /* Synthesise and write — check for provider errors */
        void *chunks[64];
        size_t lens[64];
        memset(chunks, 0, sizeof(chunks));
        memset(lens, 0, sizeof(lens));
        int n = c->provider->stream(c->provider->provider_ctx, clause,
                                    chunks, lens, 64);
        free(clause);

        if (n < 0) {
            /* clause failed */
            if (c->handle && c->handle->audible) {
                c->partial = true; c->suppress_whole_file = true;
            } else {
                c->suppress_whole_file = false;
            }
            c->completed = false;
            stts_safe_abort(c, "synthesise failed");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (c->aborted || (c->handle && c->handle->aborted)) {
                for (int j = i; j < n; j++) free(chunks[j]);
                goto cleanup;
            }
            if (lens[i] == 0) { free(chunks[i]); continue; }
            bool was_audible = c->handle->audible;
            if (c->vtable->write_streaming_tts)
                c->vtable->write_streaming_tts(c->adapter, c->handle, chunks[i], lens[i]);
            free(chunks[i]);
            if (!was_audible) {
                c->handle->audible = true;
                c->suppress_whole_file = true;
            }
        }
    }

cleanup:
    if (!c->aborted && c->handle != NULL) {
        if (c->vtable->finish_streaming_tts)
            c->vtable->finish_streaming_tts(c->adapter, c->handle, c->aborted);

        if (c->handle->audible && !c->dropped) {
            c->completed = true;
            c->suppress_whole_file = true;
        } else if (c->handle->audible && c->dropped) {
            c->partial = true;
            c->completed = false;
            c->suppress_whole_file = true;
        } else {
            c->completed = false;
            c->suppress_whole_file = false;
        }
    }

    stts_safe_abort(c, c->aborted ? "cancelled" : NULL);
    stts_queue_drain(&c->queue);

    pthread_mutex_lock(&c->drain_done_lock);
    c->drain_thread_running = false;
    c->drain_done = true;
    pthread_cond_broadcast(&c->drain_done_cond);
    pthread_mutex_unlock(&c->drain_done_lock);
    return NULL;
}

/* PoP: _safe_abort @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer._safe_abort */
static void stts_safe_abort(stts_consumer_t *c, const char *reason) {
    if (!c || !c->handle) return;
    if (c->vtable && c->vtable->abort_streaming_tts)
        c->vtable->abort_streaming_tts(c->adapter, c->handle,
                                       reason ? reason : "cancelled");
    c->handle->aborted = true;
}

/* ── Cancellation and completion ───────────────────────────────────────── */

/* PoP: abort @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.abort */
void stts_consumer_abort(stts_consumer_t *c, const char *reason) {
    if (!c) return;
    pthread_mutex_lock(&c->lock);
    if (c->aborted) { pthread_mutex_unlock(&c->lock); return; }
    c->aborted = true;
    pthread_mutex_unlock(&c->lock);

    for (int attempt = 0; attempt < 3; attempt++) {
        if (stts_queue_put_nowait(&c->queue, STTS_ITEM_ABORT, NULL)) break;
        pthread_mutex_lock(&c->queue.lock);
        if (c->queue.count > 0) {
            stts_queue_item_t *old = c->queue.items[c->queue.head];
            c->queue.head = (c->queue.head + 1) % STTS_QUEUE_MAXSIZE;
            c->queue.count--;
            if (old->clause) free(old->clause);
            free(old);
            c->dropped = true;
        }
        pthread_mutex_unlock(&c->queue.lock);
    }

    /* If drain thread already stopped, call abort directly */
    if (c->handle != NULL && !c->handle->aborted && !c->drain_thread_running) {
        stts_safe_abort(c, reason ? reason : "cancelled");
    }
}

/* PoP: wait_complete @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.wait_complete */
bool stts_consumer_wait_complete(stts_consumer_t *c, double timeout_secs) {
    if (!c) return false;
    if (!c->drain_thread_running) return c->completed;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout_secs;
    long ns = (long)((timeout_secs - (time_t)timeout_secs) * 1e9);
    ts.tv_nsec += ns;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }

    pthread_mutex_lock(&c->drain_done_lock);
    while (!c->drain_done && !c->completed) {
        int rc = pthread_cond_timedwait(&c->drain_done_cond, &c->drain_done_lock, &ts);
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&c->drain_done_lock);
            return c->completed;
        }
    }
    pthread_mutex_unlock(&c->drain_done_lock);
    return c->completed;
}

/* ── Platform base.py methods ──────────────────────────────────────────── */

/* PoP: supports_streaming_tts @ gateway/platforms/base.py:supports_streaming_tts */
bool stts_supports_streaming_tts(stts_consumer_t *c, const char *chat_id,
                                  const stts_audio_format_t *fmt) {
    if (!c || !c->vtable) return false;
    if (c->vtable->supports_streaming_tts)
        return c->vtable->supports_streaming_tts(c->adapter, chat_id, fmt);
    return false;
}

/* PoP: begin_streaming_tts @ gateway/platforms/base.py:begin_streaming_tts */
stts_tts_handle_t *stts_begin_streaming_tts(stts_consumer_t *c, const char *chat_id,
                                             const stts_audio_format_t *fmt,
                                             json_t *metadata) {
    if (!c || !c->vtable) return NULL;
    if (c->vtable->begin_streaming_tts)
        return c->vtable->begin_streaming_tts(c->adapter, chat_id, fmt, metadata);
    return NULL;
}

/* PoP: write_streaming_tts @ gateway/platforms/base.py:write_streaming_tts */
int stts_write_streaming_tts(stts_consumer_t *c, stts_tts_handle_t *handle,
                              const void *pcm_data, size_t len) {
    if (!c || !c->vtable) return -1;
    if (c->vtable->write_streaming_tts)
        return c->vtable->write_streaming_tts(c->adapter, handle, pcm_data, len);
    return -1;
}

/* PoP: finish_streaming_tts @ gateway/platforms/base.py:finish_streaming_tts */
void stts_finish_streaming_tts(stts_consumer_t *c, stts_tts_handle_t *handle,
                                bool interrupted) {
    if (!c || !c->vtable) return;
    if (c->vtable->finish_streaming_tts)
        c->vtable->finish_streaming_tts(c->adapter, handle, interrupted);
}

/* PoP: abort_streaming_tts @ gateway/platforms/base.py:abort_streaming_tts */
void stts_abort_streaming_tts(stts_consumer_t *c, stts_tts_handle_t *handle,
                               const char *error) {
    if (!c || !c->vtable) return;
    if (c->vtable->abort_streaming_tts)
        c->vtable->abort_streaming_tts(c->adapter, handle, error);
}

/* ── Streaming TTS turn-key session bookkeeping ────────────────────────── */

/* PoP: _streaming_tts_turn_key @ gateway/platforms/base.py:_streaming_tts_turn_key */
/* PoP: streaming_tts_turn_key @ gateway/platforms/base.py:streaming_tts_turn_key */
char *stts_streaming_tts_turn_key_private(const char *chat_id, const char *session_id) {
    if (!chat_id) return NULL;
    char *key = NULL;
    if (session_id)
        asprintf(&key, "tts_streaming_turn:%s:%s", chat_id, session_id);
    else
        asprintf(&key, "tts_streaming_turn:%s", chat_id);
    return key;
}

char *stts_streaming_tts_turn_key(const char *chat_id, const char *session_id) {
    return stts_streaming_tts_turn_key_private(chat_id, session_id);
}

/* PoP: _streaming_tts_turn_completed @ gateway/platforms/base.py:_streaming_tts_turn_completed */
bool stts_streaming_tts_turn_completed(stts_consumer_t *c, const char *chat_id,
                                        const char *session_id) {
    (void)chat_id; (void)session_id;
    return c && c->completed;
}

/* PoP: _mark_streaming_tts_completed_turn @ gateway/platforms/base.py:_mark_streaming_tts_completed_turn */
void stts_mark_streaming_tts_completed_turn(stts_consumer_t *c, const char *chat_id,
                                             const char *session_id) {
    if (!c || !c->completed) return;
    char *key = stts_streaming_tts_turn_key(chat_id, session_id);
    if (key) {
        hermes_log(LOG_INFO, "streaming_tts_consumer",
                   "Streaming TTS turn %s fully completed — suppressing whole-file", key);
        free(key);
    }
}

/* PoP: streaming_tts_should_skip_whole_file @ gateway/platforms/base.py:streaming_tts_should_skip_whole_file */
bool stts_streaming_tts_should_skip_whole_file(stts_consumer_t *c, const char *chat_id,
                                               const char *session_id, json_t *msg) {
    (void)msg;
    if (!c) return false;
    /* If the turn already completed a full streaming pass, skip whole-file. */
    if (c->completed && c->suppress_whole_file) return true;
    /* If audible output was produced, skip to avoid replay from beginning. */
    if (c->started && c->handle && c->handle->audible && c->suppress_whole_file)
        return true;
    return false;
}

/* PoP: _notify_media_delivery_failure @ gateway/platforms/base.py:_notify_media_delivery_failure */
void stts_notify_media_delivery_failure(stts_consumer_t *c, const char *chat_id,
                                         const char *error) {
    if (!c) return;
    if (!c->partial && !c->suppress_whole_file) {
        hermes_log(LOG_WARNING, "streaming_tts_consumer",
                   "Media delivery failed for %s: %s (no audible output, fallback eligible)",
                   chat_id ? chat_id : "?", error ? error : "unknown");
    } else {
        hermes_log(LOG_WARNING, "streaming_tts_consumer",
                   "Media delivery failed for %s: %s (partial audible, suppressing replay)",
                   chat_id ? chat_id : "?", error ? error : "unknown");
    }
    if (!c->partial) c->suppress_whole_file = false;
}

/* ── Cleanup cache dir + media scanning helpers ────────────────────────── */

/* PoP: _cleanup_cache_dir @ gateway/platforms/base.py:_cleanup_cache_dir */
void stts_cleanup_cache_dir(const char *cache_dir, int max_age_seconds) {
    if (!cache_dir) return;
    /* Walk the directory and remove files older than max_age_seconds. */
    /* In a full port, this would use nftw() or opendir(). For now, this is
     * a pass-through: the caller is responsible for cache eviction. */
    hermes_log(LOG_DEBUG, "streaming_tts_consumer",
               "Cache dir cleanup requested: %s (max_age=%ds)", cache_dir, max_age_seconds);
}

/* PoP: _sniff_audio_ext @ gateway/platforms/base.py:_sniff_audio_ext */
const char *stts_sniff_audio_ext(const void *data, size_t len) {
    if (!data || len < 12) return NULL;
    const unsigned char *p = (const unsigned char *)data;
    /* MP3: ID3v2 or frame sync (0xFF 0xFB) */
    if (len >= 3 && p[0] == 'I' && p[1] == 'D' && p[2] == '3')
        return "mp3";
    if (len >= 2 && p[0] == 0xFF && (p[1] & 0xE6) == 0xE2)
        return "mp3";
    /* WAV: RIFF....WAVE */
    if (len >= 12 && p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F' &&
        p[8] == 'W' && p[9] == 'A' && p[10] == 'V' && p[11] == 'E')
        return "wav";
    /* OGG: "OggS" */
    if (len >= 4 && p[0] == 'O' && p[1] == 'g' && p[2] == 'g' && p[3] == 'S')
        return "ogg";
    /* FLAC: "fLaC" */
    if (len >= 4 && p[0] == 'f' && p[1] == 'L' && p[2] == 'a' && p[3] == 'c')
        return "flac";
    return NULL;
}

/* PoP: _match_extensionless_path @ gateway/platforms/base.py:_match_extensionless_path */
bool stts_match_extensionless_path(const char *path, const char *ext) {
    if (!path || !ext) return false;
    /* Strip the extension from path and compare. */
    char *dot = strrchr(path, '.');
    if (!dot) {
        /* No extension — path matches if path == ext */
        return strcmp(path, ext) == 0;
    }
    /* Compare path[:dot] with ext */
    size_t base_len = (size_t)(dot - path);
    return strncmp(path, ext, base_len) == 0 && strlen(ext) == base_len;
}

/* PoP: max_message_length_for_chat @ gateway/platforms/base.py:max_message_length_for_chat */
long stts_max_message_length_for_chat(stts_provider_t *provider, const char *chat_id) {
    (void)chat_id;
    if (!provider) return 0;
    /* Heuristic: 1024 bytes per second of speech at 24kHz mono int16. */
    return (long)provider->sample_rate * provider->sample_width * provider->channels * 1024;
}

/* PoP: message_len_fn_for_chat @ gateway/platforms/base.py:message_len_fn_for_chat */
long (*stts_message_len_fn_for_chat(stts_provider_t *provider))(const char *text) {
    (void)provider;
    /* Return a function that estimates the byte length of text-for-speech.
     * For PCM output, length is proportional to char count. */
    return stts_message_char_len;
}

static long stts_message_char_len(const char *text) {
    return text ? (long)strlen(text) : 0;
}

/* PoP: format_tool_preview @ gateway/platforms/base.py:format_tool_preview */
char *stts_format_tool_preview(const char *tool_name, const char *args_json,
                                bool truncate_to_100) {
    if (!tool_name) return NULL;
    char *out = NULL;
    if (truncate_to_100) {
        asprintf(&out, "%.100s", tool_name);
    } else {
        asprintf(&out, "%s", tool_name);
    }
    if (args_json && args_json[0]) {
        char *tmp = NULL;
        asprintf(&tmp, "%s(%s)", out, args_json);
        free(out);
        out = tmp;
    } else {
        char *tmp = NULL;
        asprintf(&tmp, "%s()", out);
        free(out);
        out = tmp;
    }
    return out;
}

/* PoP: _ea_escape @ gateway/platforms/base.py:_ea_escape */
char *stts_ea_escape(const char *text) {
    if (!text) return strdup("");
    size_t cap = strlen(text) * 6 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (const char *p = text; *p; p++) {
        switch (*p) {
            case '<': memcpy(out + o, "&lt;", 4); o += 4; break;
            case '>': memcpy(out + o, "&gt;", 4); o += 4; break;
            case '&': memcpy(out + o, "&amp;", 5); o += 5; break;
            case '"': memcpy(out + o, "&quot;", 6); o += 6; break;
            case '\'': memcpy(out + o, "&#39;", 5); o += 5; break;
            default: out[o++] = *p; break;
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: _format_exec_approval @ gateway/platforms/base.py:_format_exec_approval */
char *stts_format_exec_approval(const char *user_name, const char *command,
                                 const char *channel_name) {
    char *escaped_cmd = stts_ea_escape(command);
    char *out = NULL;
    if (channel_name) {
        asprintf(&out, "%s wants to run on %s: %s",
                 user_name ? user_name : "someone", channel_name, escaped_cmd);
    } else {
        asprintf(&out, "%s wants to run: %s",
                 user_name ? user_name : "someone", escaped_cmd);
    }
    free(escaped_cmd);
    return out;
}

/* PoP: _history_media_paths_for_session @ gateway/platforms/base.py:_history_media_paths_for_session */
/* Returns the number of media paths found. out_paths[] filled with
 * malloc'd strings (caller frees each). */
int stts_history_media_paths_for_session(const json_t *session_messages,
                                          char **out_paths, int max_paths) {
    if (!session_messages || session_messages->type != JSON_ARRAY || !out_paths)
        return 0;
    int count = 0;
    size_t n = json_len(session_messages);
    for (size_t i = 0; i < n && count < max_paths; i++) {
        json_t *msg = json_get(session_messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *content = json_obj_get(msg, "content");
        if (content && content->type == JSON_STRING && content->str_val) {
            /* Scan for media URLs. */
            const char *p = content->str_val;
            while (*p && count < max_paths) {
                if (strncmp(p, "http", 4) == 0) {
                    size_t len = 0;
                    const char *start = p;
                    while (*p && !isspace((unsigned char)*p) && *p != '"' &&
                           *p != ')' && *p != '>' && len < 511) {
                        p++; len++;
                    }
                    char *url = strndup(start, len);
                    /* Check if it looks like a media URL. */
                    if (strstr(url, "/media/") || strstr(url, "/audio/") ||
                        strstr(url, "/file/") || strstr(url, ".jpg") ||
                        strstr(url, ".png") || strstr(url, ".mp3") ||
                        strstr(url, ".ogg") || strstr(url, ".webp")) {
                        out_paths[count++] = url;
                    } else {
                        free(url);
                    }
                } else {
                    p++;
                }
            }
        }
    }
    return count;
}

/* ── Constructor / destructor ──────────────────────────────────────────── */

/* PoP: __init__ @ gateway/streaming_tts_consumer.py:StreamingTTSConsumer.__init__ */
stts_consumer_t *stts_consumer_new(void *adapter,
                                    stts_adapter_vtable_t *vtable,
                                    const char *chat_id,
                                    json_t *tts_config,
                                    json_t *metadata,
                                    stts_audio_format_t *audio_format_override) {
    stts_consumer_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    c->adapter = adapter;
    c->vtable = vtable;
    c->chat_id = chat_id ? strdup(chat_id) : NULL;
    c->tts_config = tts_config ? json_copy(tts_config) : NULL;
    c->metadata = metadata ? json_copy(metadata) : NULL;

    /* Resolve streaming provider */
    c->provider = resolve_streaming_provider_c(c->tts_config);

    stts_audio_format_t fb = audio_format_override ? *audio_format_override : stts_audio_format_default();
    c->audio_format = c->provider
        ? stts_audio_format_from_provider(c->provider, fb)
        : fb;

    stts_queue_init(&c->queue);
    c->handle = NULL;

    c->started = c->completed = c->partial = c->aborted = false;
    c->finished = c->dropped = c->suppress_whole_file = false;
    c->chunker_min_len = 20;  /* Python default */

    pthread_mutex_init(&c->lock, NULL);
    pthread_mutex_init(&c->drain_done_lock, NULL);
    pthread_cond_init(&c->drain_done_cond, NULL);

    /* Initialize stateful sentence chunker */
    c->chunker = stts_chunker_new(c->chunker_min_len);

    /* Lazy strip-markdown */
    c->strip_markdown_fn = resolve_tts_strip_markdown();

    return c;
}

void stts_consumer_free(stts_consumer_t *c) {
    if (!c) return;
    /* Ensure drain thread has stopped. */
    if (c->drain_thread_running) {
        c->aborted = true;
        stts_queue_put_nowait(&c->queue, STTS_ITEM_ABORT, NULL);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        pthread_mutex_lock(&c->drain_done_lock);
        while (!c->drain_done) {
            int rc = pthread_cond_timedwait(&c->drain_done_cond, &c->drain_done_lock, &ts);
            if (rc == ETIMEDOUT) break;
        }
        pthread_mutex_unlock(&c->drain_done_lock);
    }
    if (c->handle) {
        if (c->vtable && c->vtable->abort_streaming_tts)
            c->vtable->abort_streaming_tts(c->adapter, c->handle, "consumer freed");
        c->handle->aborted = true;
        free(c->handle);
    }
    stts_queue_destroy(&c->queue);
    pthread_mutex_destroy(&c->lock);
    pthread_mutex_destroy(&c->drain_done_lock);
    pthread_cond_destroy(&c->drain_done_cond);

    free(c->chat_id);
    if (c->tts_config) json_free(c->tts_config);
    if (c->metadata) json_free(c->metadata);
    if (c->chunker) stts_chunker_free(c->chunker);
    free(c);
}
