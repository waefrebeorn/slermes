/*
 * noc_w24_telegramid_756e5b.h - C surface for upstream
 * plugins/platforms/telegram/telegram_ids.py (gate w24_telegramid_756e5b).
 *
 * The Bot API chat-identifier layer: a ``chat_id`` is a numeric id (int, negative
 * for channels) or an ``@username`` string, so ``int(chat_id)`` is never safe.
 * Everything here is a pure value->value decision over that id text: normalize,
 * recognize a username shape, parse ``<chat>[:<topic>]`` send targets, and
 * recover a positive message id from an opaque reply anchor.
 *
 * Seams: ``parse_send_target`` is typed ``str`` by the send_message contract, so
 * non-string refs are not passed in (a JSON string is required).  Entity/id
 * values are taken as their Python ``str()``, which is what upstream does.
 */
#ifndef W24_TELEGRAMID_756E5B_H
#define W24_TELEGRAMID_756E5B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Telegram usernames: "@" + 4..32 [A-Za-z0-9_] (4-char legacy handles tolerated). */
#define W24_TELEGRAMID_756E5B_USERNAME_MIN 4
#define W24_TELEGRAMID_756E5B_USERNAME_MAX 32

/* any value -> int when str(value).strip() is an int literal, else that stripped
 * string; never an error. */
json_t *w24_telegramid_756e5b_normalize_chat_id(const json_t *value);

/* any value -> true when str(value).strip() fullmatches @[A-Za-z0-9_]{4,32}. */
json_t *w24_telegramid_756e5b_looks_like_username(const json_t *value);

/* any value -> the stripped value when it is an @username target, else null. */
json_t *w24_telegramid_756e5b_parse_username_target(const json_t *value);

/* ref (string) -> [chat_id, topic_id|null] as strings, or null when nothing
 * explicit was parsed ("<chat>[:<topic>]", "<id>:<named dm topic>", "@username"). */
json_t *w24_telegramid_756e5b_parse_send_target(const json_t *ref);

/* any value -> positive int message id, or null (bools are never ids). */
json_t *w24_telegramid_756e5b_parse_message_id(const json_t *value);

/* The literal patterns/bounds the module is written against. */
json_t *w24_telegramid_756e5b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
