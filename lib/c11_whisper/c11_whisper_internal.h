/*
 * c11_whisper_internal.h — Internal types for c11_whisper
 * Not exposed to consumers; only used by implementation files.
 */
#ifndef C11_WHISPER_INTERNAL_H
#define C11_WHISPER_INTERNAL_H

#include "c11_whisper.h"

/* Forward-declared in c11_whisper.h as opaque, defined here */
struct c11_whisper_model {
    gguf_file_t gguf;
    char model_path[512];
    int n_mel;
    int n_vocab;
    int n_audio_ctx;
    int n_audio_state;
    int n_audio_head;
    int n_audio_layer;
    int n_text_ctx;
    int n_text_state;
    int n_text_head;
    int n_text_layer;
};

struct c11_whisper_ctx {
    c11_whisper_model_t model;
    bool initialized;
};

#endif /* C11_WHISPER_INTERNAL_H */
