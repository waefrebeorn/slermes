/* process_bootstrap_pure.c — pure helpers from agent/process_bootstrap.py.
 *
 * PoP: _interleave_addrinfos @ agent/process_bootstrap.py:_interleave_addrinfos
 *
 * Python:
 *   queues: dict[int, list] keyed by family, family_order: first-seen order,
 *   seen: set of (family, socktype, proto, sockaddr) markers.
 *   Pass 1 dedupes + buckets. Pass 2 round-robins one addr per family until
 *   every bucket drains.
 */

#include "process_bootstrap_pure.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Marker identity: (family, socktype, proto, sockaddr). */
static bool pbs_marker_eq(const pbs_addrinfo_t *a, const pbs_addrinfo_t *b)
{
    return a->family == b->family && a->socktype == b->socktype &&
           a->proto == b->proto &&
           strcmp(a->sockaddr, b->sockaddr) == 0;
}

size_t *pbs_interleave_addrinfos(const pbs_addrinfo_t *addrinfos, size_t n,
                                 size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!addrinfos || n == 0) return NULL;

    /* Dedupe + bucket by family, preserving first-seen order. */
    size_t *bucket_of = malloc(n * sizeof(size_t));   /* SIZE_MAX = dup */
    size_t n_families = 0;
    int *families = malloc(n * sizeof(int));
    size_t **buckets = malloc(n * sizeof(size_t *));
    size_t *bucket_len = calloc(n, sizeof(size_t));
    if (!bucket_of || !families || !buckets || !bucket_len) {
        free(bucket_of); free(families); free(buckets); free(bucket_len);
        return NULL;
    }

    size_t kept = 0;
    for (size_t i = 0; i < n; i++) {
        bool dup = false;
        for (size_t j = 0; j < i; j++) {
            if (pbs_marker_eq(&addrinfos[j], &addrinfos[i])) { dup = true; break; }
        }
        if (dup) { bucket_of[i] = SIZE_MAX; continue; }
        kept++;

        int fam = addrinfos[i].family;
        size_t fid = SIZE_MAX;
        for (size_t f = 0; f < n_families; f++)
            if (families[f] == fam) { fid = f; break; }
        if (fid == SIZE_MAX) {
            fid = n_families++;
            families[fid] = fam;
            buckets[fid] = malloc(n * sizeof(size_t));
            if (!buckets[fid]) { /* OOM: unwind */
                for (size_t k = 0; k < fid; k++) free(buckets[k]);
                free(bucket_of); free(families); free(buckets); free(bucket_len);
                return NULL;
            }
            bucket_len[fid] = 0;
        }
        buckets[fid][bucket_len[fid]++] = i;
        bucket_of[i] = fid;
    }

    /* Round-robin one address per first-seen family until all drain. */
    size_t *out = malloc((kept ? kept : 1) * sizeof(size_t));
    if (!out) {
        for (size_t k = 0; k < n_families; k++) free(buckets[k]);
        free(bucket_of); free(families); free(buckets); free(bucket_len);
        return NULL;
    }
    size_t emitted = 0;
    size_t *consumed = calloc(n_families ? n_families : 1, sizeof(size_t));
    bool more = true;
    while (more) {
        more = false;
        for (size_t f = 0; f < n_families; f++) {
            if (consumed[f] < bucket_len[f]) {
                out[emitted++] = buckets[f][consumed[f]++];
                more = true;
            }
        }
    }
    free(consumed);

    for (size_t k = 0; k < n_families; k++) free(buckets[k]);
    free(bucket_of); free(families); free(buckets); free(bucket_len);
    if (out_n) *out_n = emitted;
    return out;
}
