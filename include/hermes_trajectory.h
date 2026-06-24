#ifndef HERMES_TRAJECTORY_H
#define HERMES_TRAJECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert <REASONING_SCRATCHPAD> tags to <think> tags.
 *
 * @param content  Input string (may be NULL).
 * @return Newly allocated string with tags replaced, or NULL on allocation
 *         failure. Caller must free() the result.
 */
char *convert_scratchpad_to_think(const char *content);

/**
 * Check if content has an opening <REASONING_SCRATCHPAD> without a closing tag.
 *
 * @param content  Input string (may be NULL).
 * @return true if opening tag exists without matching closing tag.
 */
bool has_incomplete_scratchpad(const char *content);

/**
 * Append a trajectory entry to a JSONL file.
 *
 * @param trajectory  JSON-serialized string of the conversation array.
 * @param model       Model name for metadata.
 * @param completed   Whether the conversation completed successfully.
 * @param filename    Output file path. If NULL, defaults to
 *                    "trajectory_samples.jsonl" (completed) or
 *                    "failed_trajectories.jsonl" (!completed).
 * @return 0 on success, -1 on error.
 */
int save_trajectory(const char *trajectory_json,
                            const char *model,
                            bool completed,
                            const char *filename);

/**
 * Convert an array of message_t to trajectory training JSON format.
 *
 * @param messages  Array of message_t structs.
 * @param count     Number of messages in the array.
 * @return Newly allocated JSON string in trajectory format, or NULL on error.
 *         Caller must free() the result.
 */
char *convert_to_trajectory_format(const message_t *messages, int count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TRAJECTORY_H */
