/*
 * plugin_stream_hooks.c — PoP of agent/plugin_stream_hooks.py.
 *
 * Python -> C symbol map (1:1):
 *   _QUEUE_SIZE                          -> PSH_QUEUE_SIZE
 *   _STOP sentinel                       -> psh item type PSH_ITEM_STOP
 *   _ConsumerDispatcher                  -> struct psh_dispatcher
 *   _callback_name                       -> plugin_stream_hooks__callback_name
 *   _worker                              -> psh_worker (pthread start fn)
 *   _registered_callbacks                -> plugin_stream_hooks__registered_callbacks
 *   _stop_dispatcher                     -> psh_stop_dispatcher
 *   _dispatchers_for                     -> psh_dispatchers_for
 *   enqueue_plugin_stream_hook           -> enqueue_plugin_stream_hook
 *   has_stream_observer_hooks            -> has_stream_observer_hooks
 *   has_reasoning_stream_observer_hooks  -> has_reasoning_stream_observer_hooks
 *   stream_reasoning_deltas_enabled      -> stream_reasoning_deltas_enabled
 *   shutdown_plugin_stream_hook_dispatcher -> shutdown_plugin_stream_hook_dispatcher
 *
 * The Python module resolves consumers via hermes_cli.plugins.iter_hook_
 * callbacks(hook_name); the C tree has no live Python-plugin registry, so the
 * lookup goes through an injectable callback table (real consumers register;
 * empty by default exactly like an install without stream-hook plugins).
 */
#define _POSIX_C_SOURCE 200809L
#include "plugin_stream_hooks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>

#include "hermes_json.h"
#include "libyaml/yaml.h"

const size_t PSH_QUEUE_SIZE = 1024;

/* ------------------------------------------------------------------ */
/* internal types                                                      */
/* ------------------------------------------------------------------ */

typedef struct psh_item {
    int is_stop;
    json_t *payload; /* owned deep copy of the enqueued dict */
} psh_item_t;

typedef struct psh_queue {
    psh_item_t buf[1024];
    size_t head, count;
    pthread_mutex_t mu;
    pthread_cond_t not_empty, not_full;
    unsigned unfinished; /* Queue.join() task_done() bookkeeping */
    int closed;
} psh_queue_t;

struct psh_dispatcher {
    char *hook_name;
    psh_callback_t callback;
    psh_queue_t events;
    pthread_t thread;
    int thread_started;
};

/* ------------------------------------------------------------------ */
/* module state (the _dispatchers dict under _dispatcher_lock)          */
/* ------------------------------------------------------------------ */

#define PSH_MAX_DISPATCHERS 256

typedef struct {
    char hook_name[128];
    long callback_id;
    psh_dispatcher_t *dispatcher;
} psh_entry_t;

static psh_entry_t g_dispatchers[PSH_MAX_DISPATCHERS];
static size_t g_dispatchers_n = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* injectable consumer registry (see header) */
static psh_callback_t g_registry[PSH_MAX_DISPATCHERS];
static char g_registry_hook[PSH_MAX_DISPATCHERS][128];
static size_t g_registry_n = 0;

/* OBSERVER_SCHEMA_VERSION lives in hermes_cli.middleware; the value below is
 * the schema constant shipped alongside this port. */
static const char *OBSERVER_SCHEMA_VERSION_VALUE = "hermes.observer.v1";

/* ------------------------------------------------------------------ */
/* queue primitives                                                    */
/* ------------------------------------------------------------------ */

