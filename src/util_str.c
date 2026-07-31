/* Shared string / path utilities — single canonical definitions.
 * See include/hermes_util_str.h. Replaces divergent copy-pasted helpers. */

#include "hermes_util_str.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void hermes_home_dir(char *out, size_t sz) {
    if (!out || sz == 0) return;
    const char *h = getenv("HERMES_HOME");
    if (h && h[0]) { snprintf(out, sz, "%s", h); return; }
    h = getenv("SLERMES_HOME");
    if (h && h[0]) { snprintf(out, sz, "%s", h); return; }
    h = getenv("HOME");
    snprintf(out, sz, "%s/.hermes", h ? h : ".");
}
