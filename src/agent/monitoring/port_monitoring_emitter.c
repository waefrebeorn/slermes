/* port_monitoring_emitter.c — C11 port of agent/monitoring/emitter.py
 *
 * See port_monitoring_emitter.h for the faithful-port contract.
 */

#include "port_monitoring_emitter.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <setjmp.h>
#include <errno.h>

/* ---- thread-local subscriber-failure flag (mirrors Python try/except) ---- */
static __thread int g_sub_failed = 0;
void monitoring_mark_subscriber_failed(void) { g_sub_failed = 1; }

/* deep copy (libjson has no built-in) */
json_t *json_deep_copy(const json_t *v) {
    if (!v) return NULL;
    switch (v->type) {
    case JSON_NULL:   return json_null();
    case JSON_BOOL:   return json_bool(v->bool_val);
    case JSON_NUMBER: return json_number(v->num_val);
    case JSON_STRING: return json_string(v->str_val);
    case JSON_ARRAY: {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, json_deep_copy(v->c.items[i]));
        return a;
    }
    case JSON_OBJECT: {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_set(o, v->c.keys[i], json_deep_copy(v->c.items[i]));
        }
        return o;
    }
    }
    return json_null();
}

/* ---- emitter ---- */
struct monitoring_emitter {
    bool enabled;
    int max_queue;
    /* ring buffer of json_t* events */
    json_t **ring;
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    /* dispatcher thread */
    pthread_t thread;
    volatile bool started;
    volatile bool stop;
    /* subscribers */
    struct sub {
        monitoring_subscriber_fn fn;
        void *ctx;
    } *subs;
    size_t n_subs, cap_subs;
    /* stats */
    long dispatched;
    long dropped;
    /* drain cond for flush() */
    pthread_cond_t drained;
};

/* ---- ring helpers (lock held) ---- */
static void ring_push(monitoring_emitter_t *em, json_t *ev) {
    if (em->count == em->max_queue) {
        /* drop oldest */
        json_t *old = em->ring[em->head];
        if (old) json_free(old);
        em->head = (em->head + 1) % em->max_queue;
        em->count--;
        em->dropped++;
    }
    em->ring[em->tail] = ev;
    em->tail = (em->tail + 1) % em->max_queue;
    em->count++;
}
static json_t *ring_pop(monitoring_emitter_t *em) {
    if (em->count == 0) return NULL;
    json_t *ev = em->ring[em->head];
    em->ring[em->head] = NULL;
    em->head = (em->head + 1) % em->max_queue;
    em->count--;
    return ev;
}

/* ---- dispatch (shared by thread + drain_once) ---- */
/* PoP: do_dispatch @ agent/monitoring/emitter.py:MonitoringEmitter._dispatch */
static void do_dispatch(monitoring_emitter_t *em, json_t *batch) {
    size_t n = json_len(batch);
    for (size_t i = 0; i < em->n_subs; i++) {
        g_sub_failed = 0;
        /* model-led fail isolation: a raising subscriber sets g_sub_failed;
         * we swallow it exactly like Python's try/except around sub(batch). */
        em->subs[i].fn(em->subs[i].ctx, batch);
        (void)g_sub_failed;
    }
    em->dispatched += (long)n;
}

/* ---- dispatcher thread ---- */
/* PoP: dispatch_run @ agent/monitoring/emitter.py:MonitoringEmitter._run */
static void *dispatch_run(void *arg) {
    monitoring_emitter_t *em = arg;
    while (!em->stop) {
        pthread_mutex_lock(&em->lock);
        json_t *first = ring_pop(em);
        if (!first) {
            /* wait up to 0.5s for an event, then re-check stop */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 500000000;
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&em->not_empty, &em->lock, &ts);
            pthread_mutex_unlock(&em->lock);
            continue;
        }
        /* build batch */
        json_t *batch = json_array();
        json_append(batch, first);
        while (json_len(batch) < MONITORING_DRAIN_BATCH) {
            json_t *ev = ring_pop(em);
            if (!ev) break;
            json_append(batch, ev);
        }
        pthread_mutex_unlock(&em->lock);
        do_dispatch(em, batch);
        json_free(batch);
        pthread_mutex_lock(&em->lock);
        pthread_cond_broadcast(&em->drained);
        pthread_mutex_unlock(&em->lock);
    }
    return NULL;
}

