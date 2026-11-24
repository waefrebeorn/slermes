#ifndef NOC_W20_CONVERSATI_6C3404_H
#define NOC_W20_CONVERSATI_6C3404_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/conversation_compression_watermark.py (gate w20_conversati_6c3404).
 *
 * The held-row watermark cap and its lease-less staleness gate. The store reads
 * (`get_message_role`, `rewritten_held_row_ids`) are supplied as a scripted
 * stub, so the whole decision ports.
 */

/* PoP: w20_conversati_6c3404_held_archive_watermark @ agent/conversation_compression_watermark.py:held_archive_watermark
 * `watermark` is a decimal string (int64) or null; `messages` / `verbatim_tail`
 * are arrays of message dicts. `db` is the scripted session-db stub:
 *   {"roles": {"<row id>": "<role>"},   get_message_role(session_id, rid)
 *    "rewritten": [ids] | null}         rewritten_held_row_ids(session_id, exact)
 * `persistence` (when non-zero) is read from db["persistence"] instead, the
 * same {"roles","rewritten"} shape behind a messages interface.
 * Answer: {"status":"ok","value":n|null} |
 *         {"status":"raised","type":"StaleHeldHistory","message":"..."}. */
json_t *w20_conversati_6c3404_held_archive_watermark(const char *session_id,
                                                     const json_t *watermark,
                                                     const json_t *messages,
                                                     const json_t *verbatim_tail,
                                                     int stale_raises,
                                                     int persistence,
                                                     const json_t *db);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CONVERSATI_6C3404_H */
