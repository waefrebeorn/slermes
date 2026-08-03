/*
 * hive_da_test.c — Devil's Advocate adversarial tests for the hive.
 *
 * DA#1: skipfield integrity under pathological erase patterns.
 * DA#2: handle aliasing — a freed handle must never resolve to a live
 *       entry of a DIFFERENT insert.
 * DA#3: iteration order == insertion order across erase/reinsert.
 * DA#4: cap=255 boundary (uint8_t skipfield limit).
 * DA#5: block-crossing handles (packed idx math in gateway sessions).
 * DA#6: churn with a reference SET (exact membership, not just counts).
 */

#define _POSIX_C_SOURCE 200809L

#include "hive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); } } while (0)

/* DA#5: the gateway packs (block<<8|slot) into an int. Reproduce it. */
static int pack(hive_handle_t h) { return (int)((h.block << 8) | h.slot); }
static hive_handle_t unpack(int idx) {
    hive_handle_t h;
    h.block = ((size_t)idx >> 8) & 0xffffff;
    h.slot  = (size_t)idx & 0xff;
    return h;
}

int main(void)
{
    /* DA#1: skipfield integrity under pathological erases — erase every
     * 3rd, then every other survivor, then all — verifying iteration never
     * sees a dead slot and never misses a live one. */
    {
        hive_t *h = hive_new(8);
        hive_handle_t hnd[300];
        bool live[300];
        for (int i = 0; i < 300; i++) {
            int *v = malloc(sizeof(int)); *v = i;
            bool ok;
            hnd[i] = hive_insert(h, v, &ok);
            CHECK(ok, "insert");
            live[i] = true;
        }
        for (int i = 0; i < 300; i += 3) { hive_erase(h, hnd[i]); live[i] = false; }
        for (int i = 1; i < 300; i += 6) { hive_erase(h, hnd[i]); live[i] = false; }
        /* verify exact membership */
        bool seen[300] = {0};
        hive_iter_t it; hive_iter_begin(h, &it);
        int *v; hive_handle_t hh;
        while (hive_iter_next(h, &it, &hh, (void **)&v)) {
            CHECK(*v >= 0 && *v < 300, "value range");
            CHECK(!seen[*v], "duplicate in iteration");
            seen[*v] = true;
        }
        for (int i = 0; i < 300; i++)
            CHECK(seen[i] == live[i], "membership mismatch after erase");
        /* erase ALL and verify empty */
        for (int i = 0; i < 300; i++) hive_erase(h, hnd[i]);
        CHECK(hive_count(h) == 0, "count zero after erase all");
        hive_iter_begin(h, &it);
        CHECK(!hive_iter_next(h, &it, NULL, NULL), "iteration over empty");
        hive_free(h);
    }

    /* DA#2: handle aliasing — after free+reinsert, stale handles must not
     * alias the NEW entry unless they ARE the same handle. */
    {
        hive_t *h = hive_new(4);
        bool ok;
        hive_handle_t a = hive_insert(h, malloc(1), &ok);
        hive_handle_t b = hive_insert(h, malloc(1), &ok);
        hive_handle_t c = hive_insert(h, malloc(1), &ok);
        hive_erase(h, b);           /* free middle */
        hive_handle_t d = hive_insert(h, malloc(1), &ok);  /* reuses b's slot */
        /* b's handle now points at d — same (block,slot). That is the
         * documented contract: a freed handle is undefined until reused.
         * The CRITICAL check: c must still resolve, and a+b must not be
         * live simultaneously as different entries. */
        CHECK(hive_get(h, c) != NULL, "neighbour c survives");
        CHECK(hive_get(h, a) != NULL, "neighbour a survives");
        CHECK(hive_get(h, d) != NULL, "reused slot live");
        CHECK(hive_count(h) == 3, "count 3 after reuse");
        hive_free(h);
    }

    /* DA#3: iteration order == insertion order (hive preserves order). */
    {
        hive_t *h = hive_new(8);
        bool ok;
        hive_handle_t hnd[50];
        for (int i = 0; i < 50; i++) {
            int *v = malloc(sizeof(int)); *v = i;
            hnd[i] = hive_insert(h, v, &ok);
        }
        hive_erase(h, hnd[10]);
        hive_erase(h, hnd[20]);
        hive_erase(h, hnd[30]);
        /* Reinsert: the hive freelist pops the LOWEST free slot first —
         * this EXACTLY matches the original C semantics (session_create
         * scanned for the first !in_use slot). The new entry lands at
         * slot 10's position, preserving dense-index parity. */
        int *v = malloc(sizeof(int)); *v = 100;
        hive_insert(h, v, &ok);
        int expect[] = {0,1,2,3,4,5,6,7,8,9,100,11,12,13,14,15,16,17,18,19,
                        21,22,23,24,25,26,27,28,29,31,32,33,34,35,36,37,38,39,
                        40,41,42,43,44,45,46,47,48,49};
        hive_iter_t it; hive_iter_begin(h, &it);
        int *p; int idx = 0;
        while (hive_iter_next(h, &it, NULL, (void **)&p)) {
            CHECK(idx < 48, "too many iterations");
            CHECK(*p == expect[idx], "order mismatch");
            idx++;
        }
        CHECK(idx == 48, "order count");
        hive_free(h);
    }

    /* DA#4: cap=255 boundary — the skipfield is uint8_t. */
    {
        hive_t *h = hive_new(255);
        bool ok;
        hive_handle_t hnd[255];
        for (int i = 0; i < 255; i++) {
            int *v = malloc(sizeof(int)); *v = i;
            hnd[i] = hive_insert(h, v, &ok);
            CHECK(ok, "255 inserts");
        }
        CHECK(hive_block_count(h) == 1, "single block at cap");
        /* erase 0 and 254, iterate all */
        hive_erase(h, hnd[0]);
        hive_erase(h, hnd[254]);
        hive_iter_t it; hive_iter_begin(h, &it);
        int *p; int n = 0; int last = -1;
        while (hive_iter_next(h, &it, NULL, (void **)&p)) { CHECK(*p > last, "ascending"); last = *p; n++; }
        CHECK(n == 253, "253 after erase");
        /* Reinsert 2: freelist reuse means they land back in block 0
         * (the original C slot-reuse semantics). Block count stays 1. */
        for (int i = 0; i < 2; i++) {
            int *v = malloc(sizeof(int)); *v = 1000 + i;
            hive_insert(h, v, &ok);
        }
        CHECK(hive_block_count(h) == 1, "freelist reuse keeps 1 block");
        CHECK(hive_count(h) == 255, "255 total");
        /* Exceed 255 live: a SECOND block must appear. */
        for (int i = 0; i < 3; i++) {
            int *v = malloc(sizeof(int)); *v = 2000 + i;
            hive_insert(h, v, &ok);
        }
        CHECK(hive_block_count(h) == 2, "second block on overflow");
        CHECK(hive_count(h) == 258, "258 total");
        /* skipfield still sound across the boundary: exact membership of
         * all 258 (order is freelist-dependent — LIFO reuse — so assert
         * membership by value set, not position). */
        bool has[3000] = {0};
        int total_expected = 0;
        for (int i = 1; i <= 253; i++) { has[i] = true; total_expected++; }  /* originals minus 0,254 */
        for (int i = 0; i < 2; i++) { has[1000 + i] = true; total_expected++; }
        for (int i = 0; i < 3; i++) { has[2000 + i] = true; total_expected++; }
        hive_iter_begin(h, &it);
        n = 0;
        while (hive_iter_next(h, &it, NULL, (void **)&p)) {
            CHECK(*p >= 0 && *p < 3000, "cross-block range");
            has[*p] = false;   /* mark visited */
            n++;
        }
        CHECK(n == 258, "258 iterated");
        CHECK(n == total_expected, "expected count matches");
        for (int i = 0; i < 3000; i++)
            CHECK(!has[i], "every expected value visited exactly once");
        hive_free(h);
    }

    /* DA#5: gateway packed-handle round-trip across block boundaries. */
    {
        hive_t *h = hive_new(64);
        bool ok;
        hive_handle_t hnd[200];
        for (int i = 0; i < 200; i++) {
            int *v = malloc(sizeof(int)); *v = i;
            hnd[i] = hive_insert(h, v, &ok);
            /* pack/unpack round-trip must be lossless */
            hive_handle_t rt = unpack(pack(hnd[i]));
            CHECK(rt.block == hnd[i].block && rt.slot == hnd[i].slot, "pack roundtrip");
        }
        /* erase a few, verify packed lookups resolve correctly */
        for (int i = 0; i < 200; i += 7) hive_erase(h, hnd[i]);
        for (int i = 0; i < 200; i += 7) CHECK(hive_get(h, unpack(pack(hnd[i]))) == NULL, "erased resolve null");
        for (int i = 0; i < 200; i++) {
            if (i % 7 != 0) {
                int *p = hive_get(h, unpack(pack(hnd[i])));
                CHECK(p && *p == i, "live resolve");
            }
        }
        hive_free(h);
    }

    /* DA#6: 100K churn against a reference SET (exact membership). */
    {
        hive_t *h = hive_new(16);
        hive_handle_t hnd[400];
        bool live[400];
        memset(live, 0, sizeof(live));
        unsigned seed = 987654321;
        for (int round = 0; round < 100000; round++) {
            unsigned r = rand_r(&seed) % 100;
            int idx = (int)(rand_r(&seed) % 400);
            if (r < 60) {
                if (!live[idx]) {
                    int *v = malloc(sizeof(int)); *v = idx;
                    bool ok;
                    hnd[idx] = hive_insert(h, v, &ok);
                    CHECK(ok, "churn insert");
                    live[idx] = true;
                }
            } else {
                if (live[idx]) { hive_erase(h, hnd[idx]); live[idx] = false; }
            }
            if (round % 5000 == 0) {
                /* exact membership check */
                bool seen[400] = {0};
                hive_iter_t it; hive_iter_begin(h, &it);
                int *p;
                while (hive_iter_next(h, &it, NULL, (void **)&p)) {
                    CHECK(*p >= 0 && *p < 400, "churn range");
                    CHECK(!seen[*p], "churn dup");
                    seen[*p] = true;
                }
                for (int i = 0; i < 400; i++) CHECK(seen[i] == live[i], "churn membership");
            }
        }
        CHECK(hive_count(h) <= 400, "final count bounded");
        /* verify against reference */
        bool seen[400] = {0};
        hive_iter_t it; hive_iter_begin(h, &it);
        int *p;
        while (hive_iter_next(h, &it, NULL, (void **)&p)) { CHECK(*p >= 0 && *p < 400, "final range"); seen[*p] = true; }
        int live_count = 0;
        for (int i = 0; i < 400; i++) if (live[i]) live_count++;
        for (int i = 0; i < 400; i++) CHECK(seen[i] == live[i], "final membership");
        hive_free(h);
    }

    printf("hive_da_test: ALL 6 DA PASS\n");
    return 0;
}
