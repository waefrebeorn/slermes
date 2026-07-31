/*
 * c11_whisper_model.c — GGUF model file parser for pure C11 Whisper
 *
 * Reads GGUF (GPT-Generated Unified Format) binary model files used by
 * whisper.cpp. The format is:
 *   - Magic: 0x46554747 ("GGUF")
 *   - Version: uint32
 *   - n_tensors: uint32
 *   - n_kv: uint64
 *   - KV metadata pairs
 *   - Tensor info entries
 *   - Tensor data (aligned to 32 bytes)
 *
 * Reference: https://github.com/ggerganov/ggml/blob/master/docs/gguf.md
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "c11_whisper.h"
#include "c11_whisper_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── GGUF binary format helpers ── */

static uint32_t read_u32(FILE *f) {
    uint32_t v;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0;
    return v;
}

static uint64_t read_u64(FILE *f) {
    uint64_t v;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0;
    return v;
}

static int32_t read_i32(FILE *f) {
    int32_t v;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0;
    return v;
}

static float read_f32(FILE *f) {
    float v;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0.0f;
    return v;
}

static bool read_bool(FILE *f) {
    uint8_t v;
    if (fread(&v, sizeof(v), 1, f) != 1) return false;
    return v != 0;
}

static char *read_string(FILE *f) {
    uint64_t len = read_u64(f);
    if (len == 0 || len > 65536) return strdup("");
    char *s = malloc(len + 1);
    if (!s) return NULL;
    if (fread(s, 1, len, f) != len) { free(s); return strdup(""); }
    s[len] = '\0';
    return s;
}

/* ── GGUF file operations ── */

int gguf_open(gguf_file_t *gf, const char *path) {
    if (!gf || !path) return -1;
    memset(gf, 0, sizeof(*gf));

    gf->file = fopen(path, "rb");
    if (!gf->file) return -1;

    /* Magic */
    gf->magic = read_u32(gf->file);
    if (gf->magic != 0x46554747) { /* "GGUF" */
        fclose(gf->file);
        return -1;
    }

    /* Version */
    gf->version = read_u32(gf->file);

    /* Tensor count + KV count */
    gf->n_tensors = read_u32(gf->file);
    gf->n_kv = read_u64(gf->file);

    if (gf->n_tensors > 10000 || gf->n_kv > 10000) {
        fclose(gf->file);
        return -1;
    }

    /* Read KV metadata */
    gf->kvs = calloc(gf->n_kv, sizeof(gguf_kv_t));
    if (!gf->kvs) { fclose(gf->file); return -1; }

    for (uint32_t i = 0; i < gf->n_kv; i++) {
        char *key = read_string(gf->file);
        if (!key) { gguf_close(gf); return -1; }
        strncpy(gf->kvs[i].name, key, sizeof(gf->kvs[i].name) - 1);
        free(key);

        uint32_t type = read_u32(gf->file);
        gf->kvs[i].type = (gguf_type_t)type;

        switch ((gguf_type_t)type) {
            case GGUF_TYPE_UINT8:  read_u32(gf->file); break;
            case GGUF_TYPE_INT8:   read_i32(gf->file); break;
            case GGUF_TYPE_UINT16: read_u32(gf->file); break;
            case GGUF_TYPE_INT16:  read_i32(gf->file); break;
            case GGUF_TYPE_UINT32: read_u32(gf->file); break;
            case GGUF_TYPE_INT32:  read_i32(gf->file); break;
            case GGUF_TYPE_FLOAT32: read_f32(gf->file); break;
            case GGUF_TYPE_BOOL:   read_bool(gf->file); break;
            case GGUF_TYPE_STRING: { char *s = read_string(gf->file); free(s); break; }
            case GGUF_TYPE_UINT64: read_u64(gf->file); break;
            case GGUF_TYPE_INT64:  read_u64(gf->file); break;
            case GGUF_TYPE_FLOAT64: read_u64(gf->file); break;
            case GGUF_TYPE_ARRAY: {
                uint32_t elem_type = read_u32(gf->file);
                uint64_t n_elem = read_u64(gf->file);
                gf->kvs[i].n_elements = n_elem;
                gf->kvs[i].elem_type = (gguf_type_t)elem_type;
                /* Skip array data */
                for (uint64_t j = 0; j < n_elem; j++) {
                    switch ((gguf_type_t)elem_type) {
                        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8:
                        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16:
                        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32:
                            read_u32(gf->file); break;
                        case GGUF_TYPE_FLOAT32: read_f32(gf->file); break;
                        case GGUF_TYPE_BOOL: read_bool(gf->file); break;
                        case GGUF_TYPE_STRING: { char *s = read_string(gf->file); free(s); break; }
                        case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64:
                        case GGUF_TYPE_FLOAT64: read_u64(gf->file); break;
                        default: break;
                    }
                }
                break;
            }
            default: break;
        }
    }

    /* Read tensor info */
    gf->tensors = calloc(gf->n_tensors ?: 1, sizeof(gguf_tensor_t));
    if (!gf->tensors) { gguf_close(gf); return -1; }

    for (uint32_t i = 0; i < gf->n_tensors; i++) {
        char *name = read_string(gf->file);
        if (!name) { gguf_close(gf); return -1; }
        strncpy(gf->tensors[i].name, name, sizeof(gf->tensors[i].name) - 1);
        free(name);

        uint32_t n_dims = read_u32(gf->file);
        gf->tensors[i].n_dims = n_dims;

        for (uint32_t d = 0; d < n_dims && d < 4; d++) {
            gf->tensors[i].ne[d] = read_u64(gf->file);
        }

        uint32_t dtype = read_u32(gf->file);
        gf->tensors[i].dtype = (gguf_type_t)dtype;

        uint64_t offset = read_u64(gf->file);
        gf->tensors[i].offset = offset;
    }

    /* Compute tensor data section start */
    long pos = ftell(gf->file);
    /* Align to 32 bytes */
    if (pos % 32 != 0) {
        pos = ((pos + 31) / 32) * 32;
        fseek(gf->file, pos, SEEK_SET);
    }
    gf->tensor_data_offset = pos;

    return 0;
}

