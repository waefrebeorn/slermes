/*
 * patch.h — Public API for the V4A patch applier in patch.c.
 */

#ifndef HERMES_PATCH_H
#define HERMES_PATCH_H

#include <stdbool.h>

/* Apply a V4A-format patch (*** Begin Patch ... *** End Patch) to the
 * filesystem. Returns a malloc'd JSON PatchResult string
 * ({"success":true,"files":[...],"diff":"..."} or {"error":...}).
 * Caller frees. dry_run skips the actual file writes. */
char *patch_apply_v4a(const char *patch_content, bool dry_run);

#endif /* HERMES_PATCH_H */