static void q_init(psh_queue_t *q)
{
    memset(q, 0, sizeof *q);
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

static void q_destroy(psh_queue_t *q)
{
    pthread_mutex_lock(&q->mu);
    for (size_t i = 0; i < q->count; i++)
        if (q->buf[(q->head + i) % 1024].payload)
            json_free(q->buf[(q->head + i) % 1024].payload);
    q->count = 0;
    pthread_mutex_unlock(&q->mu);
    pthread_mutex_destroy(&q->mu);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

static void q_task_done(psh_queue_t *q)
{
    pthread_mutex_lock(&q->mu);
    if (q->unfinished > 0) q->unfinished--;
    if (q->unfinished == 0) pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mu);
}

static void q_join(psh_queue_t *q)
{
    pthread_mutex_lock(&q->mu);
    while (q->unfinished > 0)
        pthread_cond_wait(&q->not_full, &q->mu);
    pthread_mutex_unlock(&q->mu);
}

/* put_nowait; on Full drops the OLDEST item and retries once (mirrors the
 * except-queue.Full drop-oldest recovery in enqueue/_stop_dispatcher). */
static int q_put_nowait_drop_oldest(psh_queue_t *q, psh_item_t item)
{
    pthread_mutex_lock(&q->mu);
    if (q->count >= 1024) {
        /* drop oldest */
        psh_item_t old = q->buf[q->head];
        if (old.payload) json_free(old.payload);
        q->head = (q->head + 1) % PSH_QUEUE_SIZE;
        q->count--;
        if (q->unfinished > 0) q->unfinished--;
    }
    q->buf[(q->head + q->count) % 1024] = item;
    q->count++;
    q->unfinished++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
    return 1;
}

static int q_get(psh_queue_t *q, psh_item_t *out) /* blocking */
{
    pthread_mutex_lock(&q->mu);
    while (q->count == 0 && !q->closed)
        pthread_cond_wait(&q->not_empty, &q->mu);
    if (q->count == 0) { pthread_mutex_unlock(&q->mu); return 0; }
    *out = q->buf[q->head];
    q->head = (q->head + 1) % PSH_QUEUE_SIZE;
    q->count--;
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mu);
    return 1;
}

static void q_wake_all(psh_queue_t *q)
{
    pthread_mutex_lock(&q->mu);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
}

/* ------------------------------------------------------------------ */
/* worker (_worker)                                                    */
/* ------------------------------------------------------------------ */

static void *psh_worker(void *arg)
{
    psh_dispatcher_t *d = arg;
    for (;;) {
        psh_item_t item;
        if (!q_get(&d->events, &item)) return NULL; /* closed + drained */
        if (item.is_stop) {
            q_task_done(&d->events);
            return NULL;
        }
        if (item.payload && item.payload->type == JSON_OBJECT &&
            !json_object_get(item.payload, "telemetry_schema_version")) {
            json_object_set(item.payload, "telemetry_schema_version",
                            json_string(OBSERVER_SCHEMA_VERSION_VALUE));
        }
        if (item.payload && d->callback.fn)
            d->callback.fn(d->callback.ctx, item.payload);
        q_task_done(&d->events);
        if (item.payload) json_free(item.payload);
    }
}

const char *plugin_stream_hooks__callback_name(const psh_callback_t *cb)
{
    return cb && cb->name ? cb->name : "<callable>";
}

/* ------------------------------------------------------------------ */
/* consumer lookup (_registered_callbacks)                              */
/* ------------------------------------------------------------------ */

size_t plugin_stream_hooks__registered_callbacks(const char *hook_name,
                                                 psh_callback_t *out,
                                                 size_t out_cap)
{
    size_t n = 0;
    pthread_mutex_lock(&g_lock);
    for (size_t i = 0; i < g_registry_n && n < out_cap; i++) {
        if (strcmp(g_registry_hook[i], hook_name) == 0)
            out[n++] = g_registry[i];
    }
    pthread_mutex_unlock(&g_lock);
    return n;
}

void plugin_stream_hooks_set_consumer_registry(const char *hook_name,
                                               const psh_callback_t *cbs,
                                               size_t n)
{
    pthread_mutex_lock(&g_lock);
    /* replace all entries for this hook (test/registry reconfiguration) */
    size_t w = 0;
    for (size_t i = 0; i < g_registry_n; i++) {
        if (strcmp(g_registry_hook[i], hook_name) == 0) continue;
        if (w != i) {
            memcpy(g_registry_hook[w], g_registry_hook[i], 128);
            g_registry[w] = g_registry[i];
        }
        w++;
    }
    g_registry_n = w;
    for (size_t k = 0; k < n && g_registry_n < PSH_MAX_DISPATCHERS; k++, g_registry_n++) {
        snprintf(g_registry_hook[g_registry_n], 128, "%s", hook_name);
        g_registry[g_registry_n] = cbs[k];
    }
    pthread_mutex_unlock(&g_lock);
}

