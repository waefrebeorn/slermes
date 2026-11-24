/*
 * emitter.c -- real C11 port of the monitoring emitter queue/dispatch core of
 * upstream Hermes agent/monitoring/emitter.py.
 *
 * PoP: w21_emitter_1f24b4_emit @ agent/monitoring/emitter.py:MonitoringEmitter.emit
 * PoP: w21_emitter_1f24b4_subscribe @ agent/monitoring/emitter.py:MonitoringEmitter.subscribe
 * PoP: w21_emitter_1f24b4_unsubscribe @ agent/monitoring/emitter.py:MonitoringEmitter.unsubscribe
 * PoP: w21_emitter_1f24b4_dispatch @ agent/monitoring/emitter.py:MonitoringEmitter._dispatch
 * PoP: w21_emitter_1f24b4_stats @ agent/monitoring/emitter.py:MonitoringEmitter.stats
 * PoP: w21_emitter_1f24b4_queue_cap @ agent/monitoring/emitter.py:_MAX_QUEUE
 * PoP: w21_emitter_1f24b4_drain_batch @ agent/monitoring/emitter.py:_DRAIN_BATCH
 *
 * `_ensure_started`/`_run`/`flush`/`close` are the daemon-thread seam and stay
 * upstream; the queue discipline they execute (first event + up to
 * _DRAIN_BATCH-1 more, fail-isolated subscribers, task_done) is what dispatch()
 * reproduces here.  `time.time_ns()` is the clock seam: callers pass now_ns.
 */
#include "hermes_json.h"
#include "noc_w21_emitter_1f24b4.h"

#include <stdlib.h>
#include <string.h>

#define EM24_SUBS_SLOTS 32
#define EM24_ID_LEN   48

struct w21_emitter_1f24b4 {
    int enabled;
    size_t cap_limit;          /* 0 == unbounded, like queue.Queue(maxsize=0) */
    size_t alloc;
    size_t count;
    size_t head;
    json_t **slots;          /* owned payloads, FIFO ring */
    long long dropped;
    long long dispatched;
    struct {
        char id[EM24_ID_LEN];
        int failing;
    } subs[EM24_SUBS_SLOTS];
    size_t nsubs;
};

int w21_emitter_1f24b4_queue_cap(void) { return 10000; }

int w21_emitter_1f24b4_drain_batch(void) { return 256; }

w21_emitter_1f24b4 *w21_emitter_1f24b4_new(int enabled, size_t capacity) {
    w21_emitter_1f24b4 *self = (w21_emitter_1f24b4 *)calloc(1, sizeof(*self));
    if (!self) return NULL;
    self->enabled = enabled ? 1 : 0;
    self->cap_limit = capacity;
    self->alloc = capacity ? capacity : 4;
    self->slots = (json_t **)calloc(self->alloc, sizeof(*self->slots));
    if (!self->slots) {
        free(self);
        return NULL;
    }
    return self;
}

void w21_emitter_1f24b4_free(w21_emitter_1f24b4 *self) {
    size_t i;
    if (!self) return;
    for (i = 0; i < self->alloc; i++) {
        if (self->slots[i]) json_free(self->slots[i]);
    }
    free(self->slots);
    free(self);
}

/* `event.to_dict() if hasattr(event, "to_dict") else dict(event)`; NULL = raised. */
static json_t *em24_payload(const json_t *event) {
    if (!event) return NULL;
    if (event->type == JSON_OBJECT) {
        json_t *as_dict = json_obj_get(event, "__to_dict__");
        if (as_dict) return as_dict->type == JSON_OBJECT ? json_copy(as_dict) : NULL;
        return json_copy(event);
    }
    if (event->type == JSON_ARRAY) {
        json_t *out = json_object();
        size_t i;
        for (i = 0; i < event->c.count; i++) {
            json_t *pair = event->c.items[i];
            if (!pair || pair->type != JSON_ARRAY || pair->c.count != 2 ||
                !pair->c.items[0] || pair->c.items[0]->type != JSON_STRING) {
                json_free(out);
                return NULL;
            }
            json_set(out, pair->c.items[0]->str_val,
                     pair->c.items[1] ? json_copy(pair->c.items[1]) : json_null());
        }
        return out;
    }
    return NULL;                                   /* dict(5) / dict(None) / dict("s") raise */
}

