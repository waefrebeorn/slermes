#ifndef NOC_W20_STANDALONE_CED4F0_H
#define NOC_W20_STANDALONE_CED4F0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/signal/standalone.py (gate w20_standalone_ced4f0).
 *
 * Out-of-process Signal send: raw signal-cli JSON-RPC, attachments batched at
 * SIGNAL_MAX_ATTACHMENTS_PER_MSG and metered by the process-wide scheduler,
 * with rate-limit retries and a per-batch failure tally. The network is the
 * only thing that cannot live in C here; everything that decides what is sent,
 * how many attempts each batch gets, which errors are rate limits and what the
 * caller is told is ported.
 *
 * Conventions mirrored from the live code:
 *   * attachments are chunked at 32, and no attachments still means ONE batch;
 *   * the text rides on batch #0 only;
 *   * a batch gets SIGNAL_RATE_LIMIT_MAX_ATTEMPTS (2) attempts; a non-rate-limit
 *     RPC error aborts the whole send with an error dict, a rate limit that
 *     survives both attempts marks the batch lost and the loop moves on;
 *   * an exhausted batch count equal to the batch count fails the send.
 */

/* SIGNAL_MAX_ATTACHMENTS_PER_MSG */
#define W20_STANDALONE_CED4F0_MAX_ATTACHMENTS 32
/* SIGNAL_RATE_LIMIT_MAX_ATTEMPTS */
#define W20_STANDALONE_CED4F0_MAX_ATTEMPTS 2
/* SIGNAL_BATCH_PACING_NOTICE_THRESHOLD */
#define W20_STANDALONE_CED4F0_NOTICE_THRESHOLD 10.0

/* PoP: w20_standalone_ced4f0_is_rate_limit @ plugins/platforms/signal/signal_rate_limit.py:_is_signal_rate_limit_error
 * The typed RATELIMIT code (-5), a legacy "[429]", or libsignal-net's
 * RetryLaterException / "Retry after N seconds" leaking through an attachment
 * error. ``err`` may be an RPC error object or a bare string. */
int w20_standalone_ced4f0_is_rate_limit(const json_t *err);

/* PoP: w20_standalone_ced4f0_retry_after @ plugins/platforms/signal/signal_rate_limit.py:_extract_retry_after_seconds
 * Per-token Retry-After from error.data.response.results[*].retryAfterSeconds
 * (the max of the parsable ones), else "Retry after N seconds" in the message.
 * Writes the seconds to ``out`` and returns 1, or returns 0 for None. */
int w20_standalone_ced4f0_retry_after(const json_t *err, double *out);

/* PoP: w20_standalone_ced4f0_send_timeout @ plugins/platforms/signal/signal_rate_limit.py:_signal_send_timeout
 * 30s for a text-only RPC, else max(60, 5 * attachments). */
double w20_standalone_ced4f0_send_timeout(int num_attachments);

/* PoP: w20_standalone_ced4f0_chat_id @ plugins/platforms/signal/standalone.py:_send_signal
 * The result-safe chat identifier `_success` reports: "group:***" for a group
 * id, else the id itself. Malloc'd; caller frees. */
char *w20_standalone_ced4f0_chat_id(const char *chat_id);

/* PoP: w20_standalone_ced4f0_send @ plugins/platforms/signal/standalone.py:_send_signal
 * One whole send against a scripted RPC transport:
 *   extra          {"http_url"?, "account"?}
 *   chat_id        the Signal chat id ("group:<id>" or a recipient)
 *   media          [[path, is_voice], ...] -- real filesystem paths
 *   post_script    one entry per RPC attempt, in call order:
 *                  {"ok": true} | {"error": <rpc error>} | {"raise": "<text>"}
 *   estimates      scheduler.estimate_wait(n) per attachment batch, in order
 *   httpx_available  the live `import httpx` outcome
 * Answer:
 *   {"result": {...},            the payload the live function returns
 *    "batches": [[path...]...],  how the attachments were chunked
 *    "calls": [{"kind": "notice"|"send", "batch": i, "n": n}...],
 *    "feedback": [[retry_after|null, n]...],   scheduler.feedback() calls
 *    "acquired": [n...]}         scheduler.acquire() calls */
json_t *w20_standalone_ced4f0_send(const json_t *extra, const char *chat_id, const json_t *media,
                                   const json_t *post_script, const json_t *estimates,
                                   int httpx_available);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_STANDALONE_CED4F0_H */
