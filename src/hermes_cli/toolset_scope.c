/*
 * toolset_scope.c — C port of upstream Hermes hermes_cli/toolset_scope.py.
 * PoP: tscope_toolset_allowed_for_platform @ hermes_cli/toolset_scope.py:toolset_allowed_for_platform
 */
#include "noc_tscope.h"
#include <string.h>

/* Toolsets without a restriction entry are available on every platform. */
static const char *const RESTRICTED[] = { "discord", "discord_admin", NULL };

int tscope_toolset_allowed_for_platform(const char *ts_key, const char *platform) {
    const char *k = ts_key ? ts_key : "";
    int restricted = 0;
    for (int i = 0; RESTRICTED[i]; i++) {
        if (!strcmp(k, RESTRICTED[i])) { restricted = 1; break; }
    }
    if (!restricted) return 1;
    const char *p = platform ? platform : "";
    return !strcmp(p, "discord");
}
