#ifndef NOC_W9_TTSTOOLSPE_4E393D_H
#define NOC_W9_TTSTOOLSPE_4E393D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure byte-chunk helpers of tools/tts_tool_speaker.py.
 * Every symbol is prefixed with the gate id. JSON byte chunks are arrays of
 * integers in 0..255 (never raw strings) so null/high bytes survive a round trip. */

/* PoP: w9_ttstoolspe_4e393d_align_int16_chunks @ tools/tts_tool_speaker.py:_align_int16_chunks
 * chunks: JSON array of byte arrays. stop_after: -1 = never stop, otherwise the number
 *   of chunks processed before the synthetic stop event trips (mirrors Ev.is_set()).
 * pad_tail mirrors the keyword-only pad_tail argument.
 * Returns a JSON array of aligned byte arrays. Caller frees. */
json_t *w9_ttstoolspe_4e393d_align_int16_chunks(const json_t *chunks, int stop_after, int pad_tail);

/* PoP: w9_ttstoolspe_4e393d_drain_chunks @ tools/tts_tool_speaker.py:_drain_chunks
 * items: JSON array of (byte array | null sentinel). Returns the byte arrays up to
 * the first null sentinel. Caller frees. */
json_t *w9_ttstoolspe_4e393d_drain_chunks(const json_t *items);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_TTSTOOLSPE_4E393D_H */
