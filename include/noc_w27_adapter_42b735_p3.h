/*
 * noc_w27_adapter_42b735_p3.h - C surface for slice 3 of upstream
 * plugins/platforms/whatsapp/adapter.py (gate w27_adapter_42b735_p3):
 * inbound message shaping.
 *
 * _classify_bridge_message, _may_transcribe_at_gate, _inject_document_text
 * and the _log_delivery_status log line.  Each symbol takes the JSON-lines
 * request object and returns the Python-equivalent JSON value of the
 * same-named upstream function applied to the keys named after its
 * parameters.  See adapter.c for the PoP map; the media download/cache and
 * the logger sink stay in the caller (document bodies arrive as
 * {name, content, size} triples).
 */
#ifndef NOC_W27_ADAPTER_42B735_P3_H
#define NOC_W27_ADAPTER_42B735_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req.data ({mediaType, hasMedia, ...}) -> MessageType value string
 * ("text" | "photo" | "video" | "audio" | "voice" | "document" |
 * "sticker" | "location"). */
json_t *w27_adapter_42b735_p3_classify_bridge_message(const json_t *req);

/* req.data, req.extra (adapter config.extra) -> bool: group ptt + the
 * voice_mention_patterns opt-in. */
json_t *w27_adapter_42b735_p3_may_transcribe_at_gate(const json_t *req);

/* req.docs ([{name, content, size?}]), req.body ->
 * [body_after_injection, [inlined_flags]] (one flag per doc). */
json_t *w27_adapter_42b735_p3_inject_document_text(const json_t *req);

/* req.data, req.name (adapter display name) -> the exact logger.info text of
 * one delivery-status line. */
json_t *w27_adapter_42b735_p3_delivery_status_line(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_42b735_p3_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_42B735_P3_H */
