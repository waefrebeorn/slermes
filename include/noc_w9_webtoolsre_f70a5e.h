#ifndef NOC_W9_WEBTOOLSRE_F70A5E_H
#define NOC_W9_WEBTOOLSRE_F70A5E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure policy-refusal predicate of tools/web_tools_rescue.py.
 * Every symbol is prefixed with the gate id. */

/* PoP: w9_webtoolsre_f70a5e_policy_blocked_result @ tools/web_tools_rescue.py:_policy_blocked_result
 * Mirrors: error = str(result.get("error") or "").lower();
 *          return bool(result.get("blocked_by_policy")) or "blocked by website policy" in error.
 * result is a JSON object. */
int w9_webtoolsre_f70a5e_policy_blocked_result(const json_t *result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_WEBTOOLSRE_F70A5E_H */
