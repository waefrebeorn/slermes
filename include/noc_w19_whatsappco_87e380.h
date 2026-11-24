/* noc_w19_whatsappco_87e380.h - public C API for gateway/platforms/whatsapp_common.py port. */
#ifndef NOC_W19_WHATSAPPCO_87E380_H
#define NOC_W19_WHATSAPPCO_87E380_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_whatsappco_87e380_header_to_bold @ gateway/platforms/whatsapp_common.py:_header_to_bold
 * `# Header` -> `*Header*`, stripping already-bolded `*...*` from the captured group. */
json_t *w19_whatsappco_87e380_header_to_bold(const char *inner);

/* PoP: w19_whatsappco_87e380_sanitize_outbound_text @ gateway/platforms/whatsapp_common.py:_sanitize_outbound_text
 * Strip zero-width format chars (U+200B/U+2060/U+2063/U+FEFF) and normalize odd unicode
 * spaces to one ASCII space; emoji joiners are kept. Empty/absent content is returned as-is. */
json_t *w19_whatsappco_87e380_sanitize_outbound_text(const char *content);

/* PoP: w19_whatsappco_87e380_effective_reply_prefix @ gateway/platforms/whatsapp_common.py:_effective_reply_prefix
 * Prefix for replies in self-chat mode: "" outside WHATSAPP_MODE=self-chat; the configured
 * prefix, else the scoped env prefix, else DEFAULT_REPLY_PREFIX. A NULL argument is an
 * unset carrier; literal "\n" becomes a newline. */
json_t *w19_whatsappco_87e380_effective_reply_prefix(const char *mode,
                                                    const char *reply_prefix,
                                                    const char *env_prefix);

/* PoP: w19_whatsappco_87e380_outgoing_chunk_limit @ gateway/platforms/whatsapp_common.py:_outgoing_chunk_limit
 * max(1024, MAX_MESSAGE_LENGTH - len(prefix)); `prefix_len` is the prefix's character count. */
json_t *w19_whatsappco_87e380_outgoing_chunk_limit(int prefix_len);

/* PoP: w19_whatsappco_87e380_normalize_whatsapp_id @ gateway/platforms/whatsapp_common.py:_normalize_whatsapp_id
 * Stripped id with the device qualifier dropped: `<user>:<device>@lid` -> `<user>@lid`. */
json_t *w19_whatsappco_87e380_normalize_whatsapp_id(const char *value);

/* PoP: w19_whatsappco_87e380_is_broadcast_chat @ gateway/platforms/whatsapp_common.py:_is_broadcast_chat
 * True for status@broadcast and any @broadcast / @newsletter chat. */
int w19_whatsappco_87e380_is_broadcast_chat(const char *chat_id);

/* PoP: w19_whatsappco_87e380_coerce_allow_list @ gateway/platforms/whatsapp_common.py:_coerce_allow_list
 * list / JSON-list string / CSV -> sorted set of stripped non-empty strings. */
json_t *w19_whatsappco_87e380_coerce_allow_list(json_t *raw);

/* PoP: w19_whatsappco_87e380_select_allowlist @ gateway/platforms/whatsapp_common.py:_select_allowlist
 * (source, raw) by key presence: a config key wins, then the first truthy env carrier;
 * {"source":str|null,"raw":value|null} when neither is set. */
json_t *w19_whatsappco_87e380_select_allowlist(json_t *extra, json_t *config_keys,
                                               json_t *env_keys, json_t *env);

/* PoP: w19_whatsappco_87e380_bot_ids_from_message @ gateway/platforms/whatsapp_common.py:_bot_ids_from_message
 * Normalized non-empty bot ids from data["botIds"], sorted (set order -> sorted output). */
json_t *w19_whatsappco_87e380_bot_ids_from_message(json_t *data);

/* PoP: w19_whatsappco_87e380_message_is_reply_to_bot @ gateway/platforms/whatsapp_common.py:_message_is_reply_to_bot
 * True when data["quotedParticipant"] names one of the message's bot ids. */
int w19_whatsappco_87e380_message_is_reply_to_bot(json_t *data);

/* PoP: w19_whatsappco_87e380_message_mentions_bot @ gateway/platforms/whatsapp_common.py:_message_mentions_bot
 * True via mentionedIds or a bare `@<id>` in the lowered body. */
int w19_whatsappco_87e380_message_mentions_bot(json_t *data);

/* PoP: w19_whatsappco_87e380_clean_bot_mention_text @ gateway/platforms/whatsapp_common.py:_clean_bot_mention_text
 * Remove each bot's `@<bare>[,:\-]*\s*` mention; falls back to the original when the
 * cleaned text strips to empty. */
json_t *w19_whatsappco_87e380_clean_bot_mention_text(const char *text, json_t *data);

/* PoP: w19_whatsappco_87e380_format_message @ gateway/platforms/whatsapp_common.py:format_message
 * Markdown -> WhatsApp (*bold*, _italic_, ~strike~, headers, links), fenced and inline code
 * protected by placeholder substitution in the source's exact rule order. */
json_t *w19_whatsappco_87e380_format_message(const char *content);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_WHATSAPPCO_87E380_H */
