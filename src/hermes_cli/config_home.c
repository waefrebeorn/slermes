/* config_home.c — C home for upstream Hermes hermes_cli/config_home.py.
 *
 * PoP: w8_confighome_config_load_issue_ex @ hermes_cli/config_home.py:config_load_issue
 *
 * Faithful port of config_load_issue's two-branch classification:
 *   if isinstance(exc, (HomeInitializationError, OSError)):
 *       return ConfigIssue("error", f"Hermes storage is unavailable: {exc}",
 *                          "Check the reported path, link target, mount and permissions; "
 *                          "keep config.yaml unchanged.")
 *   return ConfigIssue("error", "Could not load config.yaml",
 *                      "Run 'hermes setup' to create a valid config")
 * `exc_kind` is one of "home" (HomeInitializationError), "os" (OSError) or any
 * other token (other exceptions); `exc_message` is str(exc). Both are injected
 * through the seam because the exception types live only on the Python side.
 */
#include "noc_w8_confighome_736bc7.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

json_t *w8_confighome_config_load_issue_ex(const char *exc_kind, const char *exc_message) {
    const char *msg = exc_message ? exc_message : "";
    int storage = (exc_kind && (!strcmp(exc_kind, "home") || !strcmp(exc_kind, "os")));

    json_t *o = json_object();
    json_set(o, "severity", json_string("error"));
    if (storage) {
        char buf[8192];
        snprintf(buf, sizeof(buf), "Hermes storage is unavailable: %s", msg);
        json_set(o, "message", json_string(buf));
        json_set(o, "remediation",
                 json_string("Check the reported path, link target, mount and permissions; "
                             "keep config.yaml unchanged."));
    } else {
        json_set(o, "message", json_string("Could not load config.yaml"));
        json_set(o, "remediation", json_string("Run 'hermes setup' to create a valid config"));
    }
    return o;
}
