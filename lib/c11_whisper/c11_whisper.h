/*
 * c11_whisper.h — Pure C11 Whisper STT engine
 *
 * Replaces whisper.cpp C++ dependency with a self-contained C11 implementation.
 * Reads GGUF model files, implements transformer encoder-decoder, and
 * provides the same API as whisper_wrapper.h.
 *
 * Architecture:
 *   c11_whisper_model.c — GGUF file parser + model loader
 *   c11_whisper_encoder.c — Audio mel-spectrogram + encoder transformer
 *   c11_whisper_decoder.c — Autoregressive token decoder
 *   c11_whisper_math.c — Matrix math (matmul, softmax, GELU, layer norm)
 *
 * No third-party dependencies. Pure C11. No C++.
 */
#ifndef C11_WHISPER_H
#define C11_WHISPER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque handles (opaque structs + minimal includes) ── */

typedef struct c11_whisper_ctx c11_whisper_ctx_t;
typedef struct c11_whisper_model c11_whisper_model_t;

/* ── Types (mirror whisper_wrapper.h for drop-in replacement) ── */

typedef struct {
    int64_t t0;
    int64_t t1;
    const char *text;
    float avg_logprob;
    float no_speech_prob;
} c11_whisper_segment_t;

typedef struct {
    const char *text;
    c11_whisper_segment_t *segments;
    int n_segments;
    const char *language;
    float language_prob;
    float duration;
} c11_whisper_result_t;

/* ── API (drop-in replacement for whisper_wrapper.h) ── */

c11_whisper_ctx_t *c11_whisper_init_from_file(const char *model_path);
c11_whisper_ctx_t *c11_whisper_init_from_file_with_params(const char *model_path,
                                                            int n_threads, int use_gpu);
void c11_whisper_free(c11_whisper_ctx_t *ctx);

int c11_whisper_full(c11_whisper_ctx_t *ctx,
                     const float *samples, int n_samples,
                     const char *language, int translate, int n_threads,
                     c11_whisper_result_t *result);

void c11_whisper_result_free(c11_whisper_result_t *result);

const char *c11_whisper_default_model_path(void);
int c11_whisper_model_exists(const char *model_path);
int c11_whisper_load_audio(const char *audio_path, float **samples, int *n_samples);
const char *c11_whisper_version(void);

/* ── Internal: GGUF model parsing ── */

typedef enum {
    GGUF_TYPE_UINT8 = 0,
    GGUF_TYPE_INT8 = 1,
    GGUF_TYPE_UINT16 = 2,
    GGUF_TYPE_INT16 = 3,
    GGUF_TYPE_UINT32 = 4,
    GGUF_TYPE_INT32 = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL = 7,
    GGUF_TYPE_STRING = 8,
    GGUF_TYPE_ARRAY = 9,
    GGUF_TYPE_UINT64 = 10,
    GGUF_TYPE_INT64 = 11,
    GGUF_TYPE_FLOAT64 = 12,
} gguf_type_t;

typedef struct {
    char name[256];
    gguf_type_t type;
    uint64_t offset;     /* offset in file */
    uint64_t n_elements; /* for arrays */
    gguf_type_t elem_type; /* for arrays */
} gguf_kv_t;

typedef struct {
    char name[256];
    uint32_t n_dims;
    uint64_t ne[4];      /* dimensions */
    gguf_type_t dtype;
    uint64_t offset;     /* data offset in file */
    /* In-memory data */
    void *data;
    size_t data_size;
} gguf_tensor_t;

typedef struct {
    uint32_t magic;          /* GGUF magic */
    uint32_t version;
    uint32_t n_tensors;
    uint64_t n_kv;
    gguf_kv_t *kvs;
    gguf_tensor_t *tensors;
    uint64_t tensor_data_offset;
    FILE *file;
} gguf_file_t;

int gguf_open(gguf_file_t *gf, const char *path);
void gguf_close(gguf_file_t *gf);
const void *gguf_tensor_data(const gguf_file_t *gf, const char *name, gguf_type_t *dtype,
                             uint64_t *ne, uint32_t *n_dims);
const char *gguf_get_string(const gguf_file_t *gf, const char *key);

/* ── Internal: Mel-spectrogram ── */

typedef struct {
    int n_mel;       /* number of mel bins (80 for whisper) */
    int n_len;       /* number of time frames (3000 for whisper) */
    int n_fft;       /* FFT size (400 for whisper) */
    int hop_length;  /* hop length (160 for 16kHz) */
    int sample_rate; /* 16000 */
    float *mel;      /* [n_mel * n_len] mel-spectrogram */
} c11_mel_spectrogram_t;

void c11_mel_compute(c11_mel_spectrogram_t *mel, const float *samples, int n_samples);
void c11_mel_free(c11_mel_spectrogram_t *mel);

/* ── Internal: Transformer math ── */

void c11_matmul(float *out, const float *a, const float *b,
                int m, int k, int n);
void c11_softmax(float *x, int n);
float c11_gelu(float x);
void c11_layer_norm(float *out, const float *x, const float *gamma,
                    const float *beta, int n, float eps);

#ifdef __cplusplus
}
#endif

#endif /* C11_WHISPER_H */
