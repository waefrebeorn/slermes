/*
 * noc_w27_adapter_74f618_p3.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p3).
 *
 * Gate 3: text-vs-blocks dedupe normalization, the redacted Block Kit
 * payload dump, and the opt-in URL harvest.
 */
#ifndef NOC_W27_ADAPTER_74F618_P3_H
#define NOC_W27_ADAPTER_74F618_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"text": str|null} -> str (&amp;/&lt;/&gt; undone) */
json_t *w27_p3_unescape_entities(const json_t *req);

/* req: {"text": str|null, "bot_uid": str?} -> str */
json_t *w27_p3_normalize_dedupe(const json_t *req);

/* req: {"blocks": [..]|null, "primary_text": str?, "bot_uid": str?} -> str */
json_t *w27_p3_additional_text(const json_t *req);

/* req: {"blocks": [..]|null, "max_chars": int?} -> str */
json_t *w27_p3_serialize_blocks(const json_t *req);

/* req: {"blocks": [..]|null} -> [str] */
json_t *w27_p3_extract_urls(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P3_H */
