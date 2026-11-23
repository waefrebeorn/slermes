#ifndef NOC_W9_SOURCESTAMP_C21B1E_H
#define NOC_W9_SOURCESTAMP_C21B1E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_sourcestamp_c21b1e_write_source_stamp @ hermes_cli/source_stamp.py:write_source_stamp
 *
 * Pure decision layer of write_source_stamp(): the stamp dict built from the
 * identity that _git_version_info() would have returned.  The git walk, the
 * timestamp and the filesystem publication are injected by the fixture:
 *   commit/commit_date/branch/base_version/derived_version/distance/dirty
 *       -> fields of the fake VersionInfo ("" == None for nullable fields,
 *          commit_date/distance are decimal int64 strings)
 *   built_at -> the frozen datetime.now(UTC).isoformat() string
 *   adopted  -> the adopted flag
 * Returns the JSON stamp object, or JSON null when commit is empty (the
 * "git cannot identify -> publish no identity" branch of the Python function).
 */
struct json_t *w9_sourcestamp_c21b1e_write_source_stamp(
    const char *commit,
    const char *commit_date,
    const char *branch,
    const char *base_version,
    const char *derived_version,
    const char *distance,
    int dirty,
    const char *built_at,
    int adopted);

/* PoP: w9_sourcestamp_c21b1e_bootstrap_receipt_key @ hermes_cli/source_stamp.py:BOOTSTRAP_RECEIPT
 * The module constant BOOTSTRAP_RECEIPT, returned as a JSON string. */
struct json_t *w9_sourcestamp_c21b1e_bootstrap_receipt_key(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_SOURCESTAMP_C21B1E_H */
