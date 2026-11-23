#ifndef NOC_W14_IMAGEEVICT_E64946_H
#define NOC_W14_IMAGEEVICT_E64946_H

#include <stdbool.h>
#include "hermes_json.h"

/* Faithful C11 port of upstream agent/image_eviction_policy.py. */

json_t *w14_imageevict_e64946_image_dimensions_from_bytes(const char *raw_b64);
bool    w14_imageevict_e64946_image_max_side_from_data(const char *data, long long *out_max);
bool    w14_imageevict_e64946_image_within_many_image_limit(const char *data);
bool    w14_imageevict_e64946_images_all_within_many_image_limit(const json_t *datas);
int     w14_imageevict_e64946_outbound_image_block_limit(bool many_image_safe);
int     w14_imageevict_e64946_outbound_image_retire_count(const json_t *carrier_blocks,
            long long reserved_blocks, const json_t *carrier_bytes, long long reserved_bytes,
            bool many_image_safe, long long limit_or_neg1, long long budget, long long batch,
            long long floor);
json_t *w14_imageevict_e64946_latched_retire_count(long long retire, const json_t *ids,
                                                  const json_t *retired_ids);

#endif /* NOC_W14_IMAGEEVICT_E64946_H */
