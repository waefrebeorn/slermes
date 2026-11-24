/*
 * noc_w27_adapter_42b735_p4.h - C surface for slice 4 of upstream
 * plugins/platforms/whatsapp/adapter.py (gate w27_adapter_42b735_p4):
 * outbound shaping + the bridge-facing constant tables.
 *
 * _bridge_media_type (the _WA_EXT_MEDIA_TYPE extension table),
 * _normalize_outbound_mentions (to_whatsapp_jid/normalize_whatsapp_mention_jid
 * inlined + order-preserving dedupe) and one constants view over the module's
 * tables.  Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied to
 * the keys named after its parameters.  See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_42B735_P4_H
#define NOC_W27_ADAPTER_42B735_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req.file_path, req.is_voice, req.force_document ->
 * "image" | "video" | "audio" | "document". */
json_t *w27_adapter_42b735_p4_bridge_media_type(const json_t *req);

/* req.mentions ([any] or null) -> [valid participant JID] deduped, order
 * preserved ("6012" -> "6012@s.whatsapp.net", "1:2@lid" -> "1@lid"). */
json_t *w27_adapter_42b735_p4_normalize_outbound_mentions(const json_t *req);

/* The module's bridge-facing constants and tables as one object. */
json_t *w27_adapter_42b735_p4_constants(void);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_42b735_p4_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_42B735_P4_H */
