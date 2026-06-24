#ifndef HERMES_GAP_FIXES_H
#define HERMES_GAP_FIXES_H

#include <stdbool.h>
#include <stddef.h>

/* Forward declaration (actual struct defined in hermes.h/agent_state_t via pointer) */
struct agent_state_t;

/* ==================================================================
 *  GAP 1: todo_hydrate
 *  Port of Python: run_agent.AIAgent._hydrate_todo_store
 * ================================================================== */

/* Scan context messages for the last todo tool response and replay into store.
 * Returns number of items restored, or 0 if none found / error. */
int todo_hydrate_from_context(void *state);

/* ==================================================================
 *  GAP 2: file_mutation_verifier
 *  Port of Python: run_agent.AIAgent._record_file_mutation_result
 *                   + _format_file_mutation_failure_footer
 * ================================================================== */

/* Max tracked mutations per turn */
#define MAX_FILE_MUTATIONS 32
#define MUTATION_PATH_MAX 512
#define MUTATION_PREVIEW_MAX 256

typedef struct {
    char path[MUTATION_PATH_MAX];
    char tool[64];
    char error_preview[MUTATION_PREVIEW_MAX];
    bool is_error;
} file_mutation_entry_t;

typedef struct file_mutation_tracker_t {
    file_mutation_entry_t entries[MAX_FILE_MUTATIONS];
    int count;
} file_mutation_tracker_t;

/* Init/reset the per-turn mutation tracker */
void file_mutation_tracker_init(file_mutation_tracker_t *tracker);

/* Record a file mutation outcome. */
void file_mutation_tracker_record(file_mutation_tracker_t *tracker,
                                  const char *tool_name,
                                  const char *args_json,
                                  const char *result,
                                  bool is_error);

/* Format the failure footer string. Returns empty string if no failures.
 * Caller must free the returned string. */
char *file_mutation_tracker_format_footer(const file_mutation_tracker_t *tracker);

/* ==================================================================
 *  GAP 3: api_error_summary
 *  Port of Python: run_agent.AIAgent._summarize_api_error
 * ================================================================== */

/* Summarize an API error into a human-readable string.
 * Returns a malloc'd string the caller must free. */
char *summarize_api_error(const char *raw_error);

#endif /* HERMES_GAP_FIXES_H */
