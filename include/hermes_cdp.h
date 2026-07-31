/*
 * hermes_cdp.h — Public umbrella for the CDP (Chrome DevTools Protocol)
 * browser-tool subsystem.
 *
 * Self-contained: delegates to the real implementation header under
 * src/tools/. CLI/gateway code includes this umbrella only — no cross-subsystem
 * path includes.
 *
 * C11 only.
 */
#ifndef SLERMES_HERMES_CDP_H
#define SLERMES_HERMES_CDP_H

/* cdp_get_url / cdp_set_url declared in the implementation header below. */
#include "../src/tools/browser_tool_cdp.h"

#endif /* SLERMES_HERMES_CDP_H */
