/*
 * noc_w26_cache_8bdf62.h - C surface for upstream
 * agent/secret_sources/_cache.py (gate w26_cache_8bdf62).
 *
 * The pure decision core of the two-layer secret cache: the SHA-256 key
 * fingerprint, the TTL freshness test, the payload -> entry reader and the
 * three decisions the disk/L1 layers make (a read hit, whether a write happens
 * and what body it would carry, and which layer answers a lookup).  The file
 * open/parse, the atomic write, the clock and the home directory are seams:
 * each is a fixed input here, so what is ported is exactly what those values
 * decide.
 */
#ifndef NOC_W26_CACHE_8BDF62_H
#define NOC_W26_CACHE_8BDF62_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"material": "<str>"} -> the first 16 hex chars of SHA-256(material). */
json_t *w26_cache_8bdf62_fingerprint(const json_t *req);

/* {"fetched_at": n, "ttl_seconds": n, "now": n} -> CachedFetch.is_fresh(ttl). */
json_t *w26_cache_8bdf62_is_fresh(const json_t *req);

/* {"payload": <any>} -> {"secrets": {...}, "fetched_at": n} or null when
 * malformed; only str -> str pairs survive. */
json_t *w26_cache_8bdf62_entry_from_payload(const json_t *req);

/* {"payload": <file body or null>, "want_key": "<str>", "ttl_seconds": n,
 *  "now": n} -> the fresh entry DiskCache.read returns, else null. */
json_t *w26_cache_8bdf62_disk_read(const json_t *req);

/* {"key": "<serialized>", "entry": {secrets, fetched_at}, "ttl_seconds": n}
 * -> {"written": bool, "payload": {...}|null}: the body DiskCache.write would
 * persist, or nothing at all when the TTL disables the layer. */
json_t *w26_cache_8bdf62_write_payload(const json_t *req);

/* {"memory": {key: entry}|null, "key": "<str>", "ttl_seconds": n, "now": n,
 *  "disk": entry|null} -> {"source": "memory"|"disk"|null, "entry": ...|null}.
 * "disk" is the fixed answer of the L2 reader, not the file body. */
json_t *w26_cache_8bdf62_lookup(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
