/*
 * noc_w23_manifest_27beb4.h - C surface for upstream agent/pet/manifest.py
 * (gate w23_manifest_27beb4).
 *
 * The public HTTP fetch (httpx + the SSRF-safe client), the process-wide cache
 * and the wall clock are seams: the caller supplies whether the transport was
 * importable, safe and what body it produced, and this surface owns only what
 * the module does with a body -- the row coercion, the eligibility filter, the
 * cache decision and the slug lookup.
 */
#ifndef NOC_W23_MANIFEST_27BEB4_H
#define NOC_W23_MANIFEST_27BEB4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W23_MANIFEST_27BEB4_URL "https://petdex.dev/api/manifest"
#define W23_MANIFEST_27BEB4_TTL 300.0
#define W23_MANIFEST_27BEB4_DEFAULT_TIMEOUT 10.0

/* ManifestEntry.from_dict: one manifest row -> {slug, display_name, kind,
 * submitted_by, spritesheet_url, pet_json_url, zip_url} with Python's str()/or
 * coercions (None prints "None", 0/""/false fall through to the fallback). */
json_t *w23_manifest_27beb4_entry_from_dict(const json_t *data);

/* fetch_manifest over a scripted transport.  world keys: force, import_ok,
 * safe, error (the exception text the client raised, if any), payload (the
 * parsed body), cached{warm, entries}.  Answers {ok:true, from_cache, entries}
 * or {ok:false, error}. */
json_t *w23_manifest_27beb4_fetch(const json_t *world);

/* find_entry over a supplied catalogue.  req keys: slug, entries (manifest rows).
 * Answers {found, entry}; the match is case-insensitive on the stripped slug. */
json_t *w23_manifest_27beb4_find_entry(const json_t *req);

/* MANIFEST_URL, the cache TTL and the default timeout. */
json_t *w23_manifest_27beb4_consts(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_MANIFEST_27BEB4_H */
