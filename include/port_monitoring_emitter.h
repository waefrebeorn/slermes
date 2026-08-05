/* port_monitoring_emitter.h — C11 port of agent/monitoring/emitter.py
 *
 * Monitoring emitter: fire-and-forget bounded queue + background dispatcher.
 * The single seam between producers (status hooks, diagnostic log handler) and
 * consumers (OTLP streamers). Hot-path invariant: emit() returns in
 * microseconds, never blocks, never raises.
 *
 * Faithful-port notes:
 *  - The bounded queue is a thread-safe ring buffer (mutex + condvar). On a
 *    full queue, emit() drops the oldest event and counts the drop — the same
 *    newest-wins bounded-memory contract as Python's queue.Queue(maxsize=N).
 *  - Events are JSON objects (the C equivalent of a dataclass.to_dict() or a
 *    plain dict). emit() injects "ts_ns" when absent.
 *  - The dispatcher thread is a thin loop around the same batch-extract +
 *    fan-out code that drain_once() exposes, so the oracle can verify dispatch
 *    deterministically without racing the thread.
 *  - Subscriber failure is model-led by a guarded call (setjmp/longjmp mirrors
 *    Python's try/except around each subscriber) — a raising subscriber never
 *    affects its peers or the hot path.
 */

#ifndef PORT_MONITORING_EMITTER_H
#define PORT_MONITORING_EMITTER_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MONITORING_MAX_QUEUE 10000
#define MONITORING_DRAIN_BATCH 256

/* A live batch subscriber: called (from the dispatcher) with a JSON array of
 * event payloads. Returning normally is success; a "raising" subscriber can
 * longjmp out — dispatch catches it (fail-isolation) and continues. */
typedef void (*monitoring_subscriber_fn)(void *ctx, const json_t *batch);

typedef struct monitoring_emitter monitoring_emitter_t;

/* enabled: whether emit() enqueues. max_queue: ring depth (0 => default 10000). */
monitoring_emitter_t *monitoring_emitter_new(bool enabled, int max_queue);
void monitoring_emitter_free(monitoring_emitter_t *em);

/* Hot path. event is a JSON object (dict payload). Never blocks, never raises
 * into the caller. Returns the resulting queued count, or -1 if disabled. */
int monitoring_emitter_emit(monitoring_emitter_t *em, const json_t *event);

/* Subscribe/unsubscribe a live batch subscriber. Subscribing enables the
 * emitter; unsubscribing the last subscriber disables it. */
void monitoring_emitter_subscribe(monitoring_emitter_t *em,
                                  monitoring_subscriber_fn fn, void *ctx);
void monitoring_emitter_unsubscribe(monitoring_emitter_t *em,
                                    monitoring_subscriber_fn fn, void *ctx);

/* Wait boundedly for queued + in-flight batches to finish. timeout<=0 => no-op. */
void monitoring_emitter_flush(monitoring_emitter_t *em, double timeout);

/* {queued, dispatched, dropped, subscribers}. Caller frees the json object. */
json_t *monitoring_emitter_stats(monitoring_emitter_t *em);

/* Stop the dispatcher thread and join it. */
void monitoring_emitter_close(monitoring_emitter_t *em);

/* Test/CLI helper: extract one batch (up to MONITORING_DRAIN_BATCH) from the
 * queue and fan it out to subscribers — the exact body of the dispatcher
 * loop, without the background thread. Returns the number dispatched. */
int monitoring_emitter_drain_once(monitoring_emitter_t *em);

/* process-wide singleton */
monitoring_emitter_t *monitoring_emitter_get(void);
void monitoring_emitter_reset_for_tests(monitoring_emitter_t *emitter);
/* Module-level convenience: emit via the singleton. */
void monitoring_emitter_emit_singleton(const json_t *event);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MONITORING_EMITTER_H */
