/*
 * neutts_synth.c — real C11 port of upstream Hermes tools/neutts_synth.py.
 *
 * PoP: neuttsynth_pcm @ tools/neutts_synth.py:_write_wav
 * PoP: neuttsynth_wav @ tools/neutts_synth.py:_write_wav
 *
 * The live helper's only deterministic computation is its WAV encoder. The
 * NumPy pipeline is reproduced with C float (IEEE-754 binary32) arithmetic:
 *   float32(round-trip) -> clip[-1,1] -> *32767 (float32) -> int16 truncation.
 * argparse / NeuTTS model loading / soundfile are runtime concerns, not ported.
 */
#include "noc_w6_neuttsynth_668f10.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (np.clip(samples.flatten(), -1, 1) * 32767).astype(np.int16) */
static int16_t *pcm_samples(const json_t *samples, size_t *count) {
    size_t n = (samples && samples->type == JSON_ARRAY) ? json_len(samples) : 0;
    int16_t *pcm = (int16_t *)malloc((n ? n : 1) * sizeof(int16_t));
    if (!pcm) { *count = 0; return NULL; }
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(samples, i);
        double raw = (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
        float f = (float)raw;                 /* np.array(..., dtype=float32) */
        if (f < -1.0f) f = -1.0f;
        if (f > 1.0f) f = 1.0f;               /* np.clip */
        f = f * 32767.0f;                     /* float32 multiply by 32767 */
        pcm[i] = (int16_t)f;                  /* astype(int16): truncate toward zero */
    }
    *count = n;
    return pcm;
}

json_t *neuttsynth_pcm(const json_t *samples) {
    size_t n, i;
    int16_t *pcm = pcm_samples(samples, &n);
    json_t *out = json_array();
    for (i = 0; i < n; i++) json_append(out, json_number((double)pcm[i]));
    free(pcm);
    return out;
}

static void put_u32(unsigned char *b, size_t *k, uint32_t v) {
    b[(*k)++] = (unsigned char)(v & 255);
    b[(*k)++] = (unsigned char)((v >> 8) & 255);
    b[(*k)++] = (unsigned char)((v >> 16) & 255);
    b[(*k)++] = (unsigned char)((v >> 24) & 255);
}
static void put_u16(unsigned char *b, size_t *k, uint16_t v) {
    b[(*k)++] = (unsigned char)(v & 255);
    b[(*k)++] = (unsigned char)((v >> 8) & 255);
}

json_t *neuttsynth_wav(const json_t *samples, const char *sample_rate) {
    size_t n, i, k = 0, total;
    int16_t *pcm = pcm_samples(samples, &n);
    long sr = strtol(sample_rate ? sample_rate : "24000", NULL, 10);
    uint32_t data_size = (uint32_t)(n * 2);
    unsigned char *buf;
    json_t *out = json_array();

    total = 44 + data_size;
    buf = (unsigned char *)malloc(total ? total : 44);
    if (!buf) { free(pcm); return out; }

    memcpy(buf + k, "RIFF", 4); k += 4;
    put_u32(buf, &k, (uint32_t)(36 + data_size));
    memcpy(buf + k, "WAVEfmt ", 8); k += 8;
    put_u32(buf, &k, 16);                              /* fmt chunk size */
    put_u16(buf, &k, 1);                              /* PCM */
    put_u16(buf, &k, 1);                              /* mono */
    put_u32(buf, &k, (uint32_t)sr);                    /* sample rate */
    put_u32(buf, &k, (uint32_t)(sr * 2));              /* byte rate */
    put_u16(buf, &k, 2);                              /* block align */
    put_u16(buf, &k, 16);                             /* bits per sample */
    memcpy(buf + k, "data", 4); k += 4;
    put_u32(buf, &k, data_size);
    for (i = 0; i < n; i++) put_u16(buf, &k, (uint16_t)pcm[i]);

    for (i = 0; i < k; i++) json_append(out, json_number((double)buf[i]));
    free(buf);
    free(pcm);
    return out;
}
