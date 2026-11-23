#ifndef NOC_W6_NEUTTSYNTH_668F10_H
#define NOC_W6_NEUTTSYNTH_668F10_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: neuttsynth_pcm @ tools/neutts_synth.py:_write_wav
 * The int16 PCM samples produced by
 *   (np.clip(samples.flatten(), -1.0, 1.0) * 32767).astype(np.int16)
 * with float32 intermediate arithmetic. Returns a JSON array of ints. */
json_t *neuttsynth_pcm(const json_t *samples);

/* PoP: neuttsynth_wav @ tools/neutts_synth.py:_write_wav
 * The complete little-endian WAV byte stream the live helper writes:
 * RIFF header + fmt chunk + data chunk + 16-bit mono PCM. Returns a JSON
 * array of byte values (0..255). `sample_rate` is a decimal string. */
json_t *neuttsynth_wav(const json_t *samples, const char *sample_rate);

#ifdef __cplusplus
}
#endif
#endif
