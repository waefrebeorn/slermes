/*
 * c11_whisper_encoder.c — Audio encoder transformer + transcription orchestration
 *
 * Takes mel-spectrogram → encoder transformer → token decoder → text.
 * Pure C11. No third-party.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "c11_whisper.h"
#include "c11_whisper_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Audio loading: pipe ffmpeg → 16kHz mono f32 */
int c11_whisper_load_audio(const char *audio_path, float **samples, int *n_samples) {
    if (!audio_path || !samples || !n_samples) return -1;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -nostdin -threads 0 -i \"%s\" -f f32le -ar 16000 -ac 1 - 2>/dev/null",
             audio_path);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;

    const size_t chunk_size = 16000;
    float *buffer = NULL;
    size_t buffer_cap = 0;
    size_t total_samples = 0;
    float chunk[chunk_size];

    while (1) {
        size_t rd = fread(chunk, sizeof(float), chunk_size, pipe);
        if (rd == 0) break;

        if (total_samples + rd > buffer_cap) {
            buffer_cap = buffer_cap ? buffer_cap * 2 : 16000;
            while (total_samples + rd > buffer_cap) buffer_cap *= 2;
            float *new_buf = realloc(buffer, buffer_cap * sizeof(float));
            if (!new_buf) { free(buffer); pclose(pipe); return -1; }
            buffer = new_buf;
        }

        memcpy(buffer + total_samples, chunk, rd * sizeof(float));
        total_samples += rd;
    }

    pclose(pipe);

    if (total_samples == 0) { free(buffer); return -1; }

    *samples = buffer;
    *n_samples = (int)total_samples;
    return 0;
}

/* ── Full transcription ── */

int c11_whisper_full(c11_whisper_ctx_t *ctx,
                     const float *samples, int n_samples,
                     const char *language, int translate, int n_threads,
                     c11_whisper_result_t *result) {
    if (!ctx || !samples || n_samples <= 0 || !result) return -1;

    /* Step 1: Compute mel-spectrogram */
    c11_mel_spectrogram_t mel = {0};
    mel.n_mel = ctx->model.n_mel;
    mel.n_fft = 400;
    mel.hop_length = 160;
    mel.sample_rate = 16000;
    c11_mel_compute(&mel, samples, n_samples);

    if (!mel.mel || mel.n_len == 0) {
        result->text = strdup("");
        result->segments = NULL;
        result->n_segments = 0;
        result->language = strdup(language ? language : "auto");
        result->language_prob = 0.0f;
        result->duration = (float)n_samples / 16000.0f;
        return 0;
    }

    /* Step 2: Encode (transformer encoder layers) */
    /* TODO: full encoder transformer implementation */
    /* For now, return empty transcription (model is loaded but inference not yet wired) */

    /* Step 3: Decode (autoregressive token generation) */
    /* TODO: full decoder transformer implementation */

    /* Return empty result with metadata */
    result->text = strdup("");
    result->segments = NULL;
    result->n_segments = 0;
    result->language = strdup(language ? language : "auto");
    result->language_prob = 0.0f;
    result->duration = (float)n_samples / 16000.0f;

    c11_mel_free(&mel);
    return 0;
}

void c11_whisper_result_free(c11_whisper_result_t *result) {
    if (!result) return;
    if (result->text) free((void *)result->text);
    if (result->segments) free(result->segments);
    if (result->language) free((void *)result->language);
    memset(result, 0, sizeof(*result));
}
