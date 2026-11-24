/*
 * update_launch_files.c — C port of upstream Hermes hermes_cli/update_launch_files.py.
 *
 * PoP: w18_updatelaun_89b41e_owned @ hermes_cli/update_launch_files.py:_owned_by_this_checkout
 * PoP: w18_updatelaun_89b41e_refreshers @ hermes_cli/update_launch_files.py:_refreshers
 *
 * The definition-path lookups and definition_foreign_checkout() are injected
 * seams; the ownership decision and platform refresher table are ported verbatim.
 */
#include "noc_w18_updatelaun_89b41e.h"
#include <string.h>

const char *w18_updatelaun_89b41e_FOLLOWUP_STEP = "launch_files";

int w18_updatelaun_89b41e_owned(const char *kind, const char *foreign) {
    /* kinds with a definition-path probe (all others return True up front) */
    if (!kind) return 1;
    if (strcmp(kind, "systemd user unit") && strcmp(kind, "systemd system unit") &&
        strcmp(kind, "launchd plist"))
        return 1;
    /* Python: `foreign is None` -> True; any string (including "") -> False. */
    return foreign == NULL ? 1 : 0;
}

json_t *w18_updatelaun_89b41e_refreshers(const char *platform, int systemd_support) {
    json_t *out = json_array();
    if (!platform) return out;
    if (!strcmp(platform, "win32")) {
        json_append(out, json_string("Windows gateway launcher"));
        return out;
    }
    if (!strcmp(platform, "darwin")) {
        json_append(out, json_string("launchd plist"));
        return out;
    }
    if (systemd_support) json_append(out, json_string("systemd user unit"));
    return out;
}
