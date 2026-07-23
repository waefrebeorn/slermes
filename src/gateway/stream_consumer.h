#ifndef GATEWAY_STREAM_CONSUMER_H
#define GATEWAY_STREAM_CONSUMER_H

/*
 * gateway/stream_consumer.h — Faithful C11 port of the *synchronous,
 * text/state* surface of Python gateway/stream_consumer.py
 * (GatewayStreamConsumer).
 *
 * This module ports only the dependency-light, oracle-verifiable logic:
 *   - GatewayStreamConsumer._filter_and_accumulate  (think-block state machine)
 *   - GatewayStreamConsumer._flush_think_buffer
 *   - GatewayStreamConsumer._reset_segment_state
 *   - GatewayStreamConsumer._visible_prefix
 *   - GatewayStreamConsumer.has_delivered_text
 *
 * The async transport (run()/_edit_message/draft streaming/adapter calls) is
 * intentionally NOT ported here — it is coupled to the asyncio + adapter
 * runtime and would require inventing a runtime to fake. Those gaps remain
 * REAL_GAP until the gateway's async surface is ported as a unit.
 *
 * Design: opaque struct + minimal includes (C11 only, no god headers).
 * The streaming text state (accumulated buffer, think-block machine,
 * commentary list, last-sent text) lives behind the opaque handle so callers
 * cannot reach into internals.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque stream consumer text/state handle. */
typedef struct gw_stream_consumer gw_stream_consumer_t;

/* Runtime config mirror (only the fields the pure logic reads). */
typedef struct {
    const char *cursor;          /* trailing cursor char to strip (may be "") */
    bool        buffer_only;     /* unused by pure logic; kept for parity */
} gw_stream_consumer_cfg_t;

/* Create a new consumer text-state handle. Returns NULL on OOM.
 * `cfg` may be NULL for defaults (empty cursor). */
gw_stream_consumer_t *gw_stream_consumer_new(const gw_stream_consumer_cfg_t *cfg);

/* Free all held state. */
void gw_stream_consumer_free(gw_stream_consumer_t *c);

/* Reset segment state (mirrors _reset_segment_state). When preserve_no_edit
 * is set and the current message id is the "__no_edit__" sentinel, the reset
 * is skipped. message_id is the current message id (may be NULL). */
void gw_stream_consumer_reset_segment(gw_stream_consumer_t *c,
                                      bool preserve_no_edit,
                                      const char *message_id);

/* Feed one text delta through the think-block filter + accumulator.
 * Updates internal state and returns a pointer to the visible accumulated
 * text (valid until the next call). NULL if nothing visible yet. */
const char *gw_stream_consumer_filter_accumulate(gw_stream_consumer_t *c,
                                                 const char *text);

/* Flush held-back partial-tag buffer at end of stream. Returns pointer to
 * any newly-flushed visible text, or NULL. */
const char *gw_stream_consumer_flush_think_buffer(gw_stream_consumer_t *c);

/* Visible text already shown in the streamed message (mirrors _visible_prefix):
 * last-sent text with the trailing cursor stripped, then media directives
 * cleaned. Caller must free the result. */
char *gw_stream_consumer_visible_prefix(gw_stream_consumer_t *c);

/* Record the last-sent text (drives _visible_prefix / has_delivered_text). */
void gw_stream_consumer_set_last_sent(gw_stream_consumer_t *c, const char *text);

/* Record that `text` was delivered as a commentary bubble (for has_delivered_text). */
void gw_stream_consumer_add_commentary(gw_stream_consumer_t *c, const char *text);

/* Return true if `text` was already delivered as visible chat content
 * (mirrors has_delivered_text). */
bool gw_stream_consumer_has_delivered_text(gw_stream_consumer_t *c, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_STREAM_CONSUMER_H */