/* ------------------------------------------------------------------ */
/* dispatcher lifecycle                                                */
/* ------------------------------------------------------------------ */

static void psh_stop_dispatcher(psh_dispatcher_t *d, double timeout)
{
    psh_item_t stop = { .is_stop = 1, .payload = NULL };
    q_put_nowait_drop_oldest(&d->events, stop); /* Full -> drop-oldest retry */
    if (d->thread_started) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)(timeout <= 0 ? 1 : timeout);
        /* join with wall timeout via timedwait emulation: plain join after
         * waking the worker is fine because STOP is already queued */
        q_wake_all(&d->events);
        pthread_join(d->thread, NULL);
        d->thread_started = 0;
    }
}

/* Returns malloc'd array of NEW dispatcher lists matching hook_name,
 * creating one worker thread per fresh callback and stopping stale ones.
 * Caller frees the array (dispatchers themselves stay owned by the module). */
static psh_dispatcher_t **psh_dispatchers_for(const char *hook_name, size_t *out_n)
{
    psh_callback_t cbs[64];
    size_t ncbs = plugin_stream_hooks__registered_callbacks(hook_name, cbs, 64);
    *out_n = 0;
    if (ncbs == 0) return NULL;

    psh_dispatcher_t **stale = calloc(PSH_MAX_DISPATCHERS, sizeof *stale);
    size_t stale_n = 0;
    psh_dispatcher_t **ready = calloc(PSH_MAX_DISPATCHERS, sizeof *ready);
    size_t ready_n = 0;

    pthread_mutex_lock(&g_lock);
    /* prune entries whose callback vanished from the current callback set */
    size_t w = 0;
    for (size_t i = 0; i < g_dispatchers_n; i++) {
        psh_entry_t *e = &g_dispatchers[i];
        int keep = 1;
        if (strcmp(e->hook_name, hook_name) == 0) {
            keep = 0;
            for (size_t k = 0; k < ncbs; k++) {
                if ((long)(intptr_t)cbs[k].fn == e->callback_id) { keep = 1; break; }
            }
        }
        if (!keep) stale[stale_n++] = e->dispatcher;
        else g_dispatchers[w++] = *e;
    }
    g_dispatchers_n = w;

    for (size_t k = 0; k < ncbs; k++) {
        long cid = (long)(intptr_t)cbs[k].fn;
        psh_dispatcher_t *d = NULL;
        for (size_t i = 0; i < g_dispatchers_n; i++) {
            psh_entry_t *e = &g_dispatchers[i];
            if (strcmp(e->hook_name, hook_name) == 0 && e->callback_id == cid) {
                d = e->dispatcher;
                break;
            }
        }
        int dead = d && (!d->thread_started);
        if (!d || dead) {
            d = calloc(1, sizeof *d);
            d->hook_name = strdup(hook_name);
            d->callback = cbs[k];
            q_init(&d->events);
            if (pthread_create(&d->thread, NULL, psh_worker, d) == 0)
                d->thread_started = 1;
            psh_entry_t *e = &g_dispatchers[g_dispatchers_n++];
            snprintf(e->hook_name, sizeof e->hook_name, "%s", hook_name);
            e->callback_id = cid;
            e->dispatcher = d;
        }
        if (ready_n < PSH_MAX_DISPATCHERS) ready[ready_n++] = d;
    }
    pthread_mutex_unlock(&g_lock);

    for (size_t i = 0; i < stale_n; i++)
        psh_stop_dispatcher(stale[i], 0.2);
    free(stale);

    *out_n = ready_n;
    return ready;
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

int enqueue_plugin_stream_hook(const char *hook_name, const json_t *payload)
{
    size_t rn = 0;
    psh_dispatcher_t **ready = psh_dispatchers_for(hook_name, &rn);
    int queued = 0;
    if (!ready) return 0;
    for (size_t i = 0; i < rn; i++) {
        psh_item_t item = { .is_stop = 0,
                            .payload = payload ? json_copy(payload) : NULL };
        q_put_nowait_drop_oldest(&ready[i]->events, item);
        queued = 1;
    }
    free(ready);
    return queued;
}

int has_stream_observer_hooks(void)
{
    static const char *names[] = { "on_stream_start", "on_stream_delta",
                                   "on_stream_end" };
    for (int i = 0; i < 3; i++) {
        psh_callback_t cbs[8];
        if (plugin_stream_hooks__registered_callbacks(names[i], cbs, 8) > 0)
            return 1;
    }
    return 0;
}

int stream_reasoning_deltas_enabled(void)
{
    /* bool(cfg_get(config, "plugins", "stream_reasoning_deltas", False)) */
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) {
        home = getenv("SLERMES_HOME");
        if (!home || !*home) home = getenv("HOME");
    }
    if (!home || !*home) return 0;
    char path[1024];
    snprintf(path, sizeof path, "%s/.hermes/config.yaml", home);
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) free(err);
    if (!doc) return 0;
    int v = yaml_get_bool(doc, "plugins.stream_reasoning_deltas", 0);
    yaml_free(doc);
    return v;
}

