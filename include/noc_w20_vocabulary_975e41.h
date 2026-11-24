#ifndef NOC_W20_VOCABULARY_975E41_H
#define NOC_W20_VOCABULARY_975E41_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/workers/vocabulary.py (gate w20_vocabulary_975e41).
 *
 * The worker family's shared vocabulary: the execution-id separator, the
 * message fields the worker wire carries, the row annotation keys, the sidecar
 * and constructor key sets and the receipt bounds.  Everything here is a pure
 * table or a pure shaper, so the whole module is portable.
 */

/* PoP: w20_vocabulary_975e41_fields @ persistence/workers/vocabulary.py:WORKER_MESSAGE_FIELDS
 * One of the four frozensets as a sorted array of strings (Python's `sorted()`
 * order, i.e. code-point order), or null for an unknown name.
 *   "worker_message_fields"        -> WORKER_MESSAGE_FIELDS
 *   "handoff_message_fields"       -> HANDOFF_MESSAGE_FIELDS
 *   "sidecar_keys"                 -> SIDECAR_KEYS
 *   "constructor_config_keys"      -> WORKER_CONSTRUCTOR_CONFIG_KEYS
 *   "row_annotation_keys"          -> ROW_ANNOTATION_KEYS (a tuple, same order
 *                                     as the tuple, not sorted: see constants). */
json_t *w20_vocabulary_975e41_fields(const char *which);

/* PoP: w20_vocabulary_975e41_contains @ persistence/workers/vocabulary.py:WORKER_MESSAGE_FIELDS
 * `key in <set>` for the same four set names plus "row_annotation_keys".
 * An unknown set name or a NULL key is false (never a crash). */
int w20_vocabulary_975e41_contains(const char *which, const char *key);

/* PoP: w20_vocabulary_975e41_constants @ persistence/workers/vocabulary.py:CHILD_SEPARATOR
 * {"child_separator": str, "receipt_payload_max_bytes": "<int64 decimal>",
 *  "transcript_batch_max": "<int64 decimal>", "row_annotation_keys": [str...],
 *  "all": [__all__ in order]}.  The two bounds exceed JSON's double precision
 * guarantee, so they travel as decimal strings like every other 64-bit int. */
json_t *w20_vocabulary_975e41_constants(void);

/* PoP: w20_vocabulary_975e41_row_annotations @ persistence/workers/vocabulary.py:row_annotations
 * `[{key: message.get(key) for key in ROW_ANNOTATION_KEYS} for message in messages]`.
 * A non-array, or a non-object element, annotates as all-null (Python would
 * raise AttributeError there; the wire never produces it, and a gate crash is
 * never the better answer). */
json_t *w20_vocabulary_975e41_row_annotations(const json_t *messages);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_VOCABULARY_975E41_H */
