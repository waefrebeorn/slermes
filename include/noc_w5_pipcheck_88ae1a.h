#ifndef NOC_W5_PIPCHECK_88AE1A_H
#define NOC_W5_PIPCHECK_88AE1A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_pipcheck_88ae1a_normalize @ pm/pip_check.py:_normalize
 * re.sub(r"[-_.]+", "-", name).lower() */
struct json_t *w5_pipcheck_88ae1a_normalize(const char *name);

/* PoP: w5_pipcheck_88ae1a_override_requirements @ pm/pip_check.py:override_requirements
 * Fixture seam: `entries` is the already-parsed [manifest] overrides list.
 * Unsupported keys return {"__error__":"ValueError"} (Python raises ValueError). */
struct json_t *w5_pipcheck_88ae1a_override_requirements(const struct json_t *entries);

/* PoP: w5_pipcheck_88ae1a_overridden_versions @ pm/pip_check.py:overridden_versions
 * `packages` is an array of [name, version] pairs; versions are sorted for JSON. */
struct json_t *w5_pipcheck_88ae1a_overridden_versions(const struct json_t *entries,
                                                     const struct json_t *packages);

/* PoP: w5_pipcheck_88ae1a_unexpected @ pm/pip_check.py:unexpected_incompatibilities
 * `lock` is null or an object with overrides/packages (same shapes as above).
 * Returns null when the report cannot be trusted, else an array of lines. */
struct json_t *w5_pipcheck_88ae1a_unexpected(const char *report, const struct json_t *lock);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_PIPCHECK_88AE1A_H */
