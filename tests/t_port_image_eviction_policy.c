/* t_port_image_eviction_policy.c — oracle harness for outbound_image_retire_count.
 * Emits one JSON line per case. */
#include "port_image_eviction_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    int blocks[128];
    int nblocks;
    int reserved_blocks;
    int sizes[128];      /* used only when has_sizes */
    int has_sizes;
    int reserved_bytes;
    int many_image_safe;
    int limit;           /* -1 = derive */
    int budget;
    int batch;
    int floor;
} case_t;

static const case_t CASES[] = {
    /* fits under the ceiling -> 0 */
    {"empty", {0}, 0, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    {"one-image", {1}, 1, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    {"under-limit", {1, 1, 1}, 3, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    {"at-limit", {1, 1, 1, 1, 1}, 5, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    /* over the strict 20 ceiling: retire in quanta of 8 */
    {"over-strict", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 12, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    {"over-strict-25", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
     25, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    /* many_image_safe raises the ceiling to 100 */
    {"safe-under-hard", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 10, 0, {0}, 0, 0, 1, -1, 24000000, 8, 3},
    {"safe-over-hard", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
     100, 0, {0}, 0, 0, 1, -1, 24000000, 8, 3},
    /* reserved uploads alone breach the ceiling -> floor shelters the newest */
    {"reserved-breaches", {1, 1, 1, 1, 1, 1, 1, 1}, 8, 20, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    {"reserved-breaches-2", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 10, 25, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    /* byte budget binds */
    {"byte-binds", {1, 1, 1, 1, 1}, 5, 0, {10000000, 10000000, 10000000, 10000000, 10000000},
     1, 0, 0, -1, 24000000, 8, 3},
    {"byte-binds-reserved", {1, 1, 1, 1, 1}, 5, 0, {10000000, 10000000, 10000000, 10000000, 10000000},
     1, 20000000, 0, -1, 24000000, 8, 3},
    /* multi-image carriers */
    {"multi-image", {3, 2, 4, 1, 2, 3, 1, 2}, 8, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
    {"multi-image-bytes", {3, 2, 4, 1, 2, 3, 1, 2}, 8, 0,
     {3000000, 2000000, 4000000, 1000000, 2000000, 3000000, 1000000, 2000000},
     1, 0, 0, -1, 12000000, 8, 3},
    /* small batch / floor variations */
    {"batch-1", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 12, 0, {0}, 0, 0, 0, -1, 24000000, 1, 3},
    {"floor-0", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 12, 0, {0}, 0, 0, 0, -1, 24000000, 8, 0},
    {"floor-10", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 12, 0, {0}, 0, 0, 0, -1, 24000000, 8, 10},
    /* explicit limit overrides the derived ceiling */
    {"limit-2", {1, 1, 1, 1, 1}, 5, 0, {0}, 0, 0, 0, 2, 24000000, 8, 3},
    {"limit-0", {1, 1, 1}, 3, 0, {0}, 0, 0, 0, 0, 24000000, 8, 3},
    /* everything oversized */
    {"all-big", {9, 9, 9, 9, 9}, 5, 0, {0}, 0, 0, 0, -1, 24000000, 8, 3},
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    size_t n = sizeof(CASES) / sizeof(CASES[0]);
    for (size_t i = 0; i < n; i++) {
        const case_t *c = &CASES[i];
        int out = outbound_image_retire_count(
            c->blocks, c->nblocks, c->reserved_blocks,
            c->has_sizes ? c->sizes : NULL, c->has_sizes, c->reserved_bytes,
            c->many_image_safe, c->limit, c->budget, c->batch, c->floor);

        printf("{\"case\":%zu,\"blocks\":[", i);
        for (int k = 0; k < c->nblocks; k++)
            printf("%s%d", k ? "," : "", c->blocks[k]);
        printf("],\"reserved_blocks\":%d,\"sizes\":", c->reserved_blocks);
        if (c->has_sizes) {
            printf("[");
            for (int k = 0; k < c->nblocks; k++)
                printf("%s%d", k ? "," : "", c->sizes[k]);
            printf("]");
        } else {
            printf("null");
        }
        printf(",\"reserved_bytes\":%d,\"many_image_safe\":%d,\"limit\":",
               c->reserved_bytes, c->many_image_safe);
        if (c->limit < 0)
            printf("null");
        else
            printf("%d", c->limit);
        printf(",\"budget\":%d,\"batch\":%d,\"floor\":%d,\"out\":%d}\n",
               c->budget, c->batch, c->floor, out);
    }
    return 0;
}
