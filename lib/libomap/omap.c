/*
 * omap.c — implementation of the insertion-ordered string map (see omap.h).
 *
 * Compact-dict layout: an insertion-ordered entry array plus an
 * open-addressed index of entry ordinals. Lookup probes the index; iteration
 * walks the entry array and skips tombstones. Compaction reclaims tombstones
 * once they outnumber the live entries.
 */

#include "omap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Index slot sentinels. Ordinals are >= 0. */
#define OMAP_EMPTY   (-1)
#define OMAP_DELETED (-2)

#define OMAP_MIN_INDEX_CAP 8

typedef struct {
    char    *key;   /* owned; NULL marks a tombstone */
    void    *value;
    uint64_t hash;
} omap_entry_t;

struct omap {
    omap_entry_t      *entries;    /* insertion-ordered, may hold tombstones */
    size_t             ecount;     /* used slots in entries (live + dead) */
    size_t             ecap;
    size_t             live;       /* live entries */

    int64_t           *index;      /* open-addressed ordinals */
    size_t             icap;       /* power of two */

    omap_value_free_fn value_free;
};

/* FNV-1a — short, well-distributed for identifier-shaped keys. */
static uint64_t omap_hash(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= (uint64_t)*p;
        h *= 1099511628211ULL;
    }
    return h;
}

/* Strict-C11 string duplicate: strdup is POSIX, and this library must build
 * with a bare -std=c11 (no _GNU_SOURCE / _POSIX_C_SOURCE) on every target. */
static char *omap_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void omap_release_value(omap_t *m, void *value)
{
    if (m->value_free && value) m->value_free(value);
}

/* Insert an ordinal into the index. The caller guarantees spare capacity and
 * that the key is not already present (used by fresh inserts and rebuilds). */
static void omap_index_put(omap_t *m, uint64_t hash, int64_t ordinal)
{
    size_t mask = m->icap - 1;
    size_t slot = (size_t)hash & mask;
    while (m->index[slot] >= 0) slot = (slot + 1) & mask;
    m->index[slot] = ordinal;
}

/* Rebuild the index from the (possibly compacted) entry array. */
static bool omap_reindex(omap_t *m, size_t new_icap)
{
    if (new_icap < OMAP_MIN_INDEX_CAP) new_icap = OMAP_MIN_INDEX_CAP;
    int64_t *idx = (int64_t *)malloc(new_icap * sizeof(int64_t));
    if (!idx) return false;
    for (size_t i = 0; i < new_icap; i++) idx[i] = OMAP_EMPTY;

    free(m->index);
    m->index = idx;
    m->icap  = new_icap;

    for (size_t i = 0; i < m->ecount; i++) {
        if (!m->entries[i].key) continue;
        omap_index_put(m, m->entries[i].hash, (int64_t)i);
    }
    return true;
}

/* Slide live entries down over tombstones, then rebuild the index. Insertion
 * order is preserved because the walk is front-to-back. */
static bool omap_compact(omap_t *m)
{
    size_t w = 0;
    for (size_t r = 0; r < m->ecount; r++) {
        if (!m->entries[r].key) continue;
        if (w != r) m->entries[w] = m->entries[r];
        w++;
    }
    m->ecount = w;
    return omap_reindex(m, m->icap);
}

/* Locate a key. Returns the entry ordinal, or -1 when absent. */
static int64_t omap_find(const omap_t *m, const char *key, uint64_t hash)
{
    if (!m->index || m->live == 0) return -1;
    size_t mask = m->icap - 1;
    size_t slot = (size_t)hash & mask;
    for (size_t probe = 0; probe < m->icap; probe++) {
        int64_t ord = m->index[slot];
        if (ord == OMAP_EMPTY) return -1;
        if (ord >= 0) {
            const omap_entry_t *e = &m->entries[(size_t)ord];
            if (e->key && e->hash == hash && strcmp(e->key, key) == 0)
                return ord;
        }
        slot = (slot + 1) & mask;
    }
    return -1;
}

/* Mark a key's index slot deleted so probe chains stay intact. */
static void omap_index_erase(omap_t *m, const char *key, uint64_t hash)
{
    size_t mask = m->icap - 1;
    size_t slot = (size_t)hash & mask;
    for (size_t probe = 0; probe < m->icap; probe++) {
        int64_t ord = m->index[slot];
        if (ord == OMAP_EMPTY) return;
        if (ord >= 0) {
            const omap_entry_t *e = &m->entries[(size_t)ord];
            if (e->key && e->hash == hash && strcmp(e->key, key) == 0) {
                m->index[slot] = OMAP_DELETED;
                return;
            }
        }
        slot = (slot + 1) & mask;
    }
}

