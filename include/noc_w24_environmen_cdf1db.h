/*
 * noc_w24_environmen_cdf1db.h - C surface for upstream
 * agent/verify/environment.py (gate w24_environmen_cdf1db).
 *
 * The environment manifest of project verification, ported as the values it
 * decides: where the manifest lives, the tolerant recipe loader (both this
 * module's field names and grok's camelCase aliases, with `or` fallbacks and
 * the shape rules), the text -> recipe read with its ``ValueError``/``OSError``
 * degradation, whether a saved manifest wins over fresh detection, and the
 * versioned payload a save writes.  Files, mkdir and the clock are seams: the
 * caller hands in the text and the timestamp.
 */
#ifndef W24_ENVIRONMEN_CDF1DB_H
#define W24_ENVIRONMEN_CDF1DB_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MANIFEST_VERSION and the manifest's path under a project root. */
#define W24_ENVIRONMEN_CDF1DB_MANIFEST_VERSION 1
#define W24_ENVIRONMEN_CDF1DB_MANIFEST_RELPATH ".hermes/environment.json"

/* {"root": <path>} -> "<root>/.hermes/environment.json" (POSIX join; an empty
 * root has no prefix). */
json_t *w24_environmen_cdf1db_manifest_path(const json_t *req);

/* {"raw": <any>} -> Recipe.from_dict(raw).to_dict(), or null when the shape is
 * refused. */
json_t *w24_environmen_cdf1db_recipe(const json_t *req);

/* {"text": <str|null>} -> the saved recipe, or null for an absent/corrupt
 * manifest (utf-8-sig, JSON, dict shape, then the recipe loader). */
json_t *w24_environmen_cdf1db_parse_manifest(const json_t *req);

/* {"saved_present": <bool>} -> "manifest" when a saved recipe loaded,
 * "detected" otherwise. */
json_t *w24_environmen_cdf1db_source(const json_t *req);

/* {"recipe": <raw>, "updated_at": <str>} ->
 * {"version", "recipe", "updatedAt"}, or {"error": "attribute"} when the
 * recipe cannot be built (``None.to_dict()``). */
json_t *w24_environmen_cdf1db_save_payload(const json_t *req);

/* {"manifest_version": 1, "manifest_relpath": ".hermes/environment.json"}. */
json_t *w24_environmen_cdf1db_constants(void);

#ifdef __cplusplus
}
#endif
#endif
