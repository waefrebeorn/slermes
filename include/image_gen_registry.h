/*
 * image_gen_registry.h — Image Generation Provider Registry for Hermes C.
 * Port of Python agent/image_gen_registry.py (145 lines).
 *
 * Central map of registered providers. Populated at init-time via
 * image_gen_register_provider(); consumed by the image_generate tool.
 *
 * Active selection mirrors Python logic:
 * 1. image_gen.provider env/config → return that provider (even if unavailable)
 * 2. Exactly one registered and available → return it
 * 3. Prefer legacy FAL when available
 * 4. Otherwise → NULL
 */
#ifndef IMAGE_GEN_REGISTRY_H
#define IMAGE_GEN_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#define IMAGE_GEN_PROVIDER_NAME_MAX 64
#define IMAGE_GEN_MAX_PROVIDERS 16

/* Provider struct — backend for image generation */
typedef struct {
    char name[IMAGE_GEN_PROVIDER_NAME_MAX];
    char display_name[IMAGE_GEN_PROVIDER_NAME_MAX];
    bool (*is_available)(void);
} image_gen_provider_t;

/* Register a provider. name must be non-empty. Overwrites on re-registration. */
bool image_gen_register_provider(const image_gen_provider_t *provider);

/* Return number of registered providers */
int image_gen_provider_count(void);

/* Get provider by index (0..count-1). Returns NULL if out of range. */
const image_gen_provider_t *image_gen_get_provider_by_index(int idx);

/* Lookup by name. Returns NULL if not found. */
const image_gen_provider_t *image_gen_get_provider(const char *name);

/* Resolve active provider.
 * Reads IMAGE_GEN_PROVIDER env var first. Falls back:
 * 1. If exactly one provider registered and is_available(), use it.
 * 2. If provider named "fal" registered and is_available(), use it.
 * 3. Return NULL.
 */
const image_gen_provider_t *image_gen_get_active_provider(void);

/* Clear registry. Test-only. */
void image_gen_reset_registry(void);

/* ================================================================
 *  Provider response helpers (port of Python image_gen_provider.py)
 * ================================================================ */

/* Clamp aspect_ratio to valid set: landscape, square, portrait. */
const char *image_gen_resolve_aspect_ratio(const char *value);

/* Build a uniform success response JSON string. Returns malloc'd string (caller free). */
char *image_gen_success_response(const char *image, const char *model,
                                  const char *prompt, const char *aspect_ratio,
                                  const char *provider, const char *extra_json);

/* Build a uniform error response JSON string. Returns malloc'd string (caller free). */
char *image_gen_error_response(const char *error, const char *error_type,
                                const char *provider, const char *model,
                                const char *prompt, const char *aspect_ratio);

/* Save a base64-encoded image to ~/.hermes/cache/images/<prefix>_<timestamp>_<rand>.<ext>.
 * Returns malloc'd absolute path string, or NULL on failure.
 * Port of Python image_gen_provider.py:save_b64_image(). */
char *image_gen_save_b64_image(const char *b64_data, const char *prefix,
                                const char *extension);

/* Download an image URL and save to ~/.hermes/cache/images/<prefix>_<timestamp>_<rand>.<ext>.
 * Infers extension from Content-Type header or URL suffix.
 * max_bytes: maximum body size to accept (0 = use 25MB default).
 * Returns malloc'd absolute path string, or NULL on failure.
 * Port of Python image_gen_provider.py:save_url_image(). */
char *image_gen_save_url_image(const char *url, const char *prefix,
                                double timeout_sec, int max_bytes);

/* === ABC method accessors (port of Python ImageGenProvider) =========
 * The Python ABC declares name/display_name/is_available/list_models/
 * default_model/generate as abstract methods overridden per concrete
 * provider. In C those map onto image_gen_provider_t fields + the
 * provider registry + image_generate_handler dispatch. The functions
 * below are the real C equivalents — no "N/A". */

/* name(p) — stable short identifier. Returns p->name (static buffer). */
const char *image_gen_provider_name(const image_gen_provider_t *p);
/* display_name(p) — human-readable label (p->display_name, else p->name). */
const char *image_gen_provider_display_name(const image_gen_provider_t *p);
/* is_available(p) — true when the provider can service calls. */
bool image_gen_provider_is_available(const image_gen_provider_t *p);
/* list_models(p) — returns malloc'd JSON array of catalog entries (caller frees);
 * empty "[]" when the provider exposes no user-selectable models. */
char *image_gen_provider_list_models(const image_gen_provider_t *p);
/* default_model(p) — first catalog id, or NULL when none. Caller frees. */
char *image_gen_provider_default_model(const image_gen_provider_t *p);
/* generate(p, prompt, aspect_ratio, model) — dispatch to image_generate_handler
 * for the named provider. Returns malloc'd path/URL or JSON (caller frees),
 * or NULL on failure. */
char *image_gen_provider_generate(const char *provider_name,
                                   const char *prompt, const char *aspect_ratio,
                                   const char *model);

#endif /* IMAGE_GEN_REGISTRY_H */
