/*
 * noc_w24_standalone_e0a2a7.h - C surface for upstream
 * plugins/platforms/discord/standalone_rest.py (gate w24_standalone_e0a2a7).
 *
 * The out-of-process Discord sender's own REST pacing: the URL builder, the
 * retry-after clamp, and the bounded bucket/429 decision the async
 * ``post_paced`` loop walks.  The HTTP session is a seam: ``pace`` takes the
 * scripted responses a session would have returned and replays the decision on
 * a virtual mono clock, so the waits and refill instants are reproducible.
 */
#ifndef W24_STANDALONE_E0A2A7_H
#define W24_STANDALONE_E0A2A7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _MAX_RATE_LIMIT_RETRIES: attempts 0..N inclusive, so N+1 posts at most. */
#define W24_STANDALONE_E0A2A7_MAX_RETRIES 5

/* Route.BASE fallback (discord.py absent -> the constant). */
#define W24_STANDALONE_E0A2A7_API_BASE "https://discord.com/api/v10"

/* {"path": <str>} -> f"{BASE.rstrip('/')}/{path.lstrip('/')}".
 * A non-string path is str()'d like the f-string does; null when absent. */
json_t *w24_standalone_e0a2a7_api_url(const json_t *args);

/* {"value": <any>} -> max(0.0, min(float(value), 60.0)), 1.0 when float()
 * raises (TypeError/ValueError) -- i.e. for null, [], {}, and "abc". */
json_t *w24_standalone_e0a2a7_seconds(const json_t *args);

/* {"responses": [{"status": int, "headers": {...}, "body": "<raw text>"}]} ->
 * {"waits": [...], "posts": n, "status": <last status>, "clock": t,
 *  "open_at": t}.  Replays post_paced: a bucket that refills in the future is
 * waited for, X-RateLimit-Remaining=="0" pushes the refill, a 429 retries
 * (body retry_after wins over the header) while attempt < MAX_RETRIES. */
json_t *w24_standalone_e0a2a7_pace(const json_t *args);

/* _MAX_RATE_LIMIT_RETRIES and the API base. */
json_t *w24_standalone_e0a2a7_constants(void);

#ifdef __cplusplus
}
#endif
#endif
