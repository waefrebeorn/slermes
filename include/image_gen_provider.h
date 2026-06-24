/*
 * image_gen_provider.h — Image Generation Provider API.
 */

#ifndef IMAGE_GEN_PROVIDER_H
#define IMAGE_GEN_PROVIDER_H

#include "hermes_json.h" /* for json_t */

/* Valid aspect ratios */
extern const char *VALID_ASPECT_RATIOS[];
#define DEFAULT_ASPECT_RATIO "landscape"

/* Aspect ratio resolution */
const char *image_gen_resolve_aspect_ratio(const char *value);

/* Image cache directory */
void image_gen_cache_dir(char *out, size_t out_size);

/* Save base64-encoded image */
bool image_gen_save_b64_image(const char *b64_data, const char *prefix,
                               const char *extension, char *out_path,
                               size_t out_path_size);

/* Download and save image from URL */
bool image_gen_save_url_image(const char *url, const char *prefix,
                               int timeout_seconds, size_t max_bytes,
                               char *out_path, size_t out_path_size);

/* Response builders */
json_t *image_gen_success_response(const char *image, const char *model,
                                    const char *prompt, const char *aspect_ratio,
                                    const char *provider, json_t *extra);

json_t *image_gen_error_response(const char *error, const char *error_type,
                                  const char *provider, const char *model,
                                  const char *prompt, const char *aspect_ratio);

/* Provider vtable (for plugin registration) */
typedef struct image_gen_provider_vtable_s {
    const char *name;
    const char *display_name;
    bool (*is_available)(void);
    json_t *(*list_models)(void);
    json_t *(*get_setup_schema)(void);
    const char *(*default_model)(void);
    json_t *(*generate)(const char *prompt, const char *aspect_ratio, json_t *kwargs);
} image_gen_provider_vtable_t;

#endif /* IMAGE_GEN_PROVIDER_H */
