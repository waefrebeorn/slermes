#ifndef NOC_W22_SETUP_424F2E_H
#define NOC_W22_SETUP_424F2E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/signal/setup.py (gate w22_setup_424f2e).
 *
 * `interactive_setup` is a terminal wizard: prompts, prints, the signal-cli
 * PATH probe and the HTTP reachability check are the UI/network seams. The
 * flow it drives is pure and ported here as one scripted run:
 *
 *   - default URL = existing SIGNAL_HTTP_URL or http://127.0.0.1:8080
 *   - a blank answer takes the default, EOF/Ctrl+C cancels the step
 *   - a failed probe asks "continue anyway"; declining aborts unsaved
 *   - account is required; allowed users fall back to the account
 *   - groups are opt-in; the group list falls back to existing or "*"
 *
 * Fixture keys: existing_url, existing_account, existing_allowed,
 * existing_groups, reconfigure, which, inputs (array of string|null),
 * yes (array of bool), http ({"kind":"ok"|"status"|"raise","status":N}),
 * quiet probes.
 * Out: {"saves":[[key,value],...], "outcome":str, "found":bool,
 *       "checked_url":str|null, "groups_line":"enabled"|"disabled"}
 */

/* PoP: w22_setup_424f2e_run @ plugins/platforms/signal/setup.py:interactive_setup
 * One deterministic wizard run over the scripted answers. */
json_t *w22_setup_424f2e_run(const json_t *req);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_SETUP_424F2E_H */
