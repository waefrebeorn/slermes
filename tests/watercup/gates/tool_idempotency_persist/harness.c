/* water-cup gate: tool_idempotency_persist — verifies RESTART-SURVIVAL of the
 * idempotency cache (src/tools/tool_idempotency.c). Uses the REAL module.
 *
 * No argv needed: creates a private temp dir via mkdtemp, then:
 *   1. new cache A, attach to <tmp>/idempotency_cache.json, put("k","v1",600)
 *   2. free A (flushes to disk)
 *   3. new cache B, attach to the SAME file (simulates process restart)
 *   4. get("k") on B must return "v1" (recovered from disk)
 * Exit 0 iff recovered == "v1".
 */
#include "tool_idempotency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* mkdtemp, rmdir */

int main(void) {
    char tmpl[512];
    snprintf(tmpl, sizeof tmpl, "/tmp/idem_persist_XXXXXX");
    char *dir = mkdtemp(tmpl);
    if (!dir) { fprintf(stderr, "mkdtemp failed\n"); return 3; }

    char path[1024];
    snprintf(path, sizeof path, "%s/idempotency_cache.json", dir);

    tool_idem_cache_t *a = tool_idem_new();
    tool_idem_attach(a, path);
    if (!tool_idem_put(a, "k", "v1", 600)) { fprintf(stderr, "put failed\n"); return 3; }
    tool_idem_free(a);   /* flush to disk */

    /* simulate restart: fresh process, same file */
    tool_idem_cache_t *b = tool_idem_new();
    tool_idem_attach(b, path);
    char *got = tool_idem_get(b, "k");
    int rc = (got && strcmp(got, "v1") == 0) ? 0 : 1;
    if (got) free(got);
    tool_idem_free(b);

    /* clean up */
    unlink(path);
    rmdir(dir);
    return rc;
}
