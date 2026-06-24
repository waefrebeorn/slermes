/**
 * port_image_generation_tool.c — Port of Python: tools/image_generation_tool.py
 *
 * Real C implementations for image generation helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: _active_image_capabilities */
char *active_image_capabilities(void)
{
    const char *fal_key = getenv("FAL_KEY");
    const char *openai_key = getenv("OPENAI_API_KEY");
    json_t *caps = json_object();
    if (!caps) return NULL;
    if (fal_key) {
        json_object_set(caps, "fal", json_new_string("available"));
    }
    if (openai_key) {
        json_object_set(caps, "openai", json_new_string("available"));
    }
    json_object_set(caps, "stable_diffusion", json_new_string("available"));
    hermes_log(LOG_DEBUG, "port", "active_image_capabilities: queried");
    return caps;
}

/* Port of Python: _build_dynamic_image_schema */
char *build_dynamic_image_schema(void)
{
    json_t *schema = json_object();
    if (!schema) return NULL;
    json_object_set(schema, "type", json_new_string("object"));
    json_t *props = json_object();
    json_object_set(props, "prompt", json_object());
    json_object_set(props, "width", json_object());
    json_object_set(props, "height", json_object());
    json_object_set(props, "style", json_object());
    json_object_set(schema, "properties", props);
    hermes_log(LOG_DEBUG, "port", "build_dynamic_image_schema: built");
    return schema;
}

/* Port of Python: _build_fal_edit_payload */
char *build_fal_edit_payload(const char *model_id, const char *prompt,
                              const char *image_urls, double aspect_ratio,
                              const char *seed, const char *overrides)
{
    if (!model_id || !prompt) {
        hermes_log(LOG_WARNING, "port", "build_fal_edit_payload: null parameter");
        return strdup("{\"error\": \"null parameter\"}");
    }
    json_t *payload = json_object();
    if (!payload) return NULL;
    json_object_set(payload, "model_id", json_new_string(model_id));
    json_object_set(payload, "prompt", json_new_string(prompt));
    if (image_urls) {
        json_object_set(payload, "image_urls", json_new_string(image_urls));
    }
    if (aspect_ratio > 0) {
        json_object_set(payload, "aspect_ratio", json_new_number(aspect_ratio));
    }
    if (seed) {
        json_object_set(payload, "seed", json_new_string(seed));
    }
    if (overrides) {
        json_t *ov = json_parse(overrides, NULL);
        if (ov) {
            json_object_set(payload, "overrides", ov);
        }
    }
    hermes_log(LOG_INFO, "port", "build_fal_edit_payload: model=%s", model_id);
    return payload;
}
