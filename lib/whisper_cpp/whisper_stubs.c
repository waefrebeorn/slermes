/*
 * whisper_stubs.c — Stub implementations for whisper functions
 *
 * These stubs are used when the whisper.cpp prebuilt libraries are not
 * available. They return sensible default values so the rest of the
 * system (transcribe, voice_mode) can compile and link without error.
 *
 * When libwhisper.a is available, the real whisper_wrapper.cc is used instead.
 */

#include "whisper_wrapper.h"
#include <stdlib.h>
#include <string.h>

whisper_context_t *whisperc_init_from_file(const char *model_path) {
    (void)model_path;
    return NULL;
}

whisper_context_t *whisperc_init_from_file_with_params(
    const char *model_path, int n_threads, int use_gpu) {
    (void)model_path;
    (void)n_threads;
    (void)use_gpu;
    return NULL;
}

void whisperc_free(whisper_context_t *ctx) {
    (void)ctx;
}

int whisperc_full(whisper_context_t *ctx,
    const float *samples, int n_samples,
    const char *language, int translate, int n_threads,
    whisper_full_result_t *result) {
    (void)ctx;
    (void)samples;
    (void)n_samples;
    (void)language;
    (void)translate;
    (void)n_threads;
    (void)result;
    return -1; /* whisper not available */
}

void whisperc_full_result_free(whisper_full_result_t *result) {
    (void)result;
}

const char *whisperc_default_model_path(void) {
    return "";
}

int whisperc_model_exists(const char *model_path) {
    (void)model_path;
    return 0;
}

int whisperc_load_audio(const char *audio_path,
    float **samples, int *n_samples) {
    (void)audio_path;
    if (samples) *samples = NULL;
    if (n_samples) *n_samples = 0;
    return -1; /* whisper not available */
}

const char *whisperc_version(void) {
    return "stub (whisper not available)";
}
