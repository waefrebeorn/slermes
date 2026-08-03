/*
 * hive.c — C11 luddite hive: linked fixed blocks + skipfield + freelist.
 *
 * Layout per block:
 *   slots[cap]   void* user pointers
 *   skip[cap]    uint8_t skipfield
 *   free_head    index of first freelist slot (-1 = none)
 *
 * skipfield encoding (eager-correct):
 *   skip[i] == 0      -> slot i is DEAD (on the freelist)
 *   skip[i] == d > 0  -> slot i is LIVE and the next live slot is i + d;
 *                        if i is the last live slot, d == cap - i (one past end)
 *   Invariant: for every live i, i + skip[i] is either live or == cap.
 *   Maintained eagerly on insert/erase so iteration is a pure skip-jump.
 *
 * Erase:  mark skip[i]=0, push i onto the block freelist, repair the
 *         nearest predecessor live slot's skip (short backward scan).
 * Insert: pop the block freelist (lowest index keeps dead runs tight),
 *         write the pointer, repair this slot's skip (short forward scan)
 *         and the nearest predecessor's skip (short backward scan).
 * Iterate: follow skip deltas only — never touch dead slots.
 *
 * Handles are (block ordinal, slot) — stable across inserts/erases that do
 * not touch the slot itself. Blocks are never freed until hive_free(), so
 * every handle ever handed out stays valid.
 *
 * The freelist is intrusive: a dead slot's slots[i] stores the next free
 * index as a pointer-sized integer. Dead slots are never read as user data.
 */

#include "hive.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* A fixed-capacity slot block. */
typedef struct hive_block {
    void        **slots;
    uint8_t      *skip;
    size_t        cap;
    size_t        live;
    long          free_head;   /* -1 = freelist empty */
    struct hive_block *next;
} hive_block_t;

/* The hive: growable block array (O(1) handle lookup) + head of the
 * linked block chain (cache-friendly forward iteration). */
struct hive {
    hive_block_t **blocks;     /* block ordinal -> block* */
    size_t         block_count;
    size_t         block_cap;  /* slots per block */
    size_t         count;      /* live elements */
};

/* ── Internal helpers ─────────────────────────────────────────────────── */

static hive_block_t *hive_block_new(size_t cap)
{
    hive_block_t *b = calloc(1, sizeof(hive_block_t));
    if (!b) return NULL;
    b->slots = calloc(cap, sizeof(void *));
    b->skip  = calloc(cap, sizeof(uint8_t));
    if (!b->slots || !b->skip) {
        free(b->slots);
        free(b->skip);
        free(b);
        return NULL;
    }
    b->cap       = cap;
    b->live      = 0;
    b->next      = NULL;
    /* Seed the freelist: every slot is free, chained in reverse order so
     * slot 0 is popped first (keeps dead runs clustered at low indices). */
    b->free_head = 0;
    for (size_t i = 0; i + 1 < cap; i++)
        b->slots[i] = (void *)(uintptr_t)(i + 1);
    b->slots[cap - 1] = (void *)(uintptr_t)-1;   /* end-of-list marker */
    return b;
}

/* Nearest live slot at or before start (walking backward). Returns -1. */
static long hive_block_prev_live(const hive_block_t *b, size_t start)
{
    for (long i = (long)start; i >= 0; i--) {
        if (b->skip[i] != 0) return i;
    }
    return -1;
}

/* Nearest live slot strictly after start; returns cap if none. */
static size_t hive_block_next_live(const hive_block_t *b, size_t start)
{
    for (size_t i = start; i < b->cap; i++) {
        if (b->skip[i] != 0) return i;
    }
    return b->cap;
}

/* Repair the skip of the nearest live predecessor of `slot` so it points
 * at `slot` (which just became live). O(dead run) worst case, O(1) typical. */
static void hive_block_repair_predecessor(hive_block_t *b, size_t slot)
{
    long p = hive_block_prev_live(b, slot - 1);
    if (p >= 0) {
        b->skip[p] = (uint8_t)(slot - (size_t)p);
    }
}

/* Repair the skip of `slot` (just became live) to point at the next live
 * slot, or cap if none. O(dead run) worst case, O(1) typical. */
