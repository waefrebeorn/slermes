/*
 * noc_w21_emitter_1f24b4.h -- public C API for the port of upstream
 *   agent/monitoring/emitter.py
 *
 * MonitoringEmitter is the single seam between producers (gateway status hooks,
 * the diagnostic log handler) and consumers (OTLP streamers).  Its invariants:
 * emit() never blocks on disk/network and never raises into the caller; a full
 * queue drops the OLDEST event; a daemon thread fans batches of up to
 * _DRAIN_BATCH out to fail-isolated subscribers; nothing is persisted.
 *
 * The C port keeps the queue, the drop-oldest policy, the batch composition,
 * subscriber bookkeeping and the counters.  Two seams replace the process
 * environment: the ring-buffer capacity (10_000 in production; the fixture
 * drives it smaller to reach the drop path) and the clock (emit() reads
 * `now_ns` instead of time.time_ns()).  The dispatcher thread and its
 * `flush()` JoinEvent are the threading seam and stay upstream: the caller
 * invokes dispatch() from whatever thread owns the queue.
 */
#ifndef NOC_W21_EMITTER_1F24B4_H
#define NOC_W21_EMITTER_1F24B4_H

#include "hermes_json.h"

#include <stddef.h>

typedef struct w21_emitter_1f24b4 w21_emitter_1f24b4;

/* _MAX_QUEUE (10_000) -- ring-buffer depth; oldest dropped when full. */
int w21_emitter_1f24b4_queue_cap(void);

/* _DRAIN_BATCH (256) -- most events one dispatch() call hands a subscriber. */
int w21_emitter_1f24b4_drain_batch(void);

/* MonitoringEmitter(enabled=..., capacity=...); NULL on OOM. */
w21_emitter_1f24b4 *w21_emitter_1f24b4_new(int enabled, size_t capacity);
void w21_emitter_1f24b4_free(w21_emitter_1f24b4 *self);

/*
 * emit(event, now_ns) -> 1 enqueued, 0 ignored (collection disabled),
 *                       -1 swallowed by the hot-path invariant (event has no
 *                       to_dict() and dict(event) failed).
 *
 * `event` may be an object (payload), an object carrying "__to_dict__" (a
 * dataclass with to_dict()), or an array of [key, value] pairs (dict(list)).
 * "ts_ns" is set only when the payload does not carry one.
 */
int w21_emitter_1f24b4_emit(w21_emitter_1f24b4 *self, const json_t *event,
                            long long now_ns);

/* subscribe(): the first subscriber enables collection. Re-adding is a no-op. */
void w21_emitter_1f24b4_subscribe(w21_emitter_1f24b4 *self, const char *callback_id,
                                  int failing);

/* unsubscribe(): unknown ids are ignored (suppress(ValueError)); the last one out
 * disables collection. */
void w21_emitter_1f24b4_unsubscribe(w21_emitter_1f24b4 *self, const char *callback_id);

/*
 * One `_run()` iteration: first event + up to _DRAIN_BATCH-1 more, handed to
 * every subscriber (a failing subscriber is logged and skipped), then
 * `dispatched += len(batch)`.  Returns the batch payloads (possibly empty).
 */
json_t *w21_emitter_1f24b4_dispatch(w21_emitter_1f24b4 *self);

/* stats(): {"queued","dispatched","dropped","subscribers"} */
json_t *w21_emitter_1f24b4_stats(const w21_emitter_1f24b4 *self);

/* MonitoringEmitter._enabled (introspection only; subscribe/unsubscribe own it). */
int w21_emitter_1f24b4_enabled(const w21_emitter_1f24b4 *self);

#endif /* NOC_W21_EMITTER_1F24B4_H */
