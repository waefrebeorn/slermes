/*
 * noc_w31_adapter_c93276_p1.h - C surface for the INBOUND half of upstream
 * plugins/platforms/simplex/adapter.py (gate w31_adapter_c93276_p1).
 *
 * Everything here is a pure function of the JSON values the caller passes in.
 * The WebSocket, the simplex-chat daemon, the filesystem and per-profile authz
 * are seams: a fixture stands in for each of them, so what is compared is only
 * what the values decide (who sent what, which chat, which media kind, which
 * text, which timestamp, may the file be fetched).
 */
#ifndef NOC_W31_ADAPTER_C93276_P1_H
#define NOC_W31_ADAPTER_C93276_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "a, b ,, c" -> ["a","b","c"]  (SimplexAdapter._parse_comma_list) */
json_t *w31_adapter_c93276_p1_parse_comma_list(const char *value);

/* "1234567890" -> "12**90", "abc" -> "abc", ""/null -> "<none>" (_redact_id) */
json_t *w31_adapter_c93276_p1_redact_id(const json_t *value);

/* pathlib suffix, both separators honoured, lowercased (_handle_chat_item ext) */
json_t *w31_adapter_c93276_p1_path_suffix(const char *path);

/* _is_image_ext / _is_audio_ext / _mime_for_ext */
json_t *w31_adapter_c93276_p1_is_image_ext(const char *ext);
json_t *w31_adapter_c93276_p1_is_audio_ext(const char *ext);
json_t *w31_adapter_c93276_p1_mime_for_ext(const char *ext);

/* _display_name(obj, profile_key, default) -> localDisplayName or
 * obj[profile_key]["displayName"] or default. */
json_t *w31_adapter_c93276_p1_display_name(const json_t *obj, const char *profile_key,
                                           const json_t *def);

/* SimplexAdapter._may_receive_file.  policy = {"item": <chat item>,
 * "group_allow_from": [..], "authorized_ids": [..]} where authorized_ids is the
 * authz seam (_is_sender_authorized reduced to "this sender id is authorized
 * for this chat").  -> bool */
json_t *w31_adapter_c93276_p1_may_receive_file(const json_t *policy);

/* datetime.fromisoformat(ts).isoformat(), or null when it would raise
 * (the truth then substitutes "now", which is caller state, not a value). */
json_t *w31_adapter_c93276_p1_ts_iso(const char *ts);

/* SimplexAdapter._handle_chat_item decision record.
 * policy = {"item": {"chatInfo": .., "chatItem": ..},
 *           "group_allow_from": [..], "authorized_ids": [..]}
 * -> {"action": "drop"|"defer"|"batch"|"handle",
 *     "text": str|null, "message_type": "text"|"photo"|"voice"|"document"|null,
 *     "media_urls": [..]|null, "media_types": [..]|null,
 *     "chat_id"/"chat_name"/"chat_type"/"user_id"/"user_name": str|null,
 *     "file_id": any|null, "command": str|null, "ts": str|null}
 * build_source is the seam: the record carries the arguments it would receive. */
json_t *w31_adapter_c93276_p1_chat_item(const json_t *policy);

#ifdef __cplusplus
}
#endif
#endif
