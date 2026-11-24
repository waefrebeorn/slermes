/*
 * noc_w26_telegramad_942de7.h - C surface for upstream
 * plugins/platforms/telegram/telegram_addressing.py (gate w26_telegramad_942de7).
 *
 * Who a Telegram group message is addressed to, as pure text decisions: the
 * UTF-16 entity offset map, the handle a mention/bot_command entity names, the
 * LEADING handle run of a text, the opening `/cmd@bot` match, and the trigger
 * text cleanup.  The message object is a seam -- its (text, entities) pair
 * arrives as values, exactly what `addressing_sources` yields.
 */
#ifndef NOC_W26_TELEGRAMAD_942DE7_H
#define NOC_W26_TELEGRAMAD_942DE7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W26_TELEGRAMAD_942DE7_SEPARATORS " \t\n\r,:;"

/* offsets[utf16_index] -> str index, low half of a surrogate pair mapping to the pair. */
json_t *w26_telegramad_942de7_utf16_offsets(const char *text);

/* mention -> the handle; bot_command -> its @bot suffix (bare /cmd names no one). */
json_t *w26_telegramad_942de7_entity_handle(const char *etype, const char *span);

/* Handles at the head of the text, in yield order. */
json_t *w26_telegramad_942de7_leading_handles(const char *text, const json_t *entities);

/* The same, kept only when is_bot_handle(handle) and deduped/sorted. */
json_t *w26_telegramad_942de7_leading_bot_handles(const char *text,
                                                  const json_t *entities,
                                                  const json_t *bots);

/* The opening command match: null, or {start1, len1, end, group2}. */
json_t *w26_telegramad_942de7_command_after_address(const char *text,
                                                    const char *bot_username,
                                                    int has_entities);

/* An opening command that explicitly targets another bot, or null. */
json_t *w26_telegramad_942de7_foreign_command_target(const char *text,
                                                     const json_t *entities,
                                                     const char *bot_username);

/* Our @handle dropped from trigger text, keeping `/cmd@bot args` separated. */
json_t *w26_telegramad_942de7_clean_trigger(const char *text, const char *bot_username);

#ifdef __cplusplus
}
#endif
#endif
