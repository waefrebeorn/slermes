/*
 * compaction_display.h — C11 port of agent/compaction_display.py.
 *
 * Client-facing projection helpers for model-only compaction carriers.
 */
#ifndef SLERMES_COMPACTION_DISPLAY_H
#define SLERMES_COMPACTION_DISPLAY_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * project_compaction_message_for_display @ agent/compaction_display.py:
 * project_compaction_message_for_display
 *
 * Return authentic transcript content as a NEW json object (caller frees),
 * or NULL for a pure handoff (and on NULL / non-object input).
 */
json_t *project_compaction_message_for_display(const json_t *message);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_COMPACTION_DISPLAY_H */
