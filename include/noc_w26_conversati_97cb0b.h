/*
 * noc_w26_conversati_97cb0b.h - C surface for upstream
 * agent/conversation_compression_image_shrink.py (gate w26_conversati_97cb0b).
 *
 * The image-shrink recovery as values: data-URL/mime handling, the Anthropic
 * base64 source <-> data URL conversion, PNG pixel probing, and the accept
 * gate that decides whether a re-encoded image may replace the original.  The
 * decoder (Pillow) and the resizer are seams: the caller injects what they
 * returned, and this file decides what it means.
 */
#ifndef NOC_W26_CONVERSATI_97CB0B_H
#define NOC_W26_CONVERSATI_97CB0B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 4 MB leaves headroom under Anthropic's 5 MB. */
#define W26_CONVERSATI_97CB0B_SHRINK_TARGET_BYTES (4 * 1024 * 1024)
#define W26_CONVERSATI_97CB0B_DEFAULT_MAX_DIMENSION 8000

/* req: {"header": str, "default": str|null} -> image mime from a data: URL,
 * or the default when the header carries none. */
json_t *w26_conversati_97cb0b_data_url_mime(const json_t *req);

/* req: {"source": <any>} -> the data URL for a base64 image source, or null. */
json_t *w26_conversati_97cb0b_source_to_data_url(const json_t *req);

/* req: {"source": {...}, "data_url": str} -> a NEW source carrying the
 * re-encoded payload (copy-on-write). */
json_t *w26_conversati_97cb0b_write_source(const json_t *req);

/* req: {"data_url": str} -> [width, height] for a PNG data URL, else null. */
json_t *w26_conversati_97cb0b_decode_pixels(const json_t *req);

/* req: {"url": <any>, "max_dimension": int, "resized": str|null} ->
 * [resized_url|null, unshrinkable].  "resized" is what the resizer returned
 * for this payload (the resize_fn seam). */
json_t *w26_conversati_97cb0b_shrink(const json_t *req);

/* The byte budget and the mime -> suffix table, as an object. */
json_t *w26_conversati_97cb0b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
