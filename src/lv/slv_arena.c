/*
 * slv_arena.c -- the interpreter's non-intrusive arena. See slv_internal.h.
 */
#include "slv_internal.h"

#include <stdlib.h>

void *slv_arena_alloc(slv *L, size_t n) {
    anode *h = (anode *)calloc(1, sizeof(anode) + n);
    if (!h) return NULL;
    h->next = L->root;
    L->root = h;
    return (void *)(h + 1);
}
