#ifndef NOC_W21_UPDATECMDF_DF3D7D_H
#define NOC_W21_UPDATECMDF_DF3D7D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/update_cmd_fleet_checkout.py (gate w21_updatecmdf_df3d7d).
 *
 * `checkout_contains` asks whether the SHA a pull landed on is still contained
 * in the code this checkout runs. Two seams are injected instead of reached:
 *
 *   identity -- what `build_info.get_code_identity()` returns
 *               ({"source": "build-file"|"git", "sha": ...}); NULL/absent reads
 *               as the `or {}` empty dict.
 *   probe    -- what the `run_git(["merge-base","--is-ancestor",sha,"HEAD"])`
 *               call yielded: {"returncode": n} or {"error": "<message>"} for a
 *               raise. A missing/null probe reads as a raise.
 *
 * Fail-closed: every unknown answer is False, exactly as upstream's contract
 * says ("an unknown ancestry is not evidence that the fleet serves the update").
 */

/* PoP: w21_updatecmdf_df3d7d_checkout_contains @ hermes_cli/update_cmd_fleet_checkout.py:checkout_contains
 * 1 when contained, else 0. A Docker/Cloud image ("source" == "build-file") has
 * no history, so containment collapses to the build stamp: `stamped == sha ||
 * stamped.startswith(sha) || sha.startswith(stamped)`, and an empty stamp is
 * never contained. */
int w21_updatecmdf_df3d7d_checkout_contains(const char *sha, const json_t *identity,
                                            const json_t *probe);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_UPDATECMDF_DF3D7D_H */
