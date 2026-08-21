#ifndef HERMES_MEMORY_TOOL_PURE_H
#define HERMES_MEMORY_TOOL_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from tools/memory_tool.py.
 * All return freshly malloc'd strings (free with free()) or NULL. */

char *memory_tool_get_memory_dir(void);
char *memory_tool_drift_error(const char *path_name, const char *bak_path);
char *memory_tool_read_failed_error(const char *path_name);

typedef struct memory_tool_pure_store memory_tool_pure_store_t;
memory_tool_pure_store_t *memory_tool_pure_store_new(void);
void memory_tool_pure_store_free(memory_tool_pure_store_t *s);
void memory_tool_pure_store_reset_consolidation_failures(memory_tool_pure_store_t *s);
char *memory_tool_pure_store_consolidation_failure(memory_tool_pure_store_t *s, const char *response_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MEMORY_TOOL_PURE_H */
