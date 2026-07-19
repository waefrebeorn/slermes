/**
 * @defgroup tts_provider TTS Provider Plugin Interface
 * @brief Abstract interface for pluggable TTS backends.
 *
 * Three coexisting TTS extension surfaces (resolution order):
 *   1. Built-in providers (edge, openai, elevenlabs, ...) — always win
 *   2. Command-type providers (tts.providers.<name>.type: command)
 *   3. Plugin-registered providers (this ABC)
 *
 * Providers live in plugins/tts/<name>/.
 *
 * Response contract:
 *   synthesize() writes audio bytes to output_path, returns the path.
 *   Implementations should raise on failure.
 *
 * @{
 */
#ifndef TTS_PROVIDER_H
#define TTS_PROVIDER_H

#include "hermes.h"
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct tts_provider_t tts_provider_t;
typedef struct tts_provider_vtable_t tts_provider_vtable_t;

/* Default output format */
#define TTS_DEFAULT_FORMAT "mp3"

/* Valid output formats */
static const char *TTS_VALID_FORMATS[] = {"mp3", "wav", "ogg", "opus", "flac", NULL};

/* ================================================================
 *  TTS Provider VTable
 * ================================================================ */

struct tts_provider_vtable_t {
    /* Stable short identifier (e.g. "cartesia", "fishaudio") */
    const char *name;

    /* Human-readable label (defaults to name.title()) */
    const char *display_name;

    /* -- Core methods (must implement) ---------------------------- */

    /* Return true when provider can service calls (checks API key, SDK) */
    bool (*is_available)(tts_provider_t *self);

    /* Synthesize text and write audio bytes to output_path.
     * Returns malloc'd path string on success, NULL on failure.
     * voice: voice identifier or NULL for default.
     * model: model identifier or NULL for default.
     * speed: speech-rate multiplier (1.0 = normal), NULL = ignore.
     * format: output audio format (mp3, wav, ogg, opus, flac).
     * extra_json: forward-compat parameters (JSON string) or NULL. */
    char *(*synthesize)(tts_provider_t *self, const char *text,
                        const char *output_path,
                        const char *voice, const char *model,
                        float speed, const char *format,
                        const char *extra_json);

    /* -- Optional methods (default implementations provided) ------ */

    /* Return voice catalog. Returns JSON array of objects:
     *   [{"id": "...", "display": "...", "language": "...", ...}, ...] */
    json_node_t *(*list_voices)(tts_provider_t *self);

    /* Return model catalog. Returns JSON array of objects:
     *   [{"id": "...", "display": "...", "languages": [...], ...}, ...] */
    json_node_t *(*list_models)(tts_provider_t *self);

    /* Return setup schema for 'hermes tools' picker.
     * Returns JSON object:
     *   {"name": "...", "badge": "...", "tag": "...", "env_vars": [...]} */
    json_node_t *(*get_setup_schema)(tts_provider_t *self);

    /* Return default model id, or NULL */
    char *(*default_model)(tts_provider_t *self);

    /* Return default voice id, or NULL */
    char *(*default_voice)(tts_provider_t *self);

    /* Stream synthesized audio bytes.
     * Returns JSON array of base64-encoded chunks, or NULL if unsupported.
     * Default: returns NULL (not supported). */
    json_node_t *(*stream)(tts_provider_t *self, const char *text,
                           const char *voice, const char *model,
                           const char *format, const char *extra_json);

    /* Whether output is suitable for voice-bubble delivery.
     * Default: false. */
    bool voice_compatible;
};

/* ================================================================
 *  TTS Provider Base Struct
 * ================================================================ */

struct tts_provider_t {
    const tts_provider_vtable_t *vtable;
    void *plugin_handle;    /* Plugin .so handle (dlopen) */
    void *provider_data;    /* Provider-specific data */
};

/* ================================================================
 *  Default implementations
 * ================================================================ */

static inline json_node_t *tts_default_list_voices(tts_provider_t *self) {
    (void)self;
    return json_array();
}

static inline json_node_t *tts_default_list_models(tts_provider_t *self) {
    (void)self;
    return json_array();
}

static inline json_node_t *tts_default_get_setup_schema(tts_provider_t *self) {
    (void)self;
    json_node_t *obj = json_object();
    if (obj && self->vtable) {
        json_object_set(obj, "name", json_string(self->vtable->display_name ?
                                                  self->vtable->display_name :
                                                  self->vtable->name));
        json_object_set(obj, "badge", json_string(""));
        json_object_set(obj, "tag", json_string(""));
        json_object_set(obj, "env_vars", json_array());
    }
    return obj;
}

static inline char *tts_default_default_model(tts_provider_t *self) {
    (void)self;
    return NULL;
}

static inline char *tts_default_default_voice(tts_provider_t *self) {
    (void)self;
    return NULL;
}

static inline json_node_t *tts_default_stream(tts_provider_t *self,
                                               const char *text,
                                               const char *voice,
                                               const char *model,
                                               const char *format,
                                               const char *extra_json) {
    (void)self; (void)text; (void)voice; (void)model;
    (void)format; (void)extra_json;
    return NULL;  /* Not supported */
}

/* ================================================================
 *  Helper: resolve output format
 * ================================================================ */

/* Port of Python agent/tts_provider.py:resolve_output_format(). */
const char *tts_resolve_output_format(const char *value);

#ifdef __cplusplus
}
#endif

#endif /* TTS_PROVIDER_H */
