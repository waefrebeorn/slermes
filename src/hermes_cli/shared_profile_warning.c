/*
 * shared_profile_warning.c — C port of hermes_cli/shared_profile_warning.py.
 * PoP: shrprof_shared_profile_warning @ hermes_cli/shared_profile_warning.py:shared_profile_warning
 *
 * Python:
 *   own_install = install_id(project_root)
 *   home_key = hermes_home_key(home)
 *   if any(entry.get("install") and entry["install"] != own_install
 *          and entry.get("hermes_home") == home_key
 *          for entry in ledger_entries(all_installs=True, verified_only=True)):
 *       return WARNING
 *   return ""
 */
#include "noc_shrprof.h"
#include <string.h>

static const char WARNING[] =
    "Another Hermes installation is using this profile. Both installations share "
    "its settings and data, so changes can conflict. You can continue, or close "
    "the other installation before making changes.";

const char *shrprof_shared_profile_warning(const char *own_install,
                                           const char *home_key,
                                           const shrprof_ledger_entry_t *entries,
                                           size_t count) {
    if (!own_install) own_install = "";
    if (!home_key) home_key = "";
    for (size_t i = 0; i < count; i++) {
        const shrprof_ledger_entry_t *e = &entries[i];
        /* entry.get("install") must be truthy */
        if (!e->has_install || !e->install || e->install[0] == '\0') continue;
        /* entry["install"] != own_install */
        if (strcmp(e->install, own_install) == 0) continue;
        /* entry.get("hermes_home") == home_key */
        if (!e->has_home || !e->hermes_home) continue;
        if (strcmp(e->hermes_home, home_key) == 0) return WARNING;
    }
    return "";
}
