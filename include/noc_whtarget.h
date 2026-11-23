#ifndef NOC_WHTARGET_H
#define NOC_WHTARGET_H
/*
 * Public C API for the port of gateway/webhook_targets.py (gate whtarget).
 * All exported symbols carry the whtarget_ prefix.
 */
/*
 * PoP: whtarget_is_known_platform @ gateway/webhook_targets.py:is_known_platform
 * Builtin cross-platform delivery names. Plugin-registered platforms are
 * resolved by the caller (a pure C port cannot query the live plugin registry);
 * see notes. Unknown names return 0, matching the live function's failure path.
 */
int whtarget_is_known_platform(const char *name);

/*
 * PoP: whtarget_is_dispatchable_target @ gateway/webhook_targets.py:is_dispatchable_target
 * A saved route's deliver value that the gateway can send to.
 */
#include "hermes_json.h"
int whtarget_is_dispatchable_target(const json_t *target);

#endif /* NOC_WHTARGET_H */
