/*
 * noc_w26_display_756d88.h - C surface for upstream
 * hermes_cli/web_routers/display.py (gate w26_display_756d88).
 *
 * The /api/display/ws RFB bridge is all socket: a viewer WebSocket spliced into
 * this profile's Xvnc.  What is pure is the input gate's eviction decision (a
 * function of the per-connection memory `held` and the lease) and the close
 * codes / pump budgets the bridge closes and chunks with.
 */
#ifndef NOC_W26_DISPLAY_756D88_H
#define NOC_W26_DISPLAY_756D88_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req {"held":{"ever":v},"lease":{"holder":h,"viewer_id":id},"viewer_id":v} ->
 * {"evict": bool, "ever": bool} with `held` updated as the live function does. */
json_t *w26_display_756d88_should_evict(const json_t *req);

/* The close codes and pump budgets, as an object. */
json_t *w26_display_756d88_constants(void);

#ifdef __cplusplus
}
#endif
#endif
