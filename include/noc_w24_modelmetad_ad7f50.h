/*
 * noc_w24_modelmetad_ad7f50.h - C surface for upstream
 * agent/model_metadata_token_memo.py (gate w24_modelmetad_ad7f50).
 *
 * The per-message token-estimate memo, ported as the values it decides: the
 * two bounds and the active window it is tuned with, the retained-bytes
 * estimate of one entry, the "too large to admit" test, the LRU insert/lookup
 * state machine with its two eviction bounds, and the drop-the-dead-entries
 * sweep keyed on the long strings a message tree holds.
 *
 * Two Python-only facts are seams a caller supplies: a pin's byte cost
 * (``sys.getsizeof``, measured by the object model) and a string's identity --
 * the sweep compares values here, so two equal strings are one leaf.
 */
#ifndef W24_MODELMETAD_AD7F50_H
#define W24_MODELMETAD_AD7F50_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Entry count, retained-bytes budget, per-leaf overhead and active window. */
#define W24_MODELMETAD_AD7F50_MAX_ENTRIES 4096
#define W24_MODELMETAD_AD7F50_MAX_BYTES 16777216.0
#define W24_MODELMETAD_AD7F50_LEAF_OVERHEAD 120
#define W24_MODELMETAD_AD7F50_ACTIVE_WINDOW_S 60.0
/* A string leaf is "long" (and so identifies a message) above this length. */
#define W24_MODELMETAD_AD7F50_LONG_STRING 64

/* {"pin_sizes": [<measured bytes>, ...]} -> sum(sizes) + 120 * len(sizes). */
json_t *w24_modelmetad_ad7f50_entry_bytes(const json_t *req);

/* {"size": <bytes>} -> size > the whole retained-bytes budget. */
json_t *w24_modelmetad_ad7f50_oversize(const json_t *req);

/* {"ops": [{"op": "remember", "key": <str>, "sizes": [...], "now": <float>},
 *          {"op": "lookup", "key": <str>, "now": <float>}, ...]} ->
 * {"keys": [<key>, ...] (least recent first), "retained": <bytes>}. */
json_t *w24_modelmetad_ad7f50_memo(const json_t *req);

/* {"texts": {<name>: <string>}, "memo": [{"key", "pins": [<name>, ...]}],
 *  "messages": <any>, "keep": <any>|null} ->
 * {"evicted": <count>, "keys": [...] } after forget_message_token_memo. */
json_t *w24_modelmetad_ad7f50_forget(const json_t *req);

/* {"max_entries", "max_bytes", "leaf_overhead", "active_window_s",
 *  "long_string"} as an object. */
json_t *w24_modelmetad_ad7f50_constants(void);

#ifdef __cplusplus
}
#endif
#endif