/* ---- public API ---- */
/* PoP: monitoring_emitter_new @ agent/monitoring/emitter.py:MonitoringEmitter.__init__ */
monitoring_emitter_t *monitoring_emitter_new(bool enabled, int max_queue) {
    monitoring_emitter_t *em = calloc(1, sizeof *em);
    em->enabled = enabled;
    em->max_queue = max_queue > 0 ? max_queue : MONITORING_MAX_QUEUE;
    em->ring = calloc(em->max_queue, sizeof(json_t *));
    pthread_mutex_init(&em->lock, NULL);
    pthread_cond_init(&em->not_empty, NULL);
    pthread_cond_init(&em->drained, NULL);
    return em;
}

void monitoring_emitter_free(monitoring_emitter_t *em) {
    if (!em) return;
    monitoring_emitter_close(em);
    pthread_mutex_lock(&em->lock);
    while (em->count > 0) { json_t *e = ring_pop(em); if (e) json_free(e); }
    pthread_mutex_unlock(&em->lock);
    for (size_t i = 0; i < em->n_subs; i++) { /* no-op */ }
    free(em->ring);
    free(em->subs);
    pthread_mutex_destroy(&em->lock);
    pthread_cond_destroy(&em->not_empty);
    pthread_cond_destroy(&em->drained);
    free(em);
}

/* PoP: monitoring_emitter_emit @ agent/monitoring/emitter.py:MonitoringEmitter.emit */
/* PoP: monitoring_emitter_ensure_started @ agent/monitoring/emitter.py:MonitoringEmitter._ensure_started */
static void monitoring_emitter_ensure_started(monitoring_emitter_t *em) {
    if (em->started) return;
    em->started = true;
    pthread_create(&em->thread, NULL, dispatch_run, em);
}

int monitoring_emitter_emit(monitoring_emitter_t *em, const json_t *event) {
    if (!em || !em->enabled) return -1;
    json_t *payload;
    if (event && event->type == JSON_OBJECT) {
        payload = json_deep_copy(event);
    } else {
        /* not a dict: wrap minimally so dispatch still receives an object */
        payload = json_object();
    }
    /* inject ts_ns if absent */
    if (json_get(payload, "ts_ns") == NULL) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long long ns = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
        json_set(payload, "ts_ns", json_number((double)ns));
    }
    monitoring_emitter_ensure_started(em);
    pthread_mutex_lock(&em->lock);
    ring_push(em, payload);
    pthread_cond_signal(&em->not_empty);
    int q = em->count;
    pthread_mutex_unlock(&em->lock);
    return q;
}

/* PoP: monitoring_emitter_subscribe @ agent/monitoring/emitter.py:MonitoringEmitter.subscribe */
void monitoring_emitter_subscribe(monitoring_emitter_t *em,
                                 monitoring_subscriber_fn fn, void *ctx) {
    if (!em) return;
    pthread_mutex_lock(&em->lock);
    bool found = false;
    for (size_t i = 0; i < em->n_subs; i++)
        if (em->subs[i].fn == fn && em->subs[i].ctx == ctx) { found = true; break; }
    if (!found) {
        if (em->n_subs == em->cap_subs) {
            em->cap_subs = em->cap_subs ? em->cap_subs * 2 : 4;
            em->subs = realloc(em->subs, em->cap_subs * sizeof(*em->subs));
        }
        em->subs[em->n_subs].fn = fn;
        em->subs[em->n_subs].ctx = ctx;
        em->n_subs++;
    }
    em->enabled = true;
    pthread_mutex_unlock(&em->lock);
}

/* PoP: monitoring_emitter_unsubscribe @ agent/monitoring/emitter.py:MonitoringEmitter.unsubscribe */
void monitoring_emitter_unsubscribe(monitoring_emitter_t *em,
                                    monitoring_subscriber_fn fn, void *ctx) {
    if (!em) return;
    pthread_mutex_lock(&em->lock);
    for (size_t i = 0; i < em->n_subs; i++) {
        if (em->subs[i].fn == fn && em->subs[i].ctx == ctx) {
            em->subs[i] = em->subs[em->n_subs - 1];
            em->n_subs--;
            break;
        }
    }
    if (em->n_subs == 0) em->enabled = false;
    pthread_mutex_unlock(&em->lock);
}

