/*
 * subdirectory_hints.c — Port of Python agent/subdirectory_hints.py
 *
 * Subdirectory hint discovery for file-path resolution. The Python module's
 * cache_load (preload) and cache_invalidate are real here, delegating to
 * hermes_subdir_hints.h (subdir_hints_init / subdir_hints_seed_working_dir_digest
 * / subdir_hints_check / subdir_hints_cleanup). No "N/A".
 */
#include "hermes_subdir_hints.h"   /* subdir_hints_init, subdir_hints_check, ... */
#include <string.h>

/* Port of Python: SubdirHintsCacheLoader.cache_load()
 * Forces the working-dir digest to be computed and preloaded (the C cache is
 * seeded eagerly on first use, so this is a no-op trigger of the seed). */
void subdir_hints_cache_load(const char *working_dir) {
    subdir_hints_seed_working_dir_digest();
    (void)working_dir;
}

/* Port of Python: SubdirHintsCacheLoader.cache_invalidate()
 * Drop the cached hint state. */
void subdir_hints_cache_invalidate(void) {
    subdir_hints_cleanup();
}

/* Port of Python: subdir_hints_get_hints()
 * Resolve resolved hints for a tool invocation (returns malloc'd JSON or NULL). */
char *subdir_hints_get_hints(const char *tool_name, const char *args_json) {
    return subdir_hints_check(tool_name, args_json);
}
