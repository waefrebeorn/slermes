/* noc_w19_adapter_5d2c00.h - public C API for plugins/platforms/qqbot/adapter.py. */
#ifndef NOC_W19_ADAPTER_5D2C00_H
#define NOC_W19_ADAPTER_5D2C00_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_adapter_5d2c00_constants @ plugins/platforms/qqbot/adapter.py:<module>
 * The adapter vocabulary: message types, media types, voice extensions, STT base URLs. */
json_t *w19_adapter_5d2c00_constants(void);

/* PoP: w19_adapter_5d2c00_ws_proxy @ plugins/platforms/qqbot/adapter.py:_ws_proxy
 * First truthy of WSS_PROXY/wss_proxy/HTTPS_PROXY/https_proxy/ALL_PROXY/all_proxy. */
json_t *w19_adapter_5d2c00_ws_proxy(json_t *env);

/* PoP: w19_adapter_5d2c00_parse_json @ plugins/platforms/qqbot/adapter.py:QQAdapter._parse_json */
json_t *w19_adapter_5d2c00_parse_json(const char *raw);
/* PoP: w19_adapter_5d2c00_parse_gateway_session_key @ plugins/platforms/qqbot/adapter.py:QQAdapter._parse_gateway_session_key */
json_t *w19_adapter_5d2c00_parse_gateway_session_key(const char *session_key);
/* PoP: w19_adapter_5d2c00_is_authorized_interaction @ plugins/platforms/qqbot/adapter.py:QQAdapter._is_authorized_interaction_for_session */
json_t *w19_adapter_5d2c00_is_authorized_interaction(json_t *event, const char *session_key);
/* PoP: w19_adapter_5d2c00_append_block @ plugins/platforms/qqbot/adapter.py:QQAdapter._append_block */
json_t *w19_adapter_5d2c00_append_block(const char *text, const char *block);
/* PoP: w19_adapter_5d2c00_merge_quote_into @ plugins/platforms/qqbot/adapter.py:QQAdapter._merge_quote_into */
json_t *w19_adapter_5d2c00_merge_quote_into(const char *text, const char *quote_block);
/* PoP: w19_adapter_5d2c00_detect_message_type @ plugins/platforms/qqbot/adapter.py:QQAdapter._detect_message_type */
json_t *w19_adapter_5d2c00_detect_message_type(json_t *media_urls, json_t *media_types);
/* PoP: w19_adapter_5d2c00_opt_str @ plugins/platforms/qqbot/adapter.py:QQAdapter._opt_str */
json_t *w19_adapter_5d2c00_opt_str(json_t *value);
/* PoP: w19_adapter_5d2c00_is_voice_content_type @ plugins/platforms/qqbot/adapter.py:QQAdapter._is_voice_content_type */
json_t *w19_adapter_5d2c00_is_voice_content_type(const char *content_type, const char *filename);
/* PoP: w19_adapter_5d2c00_guess_ext_from_data @ plugins/platforms/qqbot/adapter.py:QQAdapter._guess_ext_from_data
 * The payload is the file's leading bytes, hex-encoded. */
json_t *w19_adapter_5d2c00_guess_ext_from_data(const char *hex);
/* PoP: w19_adapter_5d2c00_looks_like_silk @ plugins/platforms/qqbot/adapter.py:QQAdapter._looks_like_silk */
json_t *w19_adapter_5d2c00_looks_like_silk(const char *hex);
/* PoP: w19_adapter_5d2c00_rest_path @ plugins/platforms/qqbot/adapter.py:QQAdapter._rest_path */
json_t *w19_adapter_5d2c00_rest_path(const char *chat_type, const char *target_id,
                                     const char *endpoint);
/* PoP: w19_adapter_5d2c00_messages_path @ plugins/platforms/qqbot/adapter.py:QQAdapter._messages_path */
json_t *w19_adapter_5d2c00_messages_path(const char *chat_type, const char *target_id);
/* PoP: w19_adapter_5d2c00_build_text_body @ plugins/platforms/qqbot/adapter.py:QQAdapter._build_text_body
 * msg_seq is _next_msg_seq (wall clock + uuid), so it is injected here. */
json_t *w19_adapter_5d2c00_build_text_body(const char *content, const char *reply_to, int markdown,
                                           long max_len, long msg_seq);
/* PoP: w19_adapter_5d2c00_is_url @ plugins/platforms/qqbot/adapter.py:QQAdapter._is_url */
json_t *w19_adapter_5d2c00_is_url(const char *source);
/* PoP: w19_adapter_5d2c00_strip_at_mention @ plugins/platforms/qqbot/adapter.py:QQAdapter._strip_at_mention */
json_t *w19_adapter_5d2c00_strip_at_mention(const char *content);
/* PoP: w19_adapter_5d2c00_entry_matches @ plugins/platforms/qqbot/adapter.py:QQAdapter._entry_matches */
json_t *w19_adapter_5d2c00_entry_matches(json_t *entries, const char *target);
/* PoP: w19_adapter_5d2c00_guess_chat_type @ plugins/platforms/qqbot/adapter.py:QQAdapter._guess_chat_type */
json_t *w19_adapter_5d2c00_guess_chat_type(json_t *chat_type_map, const char *chat_id);
/* PoP: w19_adapter_5d2c00_parse_qq_timestamp @ plugins/platforms/qqbot/adapter.py:QQAdapter._parse_qq_timestamp
 * {"iso": <str|null>, "is_now": bool} - the fallback reads the clock, so it is named, not taken. */
json_t *w19_adapter_5d2c00_parse_qq_timestamp(const char *raw);
/* PoP: w19_adapter_5d2c00_resolve_stt_config @ plugins/platforms/qqbot/adapter.py:QQAdapter._resolve_stt_config
 * extra is the platform config's extra map; env supplies QQ_STT_API_KEY/BASE_URL/MODEL. */
json_t *w19_adapter_5d2c00_resolve_stt_config(json_t *extra, json_t *env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ADAPTER_5D2C00_H */
