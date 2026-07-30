/*
 * whisper_wrapper.cc — C wrapper for whisper.cpp C++ API
 * Compiled as C++ to access whisper.cpp headers
 */

#include "whisper_wrapper.h"
#include "whisper.h"
#include "ggml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>

/* Initialize whisper context */
whisper_context_t *whisperc_init_from_file(const char *model_path) {
    if (!model_path) return NULL;
    return (whisper_context_t *)whisper_init_from_file(model_path);
}

/* Initialize with custom params */
whisper_context_t *whisperc_init_from_file_with_params(const char *model_path, int n_threads, int use_gpu) {
    if (!model_path) return NULL;
    
    struct whisper_context_params params = whisper_context_default_params();
    params.use_gpu = use_gpu ? true : false;
    
    return (whisper_context_t *)whisper_init_from_file_with_params(model_path, params);
}

/* Free whisper context */
void whisperc_free(whisper_context_t *ctx) {
    if (ctx) {
        whisper_free((struct whisper_context *)ctx);
    }
}

/* Full transcription */
int whisperc_full(whisper_context_t *ctx,
                 const float *samples,
                 int n_samples,
                 const char *language,
                 int translate,
                 int n_threads,
                 whisper_full_result_t *result) {
    if (!ctx || !samples || n_samples <= 0 || !result) return -1;
    
    struct whisper_context *wctx = (struct whisper_context *)ctx;
    
    /* Set up full params */
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = n_threads > 0 ? n_threads : 4;
    params.translate = translate ? true : false;
    params.language = language ? language : "auto";
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = true;
    
    /* Run transcription: whisper_full(ctx, params, samples, n_samples) */
    int ret = whisper_full(wctx, params, samples, n_samples);
    if (ret != 0) return -1;
    
    /* Get results */
    int n_segments = whisper_full_n_segments(wctx);
    if (n_segments <= 0) {
        result->text = strdup("");
        result->segments = NULL;
        result->n_segments = 0;
        result->language = strdup("auto");
        result->language_prob = 0.0f;
        result->duration = 0.0f;
        return 0;
    }
    
    /* Build full text */
    std::string full_text;
    std::vector<whisper_segment_t> segments;
    segments.reserve(n_segments);
    
    for (int i = 0; i < n_segments; i++) {
        const char *seg_text = whisper_full_get_segment_text(wctx, i);
        int64_t t0 = whisper_full_get_segment_t0(wctx, i);
        int64_t t1 = whisper_full_get_segment_t1(wctx, i);
        
        whisper_segment_t seg;
        seg.t0 = t0;
        seg.t1 = t1;
        seg.text = seg_text ? seg_text : "";
        seg.avg_logprob = 0.0f;
        seg.no_speech_prob = 0.0f;
        segments.push_back(seg);
        
        full_text += seg.text;
        if (i < n_segments - 1) full_text += " ";
    }
    
    /* Language detection */
    int lang_id = whisper_full_lang_id(wctx);
    const char *lang = lang_id >= 0 ? whisper_lang_str(lang_id) : "auto";
    
    /* Duration from last segment */
    float duration = 0.0f;
    if (n_segments > 0) {
        int64_t t1 = whisper_full_get_segment_t1(wctx, n_segments - 1);
        duration = t1 / 100.0f;  /* centiseconds to seconds */
    }
    
    /* Allocate result */
    result->text = strdup(full_text.c_str());
    result->segments = (whisper_segment_t *)malloc(n_segments * sizeof(whisper_segment_t));
    if (!result->segments) {
        free((void *)result->text);
        return -1;
    }
    memcpy(result->segments, segments.data(), n_segments * sizeof(whisper_segment_t));
    result->n_segments = n_segments;
    result->language = lang ? strdup(lang) : strdup("auto");
    result->language_prob = 1.0f;  /* Not available in this API version */
    result->duration = duration;
    
    return 0;
}

/* Free result memory */
void whisperc_full_result_free(whisper_full_result_t *result) {
    if (!result) return;
    if (result->text) free((void *)result->text);
    if (result->segments) free(result->segments);
    if (result->language) free((void *)result->language);
    result->text = NULL;
    result->segments = NULL;
    result->n_segments = 0;
    result->language = NULL;
    result->language_prob = 0.0f;
    result->duration = 0.0f;
}

/* Default model path */
const char *whisperc_default_model_path(void) {
    static const char *default_model = "ggml-base.en.bin";
    return default_model;
}

/* Check if model exists */
int whisperc_model_exists(const char *model_path) {
    if (!model_path) return 0;
    FILE *f = fopen(model_path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* Load audio file using ffmpeg */
int whisperc_load_audio(const char *audio_path, float **samples, int *n_samples) {
    if (!audio_path || !samples || !n_samples) return -1;
    
    /* Use ffmpeg to decode audio to 16kHz mono float */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -nostdin -threads 0 -i \"%s\" -f f32le -ar 16000 -ac 1 - 2>/dev/null",
             audio_path);
    
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;
    
    /* Read all samples */
    const size_t chunk_size = 16000;
    float *buffer = NULL;
    size_t buffer_cap = 0;
    size_t total_samples = 0;
    float chunk[chunk_size];
    
    while (1) {
        size_t read = fread(chunk, sizeof(float), chunk_size, pipe);
        if (read == 0) break;
        
        if (total_samples + read > buffer_cap) {
            buffer_cap = buffer_cap ? buffer_cap * 2 : 16000;
            while (total_samples + read > buffer_cap) buffer_cap *= 2;
            float *new_buffer = (float *)realloc(buffer, buffer_cap * sizeof(float));
            if (!new_buffer) {
                free(buffer);
                pclose(pipe);
                return -1;
            }
            buffer = new_buffer;
        }
        
        memcpy(buffer + total_samples, chunk, read * sizeof(float));
        total_samples += read;
    }
    
    int ret = pclose(pipe);
    if (ret != 0 || total_samples == 0) {
        free(buffer);
        return -1;
    }
    
    *samples = buffer;
    *n_samples = (int)total_samples;
    return 0;
}

/* Version */
const char *whisperc_version(void) {
    return "whisper.cpp";
}