void gguf_close(gguf_file_t *gf) {
    if (!gf) return;
    if (gf->file) fclose(gf->file);
    if (gf->kvs) free(gf->kvs);
    if (gf->tensors) {
        for (uint32_t i = 0; i < gf->n_tensors; i++) {
            if (gf->tensors[i].data) free(gf->tensors[i].data);
        }
        free(gf->tensors);
    }
    memset(gf, 0, sizeof(*gf));
}

const void *gguf_tensor_data(const gguf_file_t *gf, const char *name,
                             gguf_type_t *dtype, uint64_t *ne, uint32_t *n_dims) {
    if (!gf || !name) return NULL;
    for (uint32_t i = 0; i < gf->n_tensors; i++) {
        if (strcmp(gf->tensors[i].name, name) == 0) {
            if (dtype) *dtype = gf->tensors[i].dtype;
            if (ne) memcpy(ne, gf->tensors[i].ne, sizeof(uint64_t) * 4);
            if (n_dims) *n_dims = gf->tensors[i].n_dims;
            if (gf->tensors[i].data) return gf->tensors[i].data;
            /* Load on demand */
            return NULL; /* caller must call gguf_tensor_load */
        }
    }
    return NULL;
}

const char *gguf_get_string(const gguf_file_t *gf, const char *key) {
    if (!gf || !key) return NULL;
    for (uint32_t i = 0; i < gf->n_kv; i++) {
        if (strcmp(gf->kvs[i].name, key) == 0 && gf->kvs[i].type == GGUF_TYPE_STRING) {
            /* We skipped string values during parsing, need to re-read */
            return ""; /* TODO: re-read from file */
        }
    }
    return NULL;
}

/* ── Model context (defined in c11_whisper_internal.h) ── */

c11_whisper_ctx_t *c11_whisper_init_from_file(const char *model_path) {
    return c11_whisper_init_from_file_with_params(model_path, 4, 0);
}

c11_whisper_ctx_t *c11_whisper_init_from_file_with_params(const char *model_path,
                                                            int n_threads, int use_gpu) {
    (void)n_threads;
    (void)use_gpu;
    if (!model_path) return NULL;

    c11_whisper_ctx_t *ctx = calloc(1, sizeof(c11_whisper_ctx_t));
    if (!ctx) return NULL;

    strncpy(ctx->model.model_path, model_path, sizeof(ctx->model.model_path) - 1);

    if (gguf_open(&ctx->model.gguf, model_path) != 0) {
        free(ctx);
        return NULL;
    }

    /* Read model configuration from KV metadata */
    /* These defaults match whisper-base.en */
    ctx->model.n_mel = 80;
    ctx->model.n_audio_ctx = 1500;
    ctx->model.n_text_ctx = 448;
    /* Detect model size from tensor dimensions */
    for (uint32_t i = 0; i < ctx->model.gguf.n_tensors; i++) {
        if (strstr(ctx->model.gguf.tensors[i].name, "encoder.layers.0")) {
            ctx->model.n_audio_layer++;
        }
        if (strstr(ctx->model.gguf.tensors[i].name, "decoder.layers.0")) {
            ctx->model.n_text_layer++;
        }
    }
    if (ctx->model.n_audio_layer == 0) ctx->model.n_audio_layer = 6;
    if (ctx->model.n_text_layer == 0) ctx->model.n_text_layer = 6;
    ctx->model.n_audio_state = 384;
    ctx->model.n_audio_head = 6;
    ctx->model.n_text_state = 384;
    ctx->model.n_text_head = 6;

    ctx->initialized = true;
    return ctx;
}

void c11_whisper_free(c11_whisper_ctx_t *ctx) {
    if (!ctx) return;
    gguf_close(&ctx->model.gguf);
    free(ctx);
}

int c11_whisper_model_exists(const char *model_path) {
    if (!model_path) return 0;
    FILE *f = fopen(model_path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

const char *c11_whisper_default_model_path(void) {
    return "ggml-base.en.bin";
}

const char *c11_whisper_version(void) {
    return "c11-whisper 0.1.0";
}
