#ifndef FILE_MUTATION_VERIFIER_H
#define FILE_MUTATION_VERIFIER_H

#include <stdbool.h>
#include <stddef.h>

/*
 * file_mutation_verifier.h — Port of Python run_agent.AIAgent
 *   _record_file_mutation_result() + _format_file_mutation_failure_footer()
 * MIT License — WuBu Slermes Project
 *
 * Per-turn file write verification. Tracks write_file/patch outcomes and
 * renders a failure footer at turn end. (GAP 2 from the original
 * hermes_gap_fixes.c monolith — split into a self-contained module.)
 */

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

#endif /* FILE_MUTATION_VERIFIER_H */
