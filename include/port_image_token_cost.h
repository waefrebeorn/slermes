#ifndef PORT_IMAGE_TOKEN_COST_H
#define PORT_IMAGE_TOKEN_COST_H

#include "hermes_json.h"

/* Port of Python: agent/image_token_cost.py — image-bearing content parts.
 *
 * The pure counting core is `_count_image_tokens` in agent/model_metadata.py;
 * `count_images` sums it over every dict message. The learned-cost machinery
 * (ContextVar binding, per-home cache table, EMA calibration with a usage
 * anchor and atomic_json_write) is runtime + disk state and is NOT ported.
 * DEFAULT_IMAGE_TOKEN_COST, the plausible band, and the EMA alpha are kept as
 * literal constants mirroring the module top. */

#define PIC_DEFAULT_IMAGE_TOKEN_COST 1500
#define PIC_MIN_PLAUSIBLE 64
#define PIC_MAX_PLAUSIBLE 32768
#define PIC_EMA_ALPHA 0.5

/* Image-like content-part cost of one message dict (Python
 * _count_image_tokens(msg, cost): content + _anthropic_content_blocks +
 * multimodal tool result + responses `output`). cost == 1 gives the count. */
int pic_count_image_tokens(const char *msg_json, int cost_per_image);

/* Sum of per-message image part counts over all dict entries (Python
 * count_images(messages)). Accepts a JSON array of message dicts. */
int pic_count_images(const char *messages_json);

#endif /* PORT_IMAGE_TOKEN_COST_H */
