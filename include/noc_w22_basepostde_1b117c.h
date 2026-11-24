#ifndef NOC_W22_BASEPOSTDE_1B117C_H
#define NOC_W22_BASEPOSTDE_1B117C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/base_post_delivery.py (gate w22_basepostde_1b117c).
 *
 * Two pure decisions of the one-shot post-delivery callback: which run
 * generation a turn's event carries, and what the bounded callback run leaves
 * observable. The event object, the callback coroutine and the asyncio loop
 * stay upstream.
 */

/* PoP: w22_basepostde_1b117c_turn_run_generation @ gateway/platforms/base_post_delivery.py:turn_run_generation
 * `getattr(event, "_hermes_run_generation", None)`, kept only when
 * `isinstance(generation, int) and not isinstance(generation, bool)`.
 * `event` is the attribute map of the turn's event object; answer is the
 * generation as a number, or null for None. */
json_t *w22_basepostde_1b117c_turn_run_generation(const json_t *event);

/* PoP: w22_basepostde_1b117c_callback_outcome @ gateway/platforms/base_post_delivery.py:run_post_delivery_callback
 * The run's observable shape from the callback's declared nature:
 *   desc = {"callable": bool, "awaitable": bool, "raises": bool, "timed_out": bool}
 * Answer: {"invoked": bool, "awaited": bool, "raised": false, "returned_none": true}.
 * `callable` gates the call; `awaitable` (only reached when the callback
 * returns normally) gates `asyncio.wait_for`; TimeoutError and Exception are
 * both suppressed, so nothing the callback does reaches the caller. */
json_t *w22_basepostde_1b117c_callback_outcome(const json_t *desc);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_BASEPOSTDE_1B117C_H */
