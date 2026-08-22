/*
 * repetition_guard.c — C11 port of agent/repetition_guard.py
 *
 * Detects repetition-dominated truncated responses BEFORE the continuation
 * nudge is appended (issue #86581). Faithful to the Python algorithm:
 *
 *   1. Fail open on NULL/short (< MIN_FRAGMENT_LENGTH=400) input.
 *   2. Fast path: one normalized (stripped) line repeated >= _MIN_REPEAT_COUNT
 *      times whose occurrences cover >= half the fragment.
 *   3. General path: fixed 60-char exact-repeat windows, sliding one char at
 *      a time; a window appearing >= max(5, ceil(n*0.5/60)) times trips.
 */

#define _POSIX_C_SOURCE 200809L

#include "repetition_guard.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal hash set/map over string keys (window substrings, lines)   */
/* ------------------------------------------------------------------ */

typedef struct rg_entry {
    char *key;
    size_t count;
    struct rg_entry *next;
} rg_entry_t;

typedef struct rg_map {
    rg_entry_t **buckets;
    size_t nbuckets;
} rg_map_t;

static size_t rg_hash(const char *s, size_t len) {
    /* FNV-1a over exactly `len` bytes (keys are fixed-length slices or
     * stripped line copies — both compared byte-exactly below). */
    size_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int rg_map_init(rg_map_t *m, size_t expected) {
    size_t nb = 16;
    while (nb < expected * 2) nb <<= 1;
    m->buckets = calloc(nb, sizeof(rg_entry_t *));
    if (!m->buckets) return -1;
    m->nbuckets = nb;
    return 0;
}

/* count[key] += 1 (inserting with a copy of the first `len` bytes when new).
 * Returns the new count, or (size_t)-1 on OOM. */
static size_t rg_map_bump(rg_map_t *m, const char *key, size_t len) {
    size_t h = rg_hash(key, len) & (m->nbuckets - 1);
    for (rg_entry_t *e = m->buckets[h]; e; e = e->next) {
        if (strlen(e->key) == len && memcmp(e->key, key, len) == 0)
            return ++e->count;
    }
    rg_entry_t *e = malloc(sizeof(*e));
    if (!e) return (size_t)-1;
    e->key = malloc(len + 1);
    if (!e->key) { free(e); return (size_t)-1; }
    memcpy(e->key, key, len);
    e->key[len] = '\0';
    e->count = 1;
    e->next = m->buckets[h];
    m->buckets[h] = e;
    return 1;
}

static void rg_map_free(rg_map_t *m) {
    if (!m->buckets) return;
    for (size_t i = 0; i < m->nbuckets; i++) {
        rg_entry_t *e = m->buckets[i];
        while (e) {
            rg_entry_t *n = e->next;
            free(e->key);
            free(e);
            e = n;
        }
    }
    free(m->buckets);
    m->buckets = NULL;
}

/* ------------------------------------------------------------------ */
/* PoP: _line_repetition_dominated @ agent/repetition_guard.py         */
/* ------------------------------------------------------------------ */

static int repetition_guard_line_repetition_dominated(const char *text, size_t n) {
    rg_map_t counts;
    if (rg_map_init(&counts, 64) != 0) return 0;

    const char *p = text;
    const char *end = text + n;
    while (p < end) {
        /* Python str.splitlines(): split on \n, \r\n and lone \r. */
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *cr = memchr(p, '\r', (size_t)(end - p));
        const char *brk = NULL;
        size_t brklen = 0;
        if (nl && cr) {
            if (cr + 1 == nl) { brk = cr; brklen = 2; }
            else if (cr < nl) { brk = cr; brklen = 1; }
            else { brk = nl; brklen = 1; }
        } else if (nl) { brk = nl; brklen = 1; }
        else if (cr) { brk = cr; brklen = 1; }

        const char *lend = brk ? brk : end;
        /* norm = line.strip() — strip Python-whitespace both ends. */
        const char *s = p;
        const char *se = lend;
        while (s < se && (unsigned char)*s <= ' ') s++;
        while (se > s && (unsigned char)se[-1] <= ' ') se--;
        if (se > s) {
            size_t norm_len = (size_t)(se - s);
            size_t c = rg_map_bump(&counts, s, norm_len);
            if (c == (size_t)-1) { rg_map_free(&counts); return 0; }
        }
        if (!brk) break;
        p = brk + brklen;
    }

    int dominated = 0;
    for (size_t i = 0; i < counts.nbuckets && !dominated; i++) {
        for (rg_entry_t *e = counts.buckets[i]; e; e = e->next) {
            /* c >= _MIN_REPEAT_COUNT and c*len(line) >= n*_DOMINANCE_RATIO */
            if (e->count >= REPETITION_GUARD_MIN_REPEAT_COUNT &&
                (double)e->count * (double)strlen(e->key) >=
                    (double)n * REPETITION_GUARD_DOMINANCE_RATIO) {
                dominated = 1;
                break;
            }
        }
    }
    rg_map_free(&counts);
    return dominated;
}

/* ------------------------------------------------------------------ */
/* PoP: is_repetition_dominated @ agent/repetition_guard.py            */
/* ------------------------------------------------------------------ */

int repetition_guard_is_repetition_dominated(const char *text) {
    if (!text) return 0;  /* non-string / absent input fails open */
    size_t n = strlen(text);
    if (n < REPETITION_GUARD_MIN_FRAGMENT_LENGTH) return 0;

    /* Fast path: one normalized line duplicated often enough to cover half
     * the fragment (repeated paragraph/sentence on its own line). */
    if (repetition_guard_line_repetition_dominated(text, n)) return 1;

    /* General path: fixed-size exact-repeat windows sliding one char at a
     * time. needed = max(_MIN_REPEAT_COUNT, ceil(n*ratio/window)). */
    size_t window = REPETITION_GUARD_REPEAT_WINDOW;
    double needed_exact =
        ceil((double)n * REPETITION_GUARD_DOMINANCE_RATIO / (double)window);
    size_t needed = (size_t)needed_exact;
    if (needed < REPETITION_GUARD_MIN_REPEAT_COUNT)
        needed = REPETITION_GUARD_MIN_REPEAT_COUNT;

    size_t positions = n - window + 1;
    rg_map_t counts;
    if (rg_map_init(&counts, positions) != 0) return 0;
    int dominated = 0;
    for (size_t i = 0; i < positions; i++) {
        size_t c = rg_map_bump(&counts, text + i, window);
        if (c == (size_t)-1) break;  /* OOM: fail open */
        if (c >= needed) { dominated = 1; break; }
    }
    rg_map_free(&counts);
    return dominated;
}
