#ifndef SLERMES_PORT_IMAGE_GENERATION_TOOL_H
#define SLERMES_PORT_IMAGE_GENERATION_TOOL_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_image_generation_tool_state port_image_generation_tool_state_t;

/* Lifecycle */
port_image_generation_tool_state_t *port_image_generation_tool_state_init(void);
void port_image_generation_tool_state_cleanup(port_image_generation_tool_state_t *state);

/* Public API */
json_t *image_gen_resolve_managed_fal_gateway(void);
json_t *image_gen_submit_fal_request(const char *model, json_t *arguments);
json_t *image_gen_resolve_fal_model(void);
json_t *image_gen_build_fal_payload(const char *model_id, const char *prompt, const char *aspect_ratio, int seed, json_t *overrides);
json_t *image_gen_upscale_image(const char *image_url, const char *original_prompt);

#endif /* SLERMES_PORT_IMAGE_GENERATION_TOOL_H */
