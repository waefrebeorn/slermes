#ifndef NOC_W8_CONFHOME_736BC7_H
#define NOC_W8_CONFHOME_736BC7_H
#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/config_home.py (gate w8_confighome_736bc7).
 *
 * PoP: w8_confighome_config_load_issue_ex @ hermes_cli/config_home.py:config_load_issue
 *
 * config_load_issue(exc) classifies the exception that broke config loading.
 * HomeInitializationError and OSError yield a storage-unavailable ConfigIssue
 * whose message embeds str(exc); any other exception yields the generic
 * "Could not load config.yaml" issue. The Python exception type/message are
 * injected through this explicit seam (the exception classes are Python-only).
 *
 * Returns a JSON object: {"severity": str, "message": str, "remediation": str}.
 */
json_t *w8_confighome_config_load_issue_ex(const char *exc_kind, const char *exc_message);

#endif /* NOC_W8_CONFHOME_736BC7_H */