static void hive_block_repair_forward(hive_block_t *b, size_t slot)
{
    size_t nxt = hive_block_next_live(b, slot + 1);
    b->skip[slot] = (uint8_t)(nxt - slot);
}

/* Free one block (unlinked). */
static void hive_block_free_ref(hive_block_t *b)
{
    if (!b) return;
    free(b->slots);
    free(b->skip);
    free(b);
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

hive_t *hive_new(size_t block_cap)
{
    if (block_cap == 0) block_cap = HIVE_DEFAULT_BLOCK_CAP;
    /* skipfield is uint8_t; cap must fit (distance <= 255). */
    if (block_cap > 255) block_cap = 255;
    if (block_cap < 16)  block_cap = 16;

    hive_t *h = calloc(1, sizeof(hive_t));
    if (!h) return NULL;
    h->block_cap = block_cap;

    hive_block_t *b0 = hive_block_new(block_cap);
    if (!b0) { free(h); return NULL; }
    h->blocks = malloc(sizeof(hive_block_t *));
    if (!h->blocks) { free(b0); free(h); return NULL; }
    h->blocks[0] = b0;
    h->block_count = 1;
    return h;
}

void hive_free(hive_t *h)
{
    if (!h) return;
    hive_block_t *b = h->blocks ? h->blocks[0] : NULL;
    while (b) {
        hive_block_t *nxt = b->next;
        free(b->slots);
        free(b->skip);
        free(b);
        b = nxt;
    }
    free(h->blocks);
    free(h);
}

void hive_clear(hive_t *h)
{
    if (!h) return;
    for (size_t bi = 0; bi < h->block_count; bi++) {
        hive_block_t *b = h->blocks[bi];
        memset(b->slots, 0, b->cap * sizeof(void *));
        memset(b->skip,  0, b->cap * sizeof(uint8_t));
        b->live = 0;
        /* reseed the freelist chain */
        b->free_head = 0;
        for (size_t i = 0; i + 1 < b->cap; i++)
            b->slots[i] = (void *)(uintptr_t)(i + 1);
        b->slots[b->cap - 1] = (void *)(uintptr_t)-1;
    }
    h->count = 0;
}

/* ── Accessors ─────────────────────────────────────────────────────────── */

size_t hive_count(const hive_t *h)        { return h ? h->count : 0; }
size_t hive_block_count(const hive_t *h)  { return h ? h->block_count : 0; }
bool   hive_empty(const hive_t *h)        { return h ? h->count == 0 : true; }

/* ── Insert ────────────────────────────────────────────────────────────── */

hive_handle_t hive_insert(hive_t *h, void *ptr, bool *ok)
{
    hive_handle_t hnd = { 0, 0 };
    if (ok) *ok = false;
    if (!h) return hnd;

    /* Find a block with a free slot (prefer the earliest — keeps dead runs
     * clustered at low indices so skip repairs stay short). */
    hive_block_t *b = NULL;
    size_t bi = 0;
    for (; bi < h->block_count; bi++) {
        if (h->blocks[bi]->free_head >= 0) { b = h->blocks[bi]; break; }
    }

    if (!b) {
        /* All blocks full: append a new block. */
        hive_block_t *nb = hive_block_new(h->block_cap);
        if (!nb) return hnd;
        hive_block_t **nb2 = realloc(h->blocks,
                                     (h->block_count + 1) * sizeof(hive_block_t *));
        if (!nb2) { hive_block_free_ref(nb); return hnd; }
        h->blocks = nb2;
        /* link at the tail of the chain */
        hive_block_t *tail = h->blocks[0];
        while (tail->next) tail = tail->next;
        tail->next = nb;
        h->blocks[h->block_count] = nb;
        bi = h->block_count;
        h->block_count++;
        b = nb;
    }

    /* Pop the freelist head. */
    size_t slot = (size_t)b->free_head;
    long   next_free = (long)(uintptr_t)b->slots[slot];
    b->slots[slot] = ptr;
    b->free_head  = next_free;
    b->live++;
    h->count++;

    /* Repair skipfield: this slot is live, pointing at the next live
     * (or cap); predecessor live points at this slot. */
    hive_block_repair_forward(b, slot);
    hive_block_repair_predecessor(b, slot);

    hnd.block = bi;
    hnd.slot  = slot;
    if (ok) *ok = true;
    return hnd;
}

/* ── Erase ─────────────────────────────────────────────────────────────── */

bool hive_erase(hive_t *h, hive_handle_t hnd)
{
    if (!h || hnd.block >= h->block_count) return false;
    hive_block_t *b = h->blocks[hnd.block];
    if (hnd.slot >= b->cap || b->skip[hnd.slot] == 0) return false;

    /* Mark dead + push onto the freelist (intrusive chain). */
    b->slots[hnd.slot] = (void *)(uintptr_t)b->free_head;
    b->skip[hnd.slot]  = 0;
    b->free_head       = (long)hnd.slot;
    b->live--;
    h->count--;

    /* Repair the nearest live predecessor so it jumps over this dead slot. */
    long p = hive_block_prev_live(b, hnd.slot - 1);
    if (p >= 0) {
        size_t nxt = hive_block_next_live(b, hnd.slot + 1);
        b->skip[p] = (uint8_t)(nxt - (size_t)p);
    }
    return true;
}

/* ── Lookup ────────────────────────────────────────────────────────────── */

void *hive_get(const hive_t *h, hive_handle_t hnd)
{
    if (!h || hnd.block >= h->block_count) return NULL;
    const hive_block_t *b = h->blocks[hnd.block];
    if (hnd.slot >= b->cap || b->skip[hnd.slot] == 0) return NULL;
    return b->slots[hnd.slot];
}

bool hive_contains(const hive_t *h, hive_handle_t hnd)
{
    if (!h || hnd.block >= h->block_count) return false;
    const hive_block_t *b = h->blocks[hnd.block];
    return hnd.slot < b->cap && b->skip[hnd.slot] != 0;
}

/* ── Iteration ─────────────────────────────────────────────────────────── */

void hive_iter_begin(const hive_t *h, hive_iter_t *it)
{
    if (!h || !it) return;
    it->block     = 0;
    it->slot      = 0;
    it->remaining = h->count;
    /* advance to first live slot */
    while (it->remaining > 0 && it->block < h->block_count) {
        const hive_block_t *b = h->blocks[it->block];
        size_t s = hive_block_next_live(b, 0);
        if (s < b->cap) { it->slot = s; break; }
        it->block++;
        it->slot = 0;
    }
}

bool hive_iter_next(const hive_t *h, hive_iter_t *it,
                    hive_handle_t *out_handle, void **out_ptr)
{
    if (!h || !it || it->remaining == 0) return false;

    while (it->block < h->block_count) {
        const hive_block_t *b = h->blocks[it->block];
        if (it->slot < b->cap && b->skip[it->slot] != 0) {
            /* live slot: yield */
            if (out_handle) { out_handle->block = it->block; out_handle->slot = it->slot; }
            if (out_ptr) *out_ptr = b->slots[it->slot];
            it->remaining--;
            /* skip-jump to the next live slot */
            size_t jump = b->skip[it->slot];
            it->slot += jump;
            if (it->slot >= b->cap) { it->block++; it->slot = 0; }
            return true;
        }
        /* dead slot (should not happen with correct skipfield, but safe) */
        it->slot++;
        if (it->slot >= b->cap) { it->block++; it->slot = 0; }
    }
    return false;
}

/* ── Stats ─────────────────────────────────────────────────────────────── */

size_t hive_capacity(const hive_t *h)
{
    if (!h) return 0;
    return h->block_count * h->block_cap;
}

size_t hive_freelist_count(const hive_t *h)
{
    if (!h) return 0;
    return hive_capacity(h) - h->count;
}

size_t hive_max_contiguous(const hive_t *h)
{
    if (!h) return 0;
    size_t best = 0;
    for (size_t bi = 0; bi < h->block_count; bi++) {
        const hive_block_t *b = h->blocks[bi];
        size_t run = 0;
        for (size_t i = 0; i < b->cap; i++) {
            if (b->skip[i] != 0) run++;
            else { if (run > best) best = run; run = 0; }
        }
        if (run > best) best = run;
    }
    return best;
}
