/*
 * port_image_eviction_policy.c — Port of agent/image_eviction_policy.py:
 * the send-path image eviction policy shared by both stateless outbound passes.
 *
 * The retire count is a STEP FUNCTION of the overshoot (not an exact
 * ``count - limit`` target) because the pass is recomputed every request:
 * an exact target moves the frontier on every new image and rewrites the
 * provider's cached prefix each turn. This port keeps that quantum logic.
 */

#include "port_image_eviction_policy.h"

#include <stdlib.h>

#define OUTBOUND_IMAGE_LIMIT 20
#define OUTBOUND_IMAGE_HARD_LIMIT 100
#define OUTBOUND_IMAGE_BUDGET_BYTES 24000000
#define IMAGE_EVICTION_BATCH 8
#define OUTBOUND_IMAGE_FLOOR 3

/* PoP: outbound_image_block_limit @ agent/image_eviction_policy.py:outbound_image_block_limit */
int outbound_image_block_limit(int many_image_safe)
{
    return many_image_safe ? OUTBOUND_IMAGE_HARD_LIMIT : OUTBOUND_IMAGE_LIMIT;
}

static int bytes_fit_k(const int *bytes_kept, int has_sizes, int reserved_bytes,
                       int budget, int k)
{
    return !has_sizes || reserved_bytes + bytes_kept[k] <= budget;
}

static int fits_k(const int *blocks_kept, int reserved_blocks, int limit,
                  const int *bytes_kept, int has_sizes, int reserved_bytes,
                  int budget, int k)
{
    return reserved_blocks + blocks_kept[k] <= limit &&
           bytes_fit_k(bytes_kept, has_sizes, reserved_bytes, budget, k);
}

/* PoP: _retire_count_for_request @ agent/image_eviction_policy.py:_retire_count_for_request */
int retire_count_for_request(const int *blocks, int total, int reserved_blocks,
                             const int *sizes, int has_sizes, int reserved_bytes,
                             int limit, int budget, int batch, int floor)
{
    int *blocks_kept = (int *)calloc((size_t)total + 1, sizeof(int));
    int *bytes_kept = (int *)calloc((size_t)total + 1, sizeof(int));
    if (!blocks_kept || !bytes_kept) {
        free(blocks_kept);
        free(bytes_kept);
        return 0;
    }
    for (int i = 0; i < total; i++) {
        blocks_kept[i + 1] = blocks_kept[i] + blocks[i];
        bytes_kept[i + 1] = bytes_kept[i] + (has_sizes ? sizes[i] : 0);
    }

    int result;
    if (fits_k(blocks_kept, reserved_blocks, limit, bytes_kept, has_sizes,
               reserved_bytes, budget, total)) {
        result = 0;
        goto done;
    }

    if (floor < 0)
        floor = 0;
    if (floor > total)
        floor = total;

    if (!fits_k(blocks_kept, reserved_blocks, limit, bytes_kept, has_sizes,
                reserved_bytes, budget, 0) &&
        bytes_fit_k(bytes_kept, has_sizes, reserved_bytes, budget, floor)) {
        /* Reserved uploads alone breach the block ceiling: shelter the floor
         * but keep advancing in whole quanta (byte pressure narrows the step). */
        int byte_window = floor;
        for (int k = floor; k <= total; k++)
            if (bytes_fit_k(bytes_kept, has_sizes, reserved_bytes, budget, k))
                byte_window = k;
        int span = byte_window - floor + 1;
        int quantum = batch < span ? batch : span;
        if (quantum < 1)
            quantum = 1;
        int excess = total - floor;
        result = excess - excess % quantum;
        goto done;
    }

    int window = 0;
    if (fits_k(blocks_kept, reserved_blocks, limit, bytes_kept, has_sizes,
               reserved_bytes, budget, 0)) {
        for (int k = 0; k <= total; k++)
            if (fits_k(blocks_kept, reserved_blocks, limit, bytes_kept, has_sizes,
                       reserved_bytes, budget, k))
                window = k;
    }
    {
        int span = window - floor;
        int quantum = batch < span ? batch : span;
        if (quantum < 1)
            quantum = 1;
        int retire = 0;
        while (retire < total) {
            retire += quantum;
            if (retire > total)
                retire = total;
            if (fits_k(blocks_kept, reserved_blocks, limit, bytes_kept, has_sizes,
                       reserved_bytes, budget, total - retire))
                break;
        }
        result = retire;
    }

done:
    free(blocks_kept);
    free(bytes_kept);
    return result;
}

/* PoP: outbound_image_retire_count @ agent/image_eviction_policy.py:outbound_image_retire_count */
int outbound_image_retire_count(const int *blocks, int total, int reserved_blocks,
                                const int *sizes, int has_sizes, int reserved_bytes,
                                int many_image_safe, int limit, int budget,
                                int batch, int floor)
{
    int effective_limit = limit >= 0 ? limit
                                     : outbound_image_block_limit(many_image_safe);
    int best = 0;
    for (int m = 1; m <= total; m++) {
        int r = retire_count_for_request(
            blocks + (total - m), m, reserved_blocks,
            has_sizes ? sizes + (total - m) : NULL, has_sizes,
            reserved_bytes, effective_limit, budget, batch, floor);
        if (r > best)
            best = r;
    }
    return best;
}
