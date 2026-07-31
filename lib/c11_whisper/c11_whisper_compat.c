/*
 * c11_whisper_compat.c — Compatibility shim mapping whisperc_* API to c11_whisper_* API
 *
 * This allows lib/libtranscribe/transcribe.c to use the same function names
 * without modification while the backend switches from C++ whisper.cpp
 * to pure C11 whisper.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "c11_whisper.h"
#include <stdlib.h>
#include <string.h>

/* Map old whisperc_* types to c11_whisper_* types */
typedef c11_whisper_ctx_t whisper_context_compat_t;
typedef c11_whisper_result_t whisper_full_result_compat_t;
typedef c11_whisper_segment_t whisper_segment_compat_t;

/* Whisper context (old API uses opaque whisper_context_t from whisper_wrapper.h) */
/* We provide the same function signatures */

void *whisperc_init_from_file(const char *model_path) {
    return c11_whisper_init_from_file(model_path);
}

void *whisperc_init_from_file_with_params(const char *model_path, int n_threads, int use_gpu) {
    return c11_whisper_init_from_file_with_params(model_path, n_threads, use_gpu);
}

void whisperc_free(void *ctx) {
    c11_whisper_free((c11_whisper_ctx_t *)ctx);
}

int whisperc_full(void *ctx,
                  const float *samples, int n_samples,
                  const char *language, int translate, int n_threads,
                  void *result) {
    return c11_whisper_full((c11_whisper_ctx_t *)ctx, samples, n_samples,
                            language, translate, n_threads,
                            (c11_whisper_result_t *)result);
}

void whisperc_full_result_free(void *result) {
    c11_whisper_result_free((c11_whisper_result_t *)result);
}

const char *whisperc_default_model_path(void) {
    return c11_whisper_default_model_path();
}

int whisperc_model_exists(const char *model_path) {
    return c11_whisper_model_exists(model_path);
}

int whisperc_load_audio(const char *audio_path, float **samples, int *n_samples) {
    return c11_whisper_load_audio(audio_path, samples, n_samples);
}

const char *whisperc_version(void) {
    return c11_whisper_version();
}