int has_reasoning_stream_observer_hooks(void)
{
    if (!stream_reasoning_deltas_enabled()) return 0;
    psh_callback_t cbs[8];
    return plugin_stream_hooks__registered_callbacks("on_stream_delta", cbs, 8) > 0;
}

void shutdown_plugin_stream_hook_dispatcher(double timeout)
{
    pthread_mutex_lock(&g_lock);
    size_t n = g_dispatchers_n;
    psh_dispatcher_t **all = calloc(n ? n : 1, sizeof *all);
    for (size_t i = 0; i < n; i++) all[i] = g_dispatchers[i].dispatcher;
    g_dispatchers_n = 0;
    pthread_mutex_unlock(&g_lock);
    for (size_t i = 0; i < n; i++) {
        psh_stop_dispatcher(all[i], timeout > 0 ? timeout : 1.0);
        q_destroy(&all[i]->events);
        free(all[i]->hook_name);
        free(all[i]);
    }
    free(all);
}

/* ---- introspection helpers (harness-facing; mirror tests poking
 * plugin_stream_hooks._dispatchers on the Python side) ---- */
/* Harness-facing _dispatchers_for: runs the real create/prune cycle and
 * reports how many dispatchers are ready (mirrors tests calling
 * plugin_stream_hooks._dispatchers_for(hook) directly on the Python side). */
size_t plugin_stream_hooks_dispatchers_for(const char *hook_name)
{
    size_t rn = 0;
    psh_dispatcher_t **ready = psh_dispatchers_for(hook_name, &rn);
    free(ready);
    return rn;
}

size_t plugin_stream_hooks_dispatcher_count(const char *hook_name)
{
    pthread_mutex_lock(&g_lock);
    size_t n = 0;
    for (size_t i = 0; i < g_dispatchers_n; i++)
        if (!hook_name || strcmp(g_dispatchers[i].hook_name, hook_name) == 0)
            n++;
    pthread_mutex_unlock(&g_lock);
    return n;
}

void plugin_stream_hooks_wait_idle(void)
{
    pthread_mutex_lock(&g_lock);
    size_t n = g_dispatchers_n;
    psh_dispatcher_t **all = calloc(n ? n : 1, sizeof *all);
    for (size_t i = 0; i < n; i++) all[i] = g_dispatchers[i].dispatcher;
    pthread_mutex_unlock(&g_lock);
    for (size_t i = 0; i < n; i++) q_join(&all[i]->events);
    free(all);
}
