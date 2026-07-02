/*
 * video_gen_registry.h — Video Generation Provider Registry for Hermes C.
 * Port of Python agent/video_gen_registry.py (117 lines).
 *
 * Central map of registered providers. Populated at init-time via
 * video_gen_register_provider(); consumed by the video_generate tool.
 *
 * Active selection mirrors Python logic:
 * 1. video_gen.provider env/config → return that provider (even if unavailable)
 * 2. Exactly one registered and available → return it
 * 3. Otherwise → NULL
 */
#ifndef VIDEO_GEN_REGISTRY_H
#define VIDEO_GEN_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

/* Provider max name length */
#define VIDEO_GEN_PROVIDER_NAME_MAX 64

/* Provider struct — backend for video generation */
typedef struct {
    char name[VIDEO_GEN_PROVIDER_NAME_MAX];
    char display_name[VIDEO_GEN_PROVIDER_NAME_MAX];
    bool (*is_available)(void);
    char *(*generate)(const char *prompt, const char *aspect_ratio,
                      const char *image_url, int duration, int seed,
                      bool has_audio, const char *negative_prompt,
                      const char *operation, const char *resolution);
} video_gen_provider_t;

/* Maximum registered providers */
#define VIDEO_GEN_MAX_PROVIDERS 16

/* Register a provider. name must be non-empty. Overwrites on re-registration.
 * Returns true on success, false if registry is full. */
bool video_gen_register_provider(const video_gen_provider_t *provider);

/* Return number of registered providers */
int video_gen_provider_count(void);

/* Get provider by index (0..count-1). Returns NULL if out of range. */
const video_gen_provider_t *video_gen_get_provider_by_index(int idx);

/* Lookup by name. Returns NULL if not found. */
const video_gen_provider_t *video_gen_get_provider(const char *name);

/* Resolve active provider.
 *
 * Reads VIDEO_GEN_PROVIDER env var first. Falls back:
 * 1. If exactly one provider registered and is_available(), use it.
 * 2. If a provider named "fal" is registered and is_available(), use it.
 * 3. Return NULL.
 */
const video_gen_provider_t *video_gen_get_active_provider(void);

/* Clear registry. Test-only. */
void video_gen_reset_registry(void);

/* ================================================================
 *  Provider response helpers (port of Python video_gen_provider.py)
 * ================================================================ */

/* Save a base64-encoded video to ~/.hermes/cache/videos/<prefix>_<timestamp>_<rand>.<ext>.
 * Returns malloc'd absolute path string, or NULL on failure.
 * Port of Python video_gen_provider.py:save_b64_video(). */
char *video_gen_save_b64_video(const char *b64_data, const char *prefix,
                                const char *extension);

/* Save raw video bytes to ~/.hermes/cache/videos/<prefix>_<timestamp>_<rand>.<ext>.
 * Returns malloc'd absolute path string, or NULL on failure.
 * Port of Python video_gen_provider.py:save_bytes_video(). */
char *video_gen_save_bytes_video(const unsigned char *data, size_t data_len,
                                  const char *prefix, const char *extension);

/* Build a uniform success response JSON string. Returns malloc'd string (caller free).
 * Port of Python video_gen_provider.py:success_response(). */
char *video_gen_success_response(const char *video, const char *model,
                                  const char *prompt, const char *modality,
                                  const char *aspect_ratio, int duration,
                                  const char *provider, const char *extra_json);

/* Build a uniform error response JSON string. Returns malloc'd string (caller free).
 * Port of Python video_gen_provider.py:error_response(). */
char *video_gen_error_response(const char *error, const char *error_type,
                                const char *provider, const char *model,
                                const char *prompt, const char *aspect_ratio);

#endif /* VIDEO_GEN_REGISTRY_H */
