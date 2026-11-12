#ifndef SLERMES_PORT_IMAGE_EVICTION_POLICY_H
#define SLERMES_PORT_IMAGE_EVICTION_POLICY_H

/* Send-path image eviction policy (agent/image_eviction_policy.py).
 * All sizes are in bytes; `sizes`/`has_sizes` model the optional byte dimension
 * (NULL + has_sizes=0 means the byte budget is inactive). `limit` < 0 means
 * "derive from many_image_safe". */

int outbound_image_block_limit(int many_image_safe);

int retire_count_for_request(const int *blocks, int total, int reserved_blocks,
                             const int *sizes, int has_sizes, int reserved_bytes,
                             int limit, int budget, int batch, int floor);

int outbound_image_retire_count(const int *blocks, int total, int reserved_blocks,
                                const int *sizes, int has_sizes, int reserved_bytes,
                                int many_image_safe, int limit, int budget,
                                int batch, int floor);

#endif /* SLERMES_PORT_IMAGE_EVICTION_POLICY_H */
