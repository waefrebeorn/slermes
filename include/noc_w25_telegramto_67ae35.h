/*
 * noc_w25_telegramto_67ae35.h - C surface for upstream
 * plugins/platforms/telegram/telegram_topic_transfer.py
 * (gate w25_telegramto_67ae35).
 *
 * Opt-in root-DM -> fresh topic transfer.  The adapter, the awaitable transfer
 * hook, the topic lookup and the channel-prompt resolution are seams; ported
 * here is the guard chain that decides whether a root DM may be moved at all,
 * how the moved source is rewritten (fresh topic vs lobby-recovery fallback),
 * and which of the recovered topic's prompt inputs replace the lobby's.
 */
#ifndef NOC_W25_TELEGRAMTO_67AE35_H
#define NOC_W25_TELEGRAMTO_67AE35_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* request: {"has_transfer": bool, "source": {...}|null, "internal": bool,
 *           "command": <str|null>, "metadata": {...},
 *           "allow_gateway_control": bool, "authorized": true|false|null,
 *           "decided": null | {"thread_id": <any>, "created": bool} |
 *                      {"raise": <name>}} ->
 * {"moved", "thread_id", "message_id", "reply_anchor_override",
 *  "prompt_applied"}.  A message that is not moved reports every field null.
 * A fresh topic clears message_id (the root DM cannot anchor a reply there)
 * and sets reply_anchor_override to ""; the fallback topic keeps the anchor. */
json_t *w25_telegramto_67ae35_transfer(const json_t *request);

/* request: {"chat_id", "thread_id", "topic": {"name", "skill"},
 *           "channel_prompt": <resolved value>} ->
 * {"chat_topic", "auto_skill", "channel_prompt"} as set on the event. */
json_t *w25_telegramto_67ae35_prompt_inputs(const json_t *request);

/* request: {"before_thread": <any|null>, "after_thread": <any|null>,
 *           "topic": {...}, "channel_prompt": <resolved value>} ->
 * {"applied": bool, "chat_topic": <any|null>}: prompt inputs are re-applied
 * only when recovery moved the event into a different topic. */
json_t *w25_telegramto_67ae35_recovery(const json_t *request);

#ifdef __cplusplus
}
#endif
#endif
