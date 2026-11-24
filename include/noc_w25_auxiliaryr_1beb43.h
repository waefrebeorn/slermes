/*
 * noc_w25_auxiliaryr_1beb43.h - C surface for upstream
 * agent/auxiliary_rate_limit_backoff.py (gate w25_auxiliaryr_1beb43).
 *
 * The same-credential backoff ladder an auxiliary call climbs when a provider
 * refuses with a rate limit: how long the ladder waits for one attempt and the
 * budget constants that bound it.  The ladder driver, the client re-send, the
 * sleep and the logging are seams; what is ported here is a pure function of
 * the exception's declared cooldown, the attempt number and one uniform draw.
 */
#ifndef NOC_W25_AUXILIARYR_1BEB43_H
#define NOC_W25_AUXILIARYR_1BEB43_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The exception, reduced to the three value seams upstream reads:
 *   {"headers":  <Retry-After value / headers mapping / null>,   # error.response.headers
 *    "body":     <dict with "retry_after", top level or under "error", or null>,
 *    "metadata": <botocore ResponseMetadata.HTTPHeaders mapping or null>}
 * ``headers`` and ``metadata`` describe the two mutually exclusive response
 * shapes: an SDK response object (headers) or botocore's parsed response dict
 * (ResponseMetadata.HTTPHeaders).  When ``headers`` is present the botocore
 * branch is unreachable, as upstream's ``isinstance(response, dict)`` guard.
 * Results: the provider-declared cooldown in seconds (> 0 only), or null.
 * PoP: w25_auxiliaryr_1beb43_retry_after @ agent/auxiliary_rate_limit_backoff.py:rate_limit_retry_after */
json_t *w25_auxiliaryr_1beb43_retry_after(const json_t *exc);

/* Seconds to wait before same-credential retry ``attempt`` (1-based), or null
 * when the provider declared a cooldown longer than the retry-after cap.
 * ``jitter`` is the one uniform [0, 1) draw the jittered backoff consumes
 * (upstream seeds random.Random from time + counter: a seam, fixed here).
 * The delay is min(base * 2^(attempt-1), cap) + jitter * 0.5 * delay.
 * PoP: w25_auxiliaryr_1beb43_wait @ agent/auxiliary_rate_limit_backoff.py:rate_limit_wait */
json_t *w25_auxiliaryr_1beb43_wait(const json_t *exc, double attempt, double jitter);

/* RATE_LIMIT_* ladder constants, as an object.
 * PoP: w25_auxiliaryr_1beb43_constants @ agent/auxiliary_rate_limit_backoff.py:<module constants> */
json_t *w25_auxiliaryr_1beb43_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W25_AUXILIARYR_1BEB43_H */