/* Ensure room for one more entry, growing/compacting as needed. */
static bool omap_reserve_one(omap_t *m)
{
    /* Tombstones dominate -> reclaim instead of growing. */
    if (m->ecount == m->ecap && m->live * 2 <= m->ecount) {
        if (!omap_compact(m)) return false;
    }
    if (m->ecount == m->ecap) {
        size_t ncap = m->ecap ? m->ecap * 2 : OMAP_MIN_INDEX_CAP;
        omap_entry_t *ne = (omap_entry_t *)realloc(m->entries, ncap * sizeof(*ne));
        if (!ne) return false;
        m->entries = ne;
        m->ecap    = ncap;
    }
    /* Keep the index at most 2/3 full (counting tombstoned ordinals). */
    if (!m->index || (m->ecount + 1) * 3 >= m->icap * 2) {
        size_t ncap = m->icap ? m->icap * 2 : OMAP_MIN_INDEX_CAP;
        while ((m->ecount + 1) * 3 >= ncap * 2) ncap *= 2;
        if (!omap_reindex(m, ncap)) return false;
    }
    return true;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

omap_t *omap_new(omap_value_free_fn value_free)
{
    omap_t *m = (omap_t *)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->value_free = value_free;
    return m;
}

void omap_clear(omap_t *m)
{
    if (!m) return;
    for (size_t i = 0; i < m->ecount; i++) {
        if (!m->entries[i].key) continue;
        free(m->entries[i].key);
        omap_release_value(m, m->entries[i].value);
        m->entries[i].key   = NULL;
        m->entries[i].value = NULL;
    }
    m->ecount = 0;
    m->live   = 0;
    for (size_t i = 0; i < m->icap; i++) m->index[i] = OMAP_EMPTY;
}

void omap_free(omap_t *m)
{
    if (!m) return;
    omap_clear(m);
    free(m->entries);
    free(m->index);
    free(m);
}

/* ── Accessors ─────────────────────────────────────────────────────────── */

size_t omap_size(const omap_t *m) { return m ? m->live : 0; }
bool   omap_empty(const omap_t *m) { return !m || m->live == 0; }

/* ── Insert / replace ──────────────────────────────────────────────────── */

bool omap_set(omap_t *m, const char *key, void *value)
{
    if (!m || !key) return false;
    uint64_t h = omap_hash(key);
    int64_t ord = omap_find(m, key, h);
    if (ord >= 0) {
        /* Replace in place: insertion position is retained. */
        omap_entry_t *e = &m->entries[(size_t)ord];
        if (e->value != value) omap_release_value(m, e->value);
        e->value = value;
        return true;
    }
    if (!omap_reserve_one(m)) return false;

    char *kdup = omap_strdup(key);
    if (!kdup) return false;

    size_t pos = m->ecount++;
    m->entries[pos].key   = kdup;
    m->entries[pos].value = value;
    m->entries[pos].hash  = h;
    omap_index_put(m, h, (int64_t)pos);
    m->live++;
    return true;
}

bool omap_setdefault(omap_t *m, const char *key, void *value, void **out_value)
{
    if (!m || !key) return false;
    uint64_t h = omap_hash(key);
    int64_t ord = omap_find(m, key, h);
    if (ord >= 0) {
        if (out_value) *out_value = m->entries[(size_t)ord].value;
        return true;
    }
    if (!omap_set(m, key, value)) return false;
    if (out_value) *out_value = value;
    return true;
}

/* ── Lookup ────────────────────────────────────────────────────────────── */

void *omap_get(const omap_t *m, const char *key)
{
    if (!m || !key) return NULL;
    int64_t ord = omap_find(m, key, omap_hash(key));
    return ord < 0 ? NULL : m->entries[(size_t)ord].value;
}

bool omap_contains(const omap_t *m, const char *key)
{
    if (!m || !key) return false;
    return omap_find(m, key, omap_hash(key)) >= 0;
}

/* ── Erase ─────────────────────────────────────────────────────────────── */

void *omap_pop(omap_t *m, const char *key)
{
    if (!m || !key) return NULL;
    uint64_t h = omap_hash(key);
    int64_t ord = omap_find(m, key, h);
    if (ord < 0) return NULL;

    omap_entry_t *e = &m->entries[(size_t)ord];
    void *value = e->value;
    omap_index_erase(m, key, h);
    free(e->key);
    e->key   = NULL;   /* tombstone: later entries keep their ordinals */
    e->value = NULL;
    m->live--;
    return value;
}

bool omap_erase(omap_t *m, const char *key)
{
    if (!m || !key) return false;
    if (!omap_contains(m, key)) return false;
    void *value = omap_pop(m, key);
    omap_release_value(m, value);
    return true;
}

/* ── Ordered iteration ─────────────────────────────────────────────────── */

bool omap_at(const omap_t *m, size_t index, const char **out_key, void **out_value)
{
    if (!m) return false;
    /* No tombstones: ordinal == position, so a full walk is O(1) per step
     * (the common case — iteration over a map that has not seen erases). */
    if (m->live == m->ecount) {
        if (index >= m->ecount) return false;
        if (out_key)   *out_key   = m->entries[index].key;
        if (out_value) *out_value = m->entries[index].value;
        return true;
    }
    size_t seen = 0;
    for (size_t i = 0; i < m->ecount; i++) {
        if (!m->entries[i].key) continue;
        if (seen == index) {
            if (out_key)   *out_key   = m->entries[i].key;
            if (out_value) *out_value = m->entries[i].value;
            return true;
        }
        seen++;
    }
    return false;
}

const char **omap_keys(const omap_t *m, size_t *out_count)
{
    if (out_count) *out_count = 0;
    if (!m || m->live == 0) return NULL;
    const char **keys = (const char **)malloc(m->live * sizeof(*keys));
    if (!keys) return NULL;
    size_t n = 0;
    for (size_t i = 0; i < m->ecount && n < m->live; i++) {
        if (!m->entries[i].key) continue;
        keys[n++] = m->entries[i].key;
    }
    if (out_count) *out_count = n;
    return keys;
}
