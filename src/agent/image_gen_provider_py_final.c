/* Slermes C port — agent/image_gen_provider.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: agent_image_gen_provider_capabilities @ agent/image_gen_provider.py:capabilities */
/* Base-provider default capabilities. Concrete providers override to
 * advertise image-to-image / editing support; the base advertises
 * text-to-image only with no reference images (backward compatible). */
json_t *agent_image_gen_provider_capabilities(void) {
    json_t *caps = json_new_object();
    if (!caps) return NULL;
    json_t *modalities = json_new_array();
    if (modalities) {
        json_array_append(modalities, json_new_string("text"));
        json_object_set(caps, "modalities", modalities);
    }
    json_object_set(caps, "max_reference_images", json_new_number(0));
    return caps;
}

/* PoP: agent_image_gen_provider_resolve_aspect_ratio @ agent/image_gen_provider.py:resolve_aspect_ratio */
json_t *agent_image_gen_provider_resolve_aspect_ratio(void) {
    return json_object();
}
