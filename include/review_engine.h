/*
 * review_engine.h — C11 port of agent/review_engine.py (the /review engine).
 */
#ifndef REVIEW_ENGINE_H
#define REVIEW_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* DEFAULT_CONTEXT_MESSAGES — recent chat messages the reviewer receives. */
#define REVIEW_ENGINE_DEFAULT_CONTEXT_MESSAGES 10
/* _MESSAGE_CHAR_CAP — per-message excerpt cap. */
#define REVIEW_ENGINE_MESSAGE_CHAR_CAP 12000

/* snapshot_recent_messages(messages, limit): last `limit` user/assistant
 * messages as [{"role","text"}], oldest first (json_free the result).
 * limit <= 0 uses the default. */
json_t *review_engine_snapshot_recent_messages(const json_t *messages, int limit);

/* collect_parent_loaded_skills(ephemeral_system_prompt, messages, limit):
 * preloaded + history-loaded skill names, deduped, capped at limit
 * (limit <= 0 → 8). json_free the returned array of strings. */
json_t *review_engine_collect_parent_loaded_skills(const char *ephemeral_system_prompt,
                                                   const json_t *messages,
                                                   int limit);

/* build_review_task(snapshot, user_prompt, loaded_skills, &goal, &context):
 * compose the reviewer subagent's (goal, context) pair. Both strings are
 * malloc'd; caller frees. Returns false on OOM. */
bool review_engine_build_review_task(const json_t *snapshot,
                                     const char *user_prompt,
                                     const json_t *loaded_skills,
                                     char **out_goal,
                                     char **out_context);

/* format_dispatch_note(result, user_prompt): human-facing one-liner for a
 * successful dispatch. Malloc'd; caller frees. */
char *review_engine_format_dispatch_note(const json_t *result,
                                         const char *user_prompt);

#endif /* REVIEW_ENGINE_H */
