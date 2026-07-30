/*
 * whisper_wrapper.h — C interface to whisper.cpp (whisper.h C++ API)
 * Provides local faster-whisper style transcription using whisper.cpp
 */

#ifndef WHISPER_WRAPPER_H
#define WHISPER_WRAPPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whisper model handle (opaque) */
typedef struct whisper_context whisper_context_t;

/* Whisper language detection result */
typedef struct {
    const char *lang;
    float prob;
} whisper_lang_result_t;

/* Whisper segment */
typedef struct {
    int64_t t0;           /* Start time in centiseconds */
    int64_t t1;           /* End time in centiseconds */
    const char *text;     /* Transcribed text */
    float avg_logprob;    /* Average log probability */
    float no_speech_prob; /* No speech probability */
} whisper_segment_t;

/* Full transcription result */
typedef struct {
    const char *text;              /* Full transcribed text */
    whisper_segment_t *segments;   /* Individual segments */
    int n_segments;                /* Number of segments */
    const char *language;          /* Detected language */
    float language_prob;           /* Language detection confidence */
    float duration;                /* Audio duration in seconds */
} whisper_full_result_t;

/* Initialize whisper context from model file
 * Returns NULL on failure */
whisper_context_t *whisperc_init_from_file(const char *model_path);

/* Initialize whisper context from model file with custom params
 * Returns NULL on failure */
whisper_context_t *whisperc_init_from_file_with_params(const char *model_path, int n_threads, int use_gpu);

/* Free whisper context */
void whisperc_free(whisper_context_t *ctx);

/* Transcribe audio file
 * Returns 0 on success, -1 on failure */
int whisperc_full(whisper_context_t *ctx,
                 const float *samples,
                 int n_samples,
                 const char *language,        /* NULL for auto-detect */
                 int translate,               /* 1 for translate to English */
                 int n_threads,
                 whisper_full_result_t *result);

/* Free result memory */
void whisperc_full_result_free(whisper_full_result_t *result);

/* Get default model path */
const char *whisperc_default_model_path(void);

/* Check if model file exists */
int whisperc_model_exists(const char *model_path);

/* Load audio file and convert to 16kHz mono float samples
 * Returns number of samples, or -1 on failure.
 * Caller must free returned samples with free(). */
int whisperc_load_audio(const char *audio_path, float **samples, int *n_samples);

/* Version info */
const char *whisperc_version(void);

#ifdef __cplusplus
}
#endif

#endif /* WHISPER_WRAPPER_H */