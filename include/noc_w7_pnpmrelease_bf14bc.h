#ifndef NOC_W7_PNPMRELEASE_BF14BC_H
#define NOC_W7_PNPMRELEASE_BF14BC_H
/*
 * Public C API for the port of pm/pnpm_release_age.py (gate w7_pnpmrelease_bf14bc).
 * Tests the deterministic string/selection helpers; subprocess-driven functions
 * (immature_picks/prune_excludes) are deliberately out of scope for this gate.
 */
#include "hermes_json.h"

/* is_release_age_failure(output) */
json_t *w7_pnpmrelease_is_release_age_failure(const char *output);

/* _kept_versions(entry, locked): locked maps name -> array of version strings.
 * Returns JSON null for Python None (any other entry shape). */
json_t *w7_pnpmrelease_kept_versions(const char *entry, const json_t *locked);

/* _yaml_scalar(value): value as a YAML list item pnpm reads back unchanged. */
json_t *w7_pnpmrelease_yaml_scalar(const char *value);

/* exclude_hint(picks): human-readable exclusion guidance. */
json_t *w7_pnpmrelease_exclude_hint(const json_t *picks);

#endif /* NOC_W7_PNPMRELEASE_BF14BC_H */
