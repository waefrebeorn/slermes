/*
 * noc_w31_adapter_c93276_p2.h - C surface for the OUTBOUND/CONFIG half of
 * upstream plugins/platforms/simplex/adapter.py (gate w31_adapter_c93276_p2).
 *
 * The daemon, the WebSocket and the filesystem are seams; every function here
 * decides from values only: how a chat id is addressed, which MEDIA: tags a
 * reply body carries, what item payload a send builds, what corrId/backoff a
 * reconnect mints, and what the env/extra pair configures.
 */
#ifndef NOC_W31_ADAPTER_C93276_P2_H
#define NOC_W31_ADAPTER_C93276_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _send_cmd addressing: "group:42" -> "#42", anything else -> "@<id>".
 * -> {"target": str, "items": <items passed through>}  (the json.dumps join is
 * the serializer seam; target addressing is the decision). */
json_t *w31_adapter_c93276_p2_send_cmd(const char *chat_id, const json_t *items);

/* SimplexAdapter.send: r"MEDIA:(\S+)" findall, then re.sub + strip.
 * -> {"media": [..], "text": str} */
json_t *w31_adapter_c93276_p2_split_media(const char *content);

/* _VOICE_TAG_EXTS membership (note: narrower than _AUDIO_EXTS). */
json_t *w31_adapter_c93276_p2_is_voice_ext(const char *ext);

/* send_image / send_document / send_voice item shaping.
 * kind in {"image","document","voice"}; path, caption, duration, thumb. */
json_t *w31_adapter_c93276_p2_send_item(const char *kind, const char *path,
                                        const json_t *caption, const json_t *duration,
                                        const char *thumb);

/* _make_corr_id(counter, now_ms) / the standalone "snd" corrId (counter null). */
json_t *w31_adapter_c93276_p2_corr_id(const json_t *counter, const json_t *now_ms);

/* _ws_listener backoff step: -> {"next": min(backoff*2, 60), "sleep": backoff
 * + backoff*0.2*rnd} */
json_t *w31_adapter_c93276_p2_retry_math(const json_t *backoff, const json_t *rnd);

/* _health_monitor idleness predicate: elapsed > HEALTH_CHECK_STALE_THRESHOLD. */
json_t *w31_adapter_c93276_p2_health_stale(const json_t *elapsed);

/* SimplexAdapter.__init__ config: env + extra (both seams) -> {
 * "ws_url", "auto_accept", "group_allowed" (first-sighting order, membership
 * only), "batch_delay", "split_delay"} */
json_t *w31_adapter_c93276_p2_init_config(const json_t *env, const json_t *extra);

/* check_requirements / validate_config / is_connected.
 * websockets is the import seam; env/extra are the scoped-read seam. */
json_t *w31_adapter_c93276_p2_config_gate(const json_t *env, const json_t *extra,
                                          const json_t *websockets);

/* _env_enablement: null when SIMPLEX_WS_URL is blank, else the seed object. */
json_t *w31_adapter_c93276_p2_env_enablement(const json_t *env);

/* get_chat_info(chat_id). */
json_t *w31_adapter_c93276_p2_get_chat_info(const char *chat_id);

/* list_channels shaping of the /contacts and /groups replies; null when the
 * /contacts send failed (the caller then keeps its known targets). */
json_t *w31_adapter_c93276_p2_list_channels(const json_t *contacts_resp,
                                            const json_t *groups_resp);

/* _prepare_image's pure half: needs_png + the suffix it keys on. */
json_t *w31_adapter_c93276_p2_prepare_image(const char *file_path);

/* _text_batch_key: "<platform>:<chat_id>". */
json_t *w31_adapter_c93276_p2_text_batch_key(const char *platform, const char *chat_id);

#ifdef __cplusplus
}
#endif
#endif
