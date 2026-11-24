#ifndef NOC_W18_TRANSCRIPTI_9F815D_H
#define NOC_W18_TRANSCRIPTI_9F815D_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/transcription_streaming.py.
 * resample(): apply a stateful s16le mono Resampler over the ordered hex chunks
 *             and return the concatenated output bytes as a hex string (caller frees).
 * error():    StreamingSession._error(message) envelope. */
char   *w18_transcripti_9f815d_resample(int src_rate, int dst_rate, const json_t *chunks_hex);
json_t *w18_transcripti_9f815d_error(const char *provider, const char *message);

#endif /* NOC_W18_TRANSCRIPTI_9F815D_H */
