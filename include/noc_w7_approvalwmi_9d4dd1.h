#ifndef NOC_W7_APPROVALWMI_9D4DD1_H
#define NOC_W7_APPROVALWMI_9D4DD1_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Port of the self-contained detection logic of tools/approval_wmic.py.
 * The borrowed shell word/segment parser from tools.approval_detection is not
 * re-implemented here; callers supply the carrier rest / command segment.
 * Every exported symbol is prefixed with the gate id. */

/* PoP: w7_approvalwmi_9d4dd1_search @ tools/approval_wmic.py:WmicShadowDeleteMatcher.search
 * True when "wmic" appears in text.lower() (the fast guard of search). */
int w7_approvalwmi_9d4dd1_has_wmic(const char *text);

/* PoP: w7_approvalwmi_9d4dd1_carrier_script @ tools/approval_wmic.py:WmicShadowDeleteMatcher._carrier_script
 * Returns a JSON array of the script variants produced by _carrier_script.
 * Caller owns the returned node. */
json_t *w7_approvalwmi_9d4dd1_carrier_script(const char *name, const char *rest);

/* PoP: w7_approvalwmi_9d4dd1_wmic_delete_in_segment @ tools/approval_wmic.py:_WMIC_DELETE_IN_SEGMENT_RE
 * True when _WMIC_DELETE_IN_SEGMENT_RE.search(" " + segment) matches. */
int w7_approvalwmi_9d4dd1_wmic_delete_in_segment(const char *segment);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_APPROVALWMI_9D4DD1_H */