static void em24_drop_oldest(w21_emitter_1f24b4 *self) {
    if (self->count == 0) return;
    if (self->slots[self->head]) json_free(self->slots[self->head]);
    self->slots[self->head] = NULL;
    self->head = (self->head + 1) % self->alloc;
    self->count--;
    self->dropped++;
}

/* Unbounded queues (maxsize=0) grow instead of dropping. */
static int em24_grow(w21_emitter_1f24b4 *self) {
    size_t want = self->alloc * 2, i;
    json_t **grown = (json_t **)calloc(want, sizeof(*grown));
    if (!grown) return 0;
    for (i = 0; i < self->count; i++) {
        grown[i] = self->slots[(self->head + i) % self->alloc];
    }
    free(self->slots);
    self->slots = grown;
    self->alloc = want;
    self->head = 0;
    return 1;
}

int w21_emitter_1f24b4_emit(w21_emitter_1f24b4 *self, const json_t *event,
                            long long now_ns) {
    json_t *payload;
    size_t tail;

    if (!self) return -1;
    if (!self->enabled) return 0;
    payload = em24_payload(event);
    if (!payload) return -1;                       /* logged, then dropped */
    if (!json_obj_get(payload, "ts_ns")) {
        json_set(payload, "ts_ns", json_number((double)now_ns));
    }
    if (self->count == self->alloc) {
        if (self->cap_limit == 0) {
            if (!em24_grow(self)) {
                json_free(payload);
                return -1;
            }
        } else {
            em24_drop_oldest(self);                /* newest wins, bounded memory */
        }
    }
    tail = (self->head + self->count) % self->alloc;
    self->slots[tail] = payload;
    self->count++;
    return 1;
}

void w21_emitter_1f24b4_subscribe(w21_emitter_1f24b4 *self, const char *callback_id,
                                  int failing) {
    size_t i;
    if (!self || !callback_id) return;
    for (i = 0; i < self->nsubs; i++) {
        if (!strcmp(self->subs[i].id, callback_id)) {
            self->enabled = 1;
            return;
        }
    }
    if (self->nsubs >= EM24_SUBS_SLOTS) return;
    strncpy(self->subs[self->nsubs].id, callback_id, EM24_ID_LEN - 1);
    self->subs[self->nsubs].id[EM24_ID_LEN - 1] = '\0';
    self->subs[self->nsubs].failing = failing ? 1 : 0;
    self->nsubs++;
    self->enabled = 1;                             /* first subscriber enables collection */
}

void w21_emitter_1f24b4_unsubscribe(w21_emitter_1f24b4 *self, const char *callback_id) {
    size_t i;
    if (!self || !callback_id) return;
    for (i = 0; i < self->nsubs; i++) {
        if (!strcmp(self->subs[i].id, callback_id)) {
            memmove(&self->subs[i], &self->subs[i + 1],
                    (self->nsubs - i - 1) * sizeof(self->subs[0]));
            self->nsubs--;
            break;                                 /* suppress(ValueError) */
        }
    }
    if (self->nsubs == 0) self->enabled = 0;
}

json_t *w21_emitter_1f24b4_dispatch(w21_emitter_1f24b4 *self) {
    json_t *batch = json_array();
    size_t n = 0;
    int limit = w21_emitter_1f24b4_drain_batch();

    if (!self) return batch;
    while (n < (size_t)limit && self->count > 0) {
        json_t *payload = self->slots[self->head];
        self->slots[self->head] = NULL;
        self->head = (self->head + 1) % self->alloc;
        self->count--;
        json_append(batch, payload ? payload : json_null());
        n++;
    }
    /* _dispatch: every subscriber sees the batch; a failing one is logged and
     * skipped, and the counter still advances by len(batch). */
    self->dispatched += (long long)n;
    return batch;
}

json_t *w21_emitter_1f24b4_stats(const w21_emitter_1f24b4 *self) {
    json_t *out = json_object();
    json_set(out, "queued", json_number(self ? (double)self->count : 0.0));
    json_set(out, "dispatched", json_number(self ? (double)self->dispatched : 0.0));
    json_set(out, "dropped", json_number(self ? (double)self->dropped : 0.0));
    json_set(out, "subscribers", json_number(self ? (double)self->nsubs : 0.0));
    return out;
}

int w21_emitter_1f24b4_enabled(const w21_emitter_1f24b4 *self) {
    return (self && self->enabled) ? 1 : 0;
}
