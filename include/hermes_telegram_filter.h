/* hermes_telegram_filter.h — Telegram message filtering helpers
 * Port of Python TelegramAdapter _should_process_message, _should_observe, etc.
 *
 * Functions for checking if a Telegram message should be processed based on
 * configured allow/deny lists (allowed_chats, topics, threads), mention
 * detection, mention patterns, exclusive bot routing, and guest mode.
 */

#ifndef HERMES_TELEGRAM_FILTER_H
#define HERMES_TELEGRAM_FILTER_H

#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdbool.h>

/* ── Config helper ───────────────────────────────────────────── */

/* Get the platform config for a named platform (telegram, discord, etc.).
 * Returns NULL if not found. Thin wrapper around hermes_config_get_platform. */
const hermes_platform_cfg_t *tg_get_config(void);

/* ── Chat/topic/thread filtering ─────────────────────────────── */

/* Check if a chat_id is in a comma-separated list.
 * Returns true if list is empty (no restriction), or chat_id is in the list. */
bool tg_chat_in_list(const char *chat_id, const char *comma_list);

/* Check if an integer thread_id is in a comma-separated list (ignored_threads). */
bool tg_thread_in_list(int thread_id, const char *comma_list);

/* Check if a chat_id is in the allowed_chats whitelist.
 * If allowed_chats is empty, returns true (no restriction). */
bool tg_chat_is_allowed(const char *chat_id);

/* Check if a chat_id is in the free_response_chats list. */
bool tg_chat_is_free_response(const char *chat_id);

/* Check if topic_id (as string) is in allowed_topics.
 * If allowed_topics is empty, returns true (no restriction). */
bool tg_topic_is_allowed(const char *topic_id);

/* Check if thread_id (as integer) is in ignored_threads.
 * thread_id may be NULL (General topic = 1). */
bool tg_thread_is_ignored(int thread_id);

/* ── Mention/group filtering ─────────────────────────────────── */

/* Check if require_mention is enabled for this Telegram config. */
bool tg_require_mention(void);

/* Check if exclusive_bot_mentions is enabled. */
bool tg_exclusive_bot_mentions(void);

/* Check if guest_mode is enabled. */
bool tg_guest_mode(void);

/* Check if observe_unmentioned is enabled. */
bool tg_observe_unmentioned(void);

/* Check if a chat_id is in group_allowed_chats (with allowed_chats intersection logic). */
bool tg_chat_is_group_allowed(const char *chat_id);

/* Check if a chat_id is in observe_allowed_chats (intersection of group+allowed). */
bool tg_chat_is_observe_allowed(const char *chat_id);

/* ── Mention pattern matching ────────────────────────────────── */

/* Compile and cache regex patterns from mention_patterns config.
 * Returns true if any patterns are loaded. */
bool tg_mention_patterns_loaded(void);

/* Check if text matches any compiled mention pattern.
 * Returns true if any pattern matches. */
bool tg_text_matches_mention_patterns(const char *text);

/* ── Multi-bot exclusive mention routing ─────────────────────── */

/* Check if text contains explicit @...bot handles targeting other bots.
 * Returns true when other bots are mentioned but NOT this one.
 * bot_username should be the bot's @name without @. */
bool tg_explicit_other_bot_mention(const char *text, const char *bot_username);

/* Extract bot usernames from text (@...bot pattern).
 * Fills a dynamically allocated array; caller must free each string and the array.
 * Returns the count of found handles. */
int tg_extract_bot_handles(const char *text, const char ***out_handles);

/* ── Main filtering decision ─────────────────────────────────── */

/* Determine if a Telegram message should be processed (port of Python
 * _should_process_message). Returns true if the message should be dispatched.
 * chat_type: "dm", "group", "channel", "supergroup"
 * chat_id: the chat ID string
 * text: message text
 * thread_id: message_thread_id (may be NULL)
 * is_group: true if group/supergroup
 * is_mentioned: true if the bot is @mentioned
 * is_reply: true if replying to bot's message
 * bot_username: the bot's @name without @ */
bool tg_should_process_message(const char *chat_type, const char *chat_id,
                                const char *text, const char *thread_id,
                                bool is_group, bool is_mentioned,
                                bool is_reply_to_bot,
                                const char *bot_username);

/* Determine if a message should be observed (stored as context but not dispatched).
 * Port of Python _should_observe_unmentioned_group_message. */
bool tg_should_observe_message(const char *chat_type, const char *chat_id,
                                const char *text, const char *thread_id,
                                bool is_group, bool is_mentioned,
                                bool is_reply_to_bot,
                                const char *bot_username);

#endif /* HERMES_TELEGRAM_FILTER_H */