/* PoP: monitoring_emitter_flush @ agent/monitoring/emitter.py:MonitoringEmitter.flush */
void monitoring_emitter_flush(monitoring_emitter_t *em, double timeout) {
    if (!em || timeout <= 0.0) return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long long ns = (long long)(timeout * 1e9);
    ts.tv_sec += (time_t)(ns / 1000000000LL);
    ts.tv_nsec += (long)(ns % 1000000000LL);
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    pthread_mutex_lock(&em->lock);
    while (em->count > 0 && !em->stop) {
        if (pthread_cond_timedwait(&em->drained, &em->lock, &ts) == ETIMEDOUT) break;
    }
    pthread_mutex_unlock(&em->lock);
}

/* PoP: monitoring_emitter_stats @ agent/monitoring/emitter.py:MonitoringEmitter.stats */
json_t *monitoring_emitter_stats(monitoring_emitter_t *em) {
    json_t *d = json_object();
    pthread_mutex_lock(&em->lock);
    json_set(d, "queued", json_number((double)em->count));
    json_set(d, "dispatched", json_number((double)em->dispatched));
    json_set(d, "dropped", json_number((double)em->dropped));
    json_set(d, "subscribers", json_number((double)em->n_subs));
    pthread_mutex_unlock(&em->lock);
    return d;
}

/* PoP: monitoring_emitter_close @ agent/monitoring/emitter.py:MonitoringEmitter.close */
void monitoring_emitter_close(monitoring_emitter_t *em) {
    if (!em || !em->started || em->stop) return;
    em->stop = true;
    pthread_mutex_lock(&em->lock);
    pthread_cond_broadcast(&em->not_empty);
    pthread_mutex_unlock(&em->lock);
    pthread_join(em->thread, NULL);
    em->started = false;
}

int monitoring_emitter_drain_once(monitoring_emitter_t *em) {
    if (!em) return 0;
    pthread_mutex_lock(&em->lock);
    json_t *first = ring_pop(em);
    if (!first) { pthread_mutex_unlock(&em->lock); return 0; }
    json_t *batch = json_array();
    json_append(batch, first);
    while (json_len(batch) < MONITORING_DRAIN_BATCH) {
        json_t *ev = ring_pop(em);
        if (!ev) break;
        json_append(batch, ev);
    }
    pthread_mutex_unlock(&em->lock);
    do_dispatch(em, batch);
    int n = (int)json_len(batch);
    json_free(batch);
    return n;
}

/* ---- process-wide singleton ---- */
static monitoring_emitter_t *g_emitter = NULL;
static pthread_mutex_t g_emitter_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: monitoring_emitter_get @ agent/monitoring/emitter.py:get_emitter */
/* PoP: monitoring_emitter_emit_singleton @ agent/monitoring/emitter.py:emit */
void monitoring_emitter_emit_singleton(const json_t *event) {
    monitoring_emitter_emit(monitoring_emitter_get(), event);
}

monitoring_emitter_t *monitoring_emitter_get(void) {
    pthread_mutex_lock(&g_emitter_lock);
    if (g_emitter == NULL) {
        g_emitter = monitoring_emitter_new(false, 0);  /* collection opt-in */
    }
    monitoring_emitter_t *r = g_emitter;
    pthread_mutex_unlock(&g_emitter_lock);
    return r;
}

/* PoP: monitoring_emitter_reset_for_tests @ agent/monitoring/emitter.py:reset_emitter_for_tests */
void monitoring_emitter_reset_for_tests(monitoring_emitter_t *emitter) {
    pthread_mutex_lock(&g_emitter_lock);
    if (g_emitter != NULL && emitter != g_emitter) {
        monitoring_emitter_close(g_emitter);
        monitoring_emitter_free(g_emitter);
    }
    g_emitter = emitter;
    pthread_mutex_unlock(&g_emitter_lock);
}
