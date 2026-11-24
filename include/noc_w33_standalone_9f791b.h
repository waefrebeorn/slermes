/*
 * noc_w33_standalone_9f791b.h - C surface for the value decisions of upstream
 * plugins/platforms/telegram/standalone.py (gate w33_standalone_9f791b).
 *
 * Ported: the media-caption split, the media-method routing by extension, the
 * photo-album eligibility, the topic-id -> message_thread_id mapping, the
 * "is this a topic name" refusal predicate, the thread-not-found predicate, the
 * Telegram-HTML tag probe, the non-Telegram markup escaper, the fallback-IP
 * resolution (opt-out + configured list + seed tail) and the bot-token
 * precedence (profile config, then the profile-scoped secret).
 *
 * Seams, unchanged in the caller: the Bot API client and every send/upload,
 * python-telegram-bot's request stack, os.environ and the filesystem
 * (os.path.exists for the album), strip_code (Markdown code regions),
 * format_message (MarkdownV2), redact_sensitive_text, request_may_have_delivered
 * and the profile-scoped secret store.  Env lookups arrive as fixture values so
 * the gate stays hermetic.
 */
#ifndef NOC_W33_STANDALONE_9F791B_H
#define NOC_W33_STANDALONE_9F791B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _media_caption_split: in {text: str|null, media: [[path, is_voice], ...]|null}
 * -> {caption: str|null, text: str|null}: the caption when exactly one
 * captionable, non-note file rides the bubble and the text fits 1024 chars. */
json_t *w33_standalone_9f791b_caption_split(const json_t *in);

/* _telegram_send_media kind: in {path, is_voice, force_document} ->
 * "photo"|"video"|"voice"|"audio"|"document" (first matching extension set). */
json_t *w33_standalone_9f791b_media_kind(const json_t *in);

/* _telegram_album_paths shape gate: in {media, force_document, caption} ->
 * bool: 2+ still images (no gif), no caption, nothing forced to a document.
 * os.path.exists/abspath stay in the caller. */
json_t *w33_standalone_9f791b_album_shape(const json_t *in);

/* _telegram_thread_kwargs: in {thread_id: str|int|null} -> {}
 * or {"message_thread_id": <int>}; forum "General" (1) maps to no thread. */
json_t *w33_standalone_9f791b_thread_kwargs(const json_t *in);

/* _send_telegram's topic-name refusal: in {thread_id: any} -> bool: a
 * non-empty thread id that is not `-?digits` cannot be resolved by a direct
 * send. */
json_t *w33_standalone_9f791b_thread_is_name(const json_t *in);

/* _is_telegram_thread_not_found: in {error: str} -> bool. */
json_t *w33_standalone_9f791b_thread_not_found(const json_t *in);

/* _telegram_format's HTML branch probe: in {text} -> bool: the text holds one
 * of Telegram's HTML tags (outside Markdown code, which stays the caller's
 * strip_code seam). */
json_t *w33_standalone_9f791b_html_tag_present(const json_t *in);

/* _escape_non_telegram_markup: in {text} -> str with every &, <, > outside a
 * supported tag (and outside an existing entity) escaped, tags kept verbatim. */
json_t *w33_standalone_9f791b_escape_markup(const json_t *in);

/* _telegram_fallback_ips: in {configured, env: {name: value}} -> [ip, ...]:
 * [] when HERMES_TELEGRAM_DISABLE_FALLBACK_IPS opts out, else the normalized
 * configured IPv4s, else the seed list. */
json_t *w33_standalone_9f791b_fallback_ips(const json_t *in);

/* standalone_send's token precedence: in {config_token, scoped_token} -> str:
 * the profile config's token, else the profile-scoped TELEGRAM_BOT_TOKEN,
 * else "". */
json_t *w33_standalone_9f791b_resolve_token(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
