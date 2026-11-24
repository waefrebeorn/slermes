/*
 * noc_w26_profilemem_515314.h - C surface for upstream
 * hermes_cli/profile_memory_config.py (gate w26_profilemem_515314).
 *
 * ``--clone`` copies config.yaml but not the memory provider's own settings, so
 * the clone booted with a provider selected and silently unavailable.  The
 * ``hermes_cli.web_routers.memory_providers`` convention -- ``<home>/<provider>/``
 * or a flat ``<home>/<provider>.json`` -- is the seam: this file owns only the
 * value logic that decides whether a parsed config names an external provider
 * and whether that name is safe to use as a path stem.
 */
#ifndef NOC_W26_PROFILEMEM_515314_H
#define NOC_W26_PROFILEMEM_515314_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* active_memory_provider(config) -> the provider name, or null for the built-in
 * store / an unsafe name / a missing ``memory`` mapping. */
json_t *w26_profilemem_515314_active_provider(const json_t *config);

/* _names_external_provider's loop over parsed configs (the profile's own and the
 * managed overlay): true when any names a non-core ``memory.provider``. */
json_t *w26_profilemem_515314_names_external(const json_t *configs);

/* The core-provider sentinels and the name pattern, as an object. */
json_t *w26_profilemem_515314_constants(void);

#ifdef __cplusplus
}
#endif
#endif
