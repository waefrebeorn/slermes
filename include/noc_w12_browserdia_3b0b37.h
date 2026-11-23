#ifndef NOC_W12_BROWSERDIA_3B0B37_H
#define NOC_W12_BROWSERDIA_3B0B37_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_browserdia_3b0b37_browser_dialog @ tools/browser_dialog_tool.py:browser_dialog
 * All four live Python args are explicit. `supervisor` (null => registry.get returned
 * None) and `respond_result` (the dict from respond_to_dialog) are injected seams so
 * the tool can be exercised without a live CDP endpoint. Returns malloc'd JSON text. */
char *w12_browserdia_3b0b37_browser_dialog(const char *action,
                                           const char *prompt_text,
                                           const char *dialog_id,
                                           const char *task_id,
                                           const struct json_t *supervisor,
                                           const struct json_t *respond_result);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_BROWSERDIA_3B0B37_H */
