/*
 * noc_w27_adapter_1f12c3_p13.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p13).
 *
 * prompt/approval shaping and thread parts (adapter.py:3224, 5317-5620).
 * PORTED (pure): the created-thread result payload (thread object seamed), the
 * self-contained prompt budgeter that truncates to the 2000-char message cap,
 * the approval mention line and the forum-thread id/message-id resolution.
 * SEAM: discord.py thread objects (dropped from the result), the t() truncation
 * marker (supplied by the fixture and compared against the live catalog) and
 * the adapter's 2000-char MAX_MESSAGE_LENGTH.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P13_H
#define NOC_W27_ADAPTER_1F12C3_P13_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p13_thread_created(const json_t *thread, const json_t *name);
json_t *w27_adapter_1f12c3_p13_self_contained_prompt_content(const json_t *header, const json_t *body, const json_t *code_block, const json_t *tail, const json_t *max_len, const json_t *marker);
json_t *w27_adapter_1f12c3_p13_approval_mention_content(const json_t *flag, const json_t *ids);
json_t *w27_adapter_1f12c3_p13_forum_thread_parts(const json_t *thread);

#ifdef __cplusplus
}
#endif
#endif

