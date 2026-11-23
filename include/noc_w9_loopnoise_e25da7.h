#ifndef NOC_W9_LOOPNOISE_E25DA7_H
#define NOC_W9_LOOPNOISE_E25DA7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure teardown-noise predicate of tui_gateway/loop_noise.py.
 * Every symbol is prefixed with the gate id. */

/* PoP: w9_loopnoise_e25da7_is_benign_teardown @ tui_gateway/loop_noise.py:_is_benign_teardown
 * exc_kind is the exception class name (or "none"/unknown).
 * callback_repr / handle_repr mirror repr(context.get("callback")) and
 * repr(context.get("handle")); NULL means the key is absent / value is None.
 * Returns 1 only for a ConnectionReset/Aborted/BrokenPipe error whose callback
 * or handle repr contains "_call_connection_lost". */
int w9_loopnoise_e25da7_is_benign_teardown(const char *exc_kind,
                                           const char *callback_repr,
                                           const char *handle_repr);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_LOOPNOISE_E25DA7_H */
