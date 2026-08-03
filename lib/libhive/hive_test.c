/*
 * hive_test.c — oracle-style correctness test for the hive.
 *
 * Proves the skipfield invariants:
 *   1. Insert N elements -> count == N, all retrievable, iteration visits N.
 *   2. Erase every other -> count == N/2, survivors keep their handles,
 *      iteration visits exactly the survivors in slot order.
 *   3. Reinsert after erase -> reused freelist slots, stable handles.
 *   4. Skip-jump correctness: iteration must NEVER visit a dead slot and
 *      NEVER miss a live one (validated against a reference bitmap).
 */

#define _POSIX_C_SOURCE 200809L

#include "hive.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 1000

static void check_invariants(const hive_t *h, const char *when)
{
    /* capacity/block consistency */
    assert(hive_block_count(h) >= 1);
    assert(hive_capacity(h) == hive_block_count(h) * 64);
    assert(hive_freelist_count(h) == hive_capacity(h) - hive_count(h));
    /* iteration count must equal live count */
    hive_iter_t it;
    hive_iter_begin(h, &it);
    size_t visited = 0;
    while (hive_iter_next(h, &it, NULL, NULL)) visited++;
    if (visited != hive_count(h)) {
        fprintf(stderr, "ITER MISMATCH %s: visited=%zu live=%zu\n",
                when, visited, hive_count(h));
        exit(1);
    }
}

int main(void)
{
    /* ── 1. Basic insert + iterate ─────────────────────────────── */
    hive_t *h = hive_new(64);
    assert(h);
    hive_handle_t handles[N];
    bool ok;
    for (int i = 0; i < N; i++) {
        int *v = malloc(sizeof(int));
        *v = i;
        handles[i] = hive_insert(h, v, &ok);
        assert(ok);
    }
    assert(hive_count(h) == (size_t)N);
    assert(hive_block_count(h) == (size_t)(N / 64) + (N % 64 ? 1 : 0));
    check_invariants(h, "after-insert");

    /* values retrievable by handle */
    for (int i = 0; i < N; i++) {
        int *v = hive_get(h, handles[i]);
        assert(v && *v == i);
    }

    /* iteration visits all in slot order */
    {
        hive_iter_t it;
        hive_iter_begin(h, &it);
        size_t visited = 0;
        int last = -1;
        hive_handle_t hnd;
        int *v;
        while (hive_iter_next(h, &it, &hnd, (void **)&v)) {
            assert(v && *v > last);
            last = *v;
            assert(hive_contains(h, hnd));
            visited++;
        }
        assert(visited == (size_t)N);
    }

    /* ── 2. Erase every other ──────────────────────────────────── */
    for (int i = 0; i < N; i += 2) {
        assert(hive_erase(h, handles[i]));
        /* double erase must fail */
        assert(!hive_erase(h, handles[i]));
    }
    assert(hive_count(h) == (size_t)(N / 2));
    check_invariants(h, "after-erase-odd");

    /* survivors keep handles and values */
    for (int i = 1; i < N; i += 2) {
        int *v = hive_get(h, handles[i]);
        assert(v && *v == i);
    }
    /* erased slots return NULL */
    for (int i = 0; i < N; i += 2) {
        assert(hive_get(h, handles[i]) == NULL);
    }

    /* ── 3. Reinsert reuses freelist slots, stable other handles ── */
    hive_handle_t reused[N / 2];
    for (int i = 0; i < N / 2; i++) {
        int *v = malloc(sizeof(int));
        *v = 100000 + i;
        reused[i] = hive_insert(h, v, &ok);
        assert(ok);
        /* a reused slot must equal one of the erased handles */
        bool is_reused = false;
        for (int j = 0; j < N; j += 2)
            if (reused[i].block == handles[j].block &&
                reused[i].slot == handles[j].slot) { is_reused = true; break; }
        assert(is_reused);
    }
    assert(hive_count(h) == (size_t)N);
    check_invariants(h, "after-reinsert");

    /* the survivors were never moved */
    for (int i = 1; i < N; i += 2) {
        int *v = hive_get(h, handles[i]);
        assert(v && *v == i);
    }

    /* ── 4. Erase everything, then stress the skipfield ─────────── */
    for (int i = 0; i < N; i++)
        hive_erase(h, handles[i]);
    for (int i = 0; i < N / 2; i++)
        hive_erase(h, reused[i]);
    assert(hive_count(h) == 0);
    check_invariants(h, "after-erase-all");

    /* fully erased: iteration yields nothing */
    {
        hive_iter_t it;
        hive_iter_begin(h, &it);
        assert(!hive_iter_next(h, &it, NULL, NULL));
    }

    /* ── 5. Chaotic churn: random insert/erase against a bitmap ─── */
    {
        size_t n_live = 0;
        hive_handle_t hnds[512];
        bool live[512];
        memset(live, 0, sizeof(live));
        unsigned seed = 12345;
        for (int round = 0; round < 20000; round++) {
            unsigned r = rand_r(&seed) % 100;
            if (r < 55) {
                /* insert */
                int idx = (int)(rand_r(&seed) % 512);
                if (!live[idx]) {
                    int *v = malloc(sizeof(int));
                    *v = idx;
                    hnds[idx] = hive_insert(h, v, &ok);
                    assert(ok);
                    live[idx] = true;
                    n_live++;
                }
            } else {
                /* erase */
                int idx = (int)(rand_r(&seed) % 512);
                if (live[idx]) {
                    assert(hive_erase(h, hnds[idx]));
                    live[idx] = false;
                    n_live--;
                }
            }
            if (round % 100 == 0) check_invariants(h, "churn");
        }
        assert(hive_count(h) == n_live);
        check_invariants(h, "churn-final");
        /* Verify against the bitmap: because slots are REUSED across
         * inserts, a stale handle can alias a newer element. The correct
         * invariant is VALUE-based: a live idx must be findable by
         * iterating (its value appears), and a dead idx's value must not
         * appear in the iteration. */
        {
            bool seen[512];
            memset(seen, 0, sizeof(seen));
            hive_iter_t it;
            hive_iter_begin(h, &it);
            int *v;
            while (hive_iter_next(h, &it, NULL, (void **)&v))
                if (v && *v >= 0 && *v < 512) seen[*v] = true;
            for (int i = 0; i < 512; i++) {
                if (live[i]) assert(seen[i]);
                else         assert(!seen[i]);
            }
        }
        /* handles of live elements must still resolve to their value */
        for (int i = 0; i < 512; i++) {
            if (live[i]) {
                int *v = hive_get(h, hnds[i]);
                assert(v && *v == i);
            }
        }
    }

    hive_free(h);
    printf("hive_test: ALL PASS (N=%d, 20K churn rounds)\n", N);
    return 0;
}
