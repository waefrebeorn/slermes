#ifndef NOC_W5_TOOLSETDIST_3D9EBC_H
#define NOC_W5_TOOLSETDIST_3D9EBC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes toolset_distributions.py (gate w5_toolsetdist_3d9ebc).
 * The static DISTRIBUTIONS table and its pure selection logic are ported.
 * random.random() and validate_toolset() are injected seams. */

/* PoP: w5_toolsetdist_3d9ebc_get_distribution @ toolset_distributions.py:get_distribution
 * Returns {"description":str, "toolsets":{name:int}} or JSON null. */
struct json_t *w5_toolsetdist_3d9ebc_get_distribution(const char *name);

/* PoP: w5_toolsetdist_3d9ebc_validate_distribution @ toolset_distributions.py:validate_distribution
 * Returns whether name is a key of DISTRIBUTIONS. */
struct json_t *w5_toolsetdist_3d9ebc_validate_distribution(const char *name);

/* PoP: w5_toolsetdist_3d9ebc_entry_members @ toolset_distributions.py:_entry_members
 * Splits a "+"-grouped entry on "+" and strips each member. */
struct json_t *w5_toolsetdist_3d9ebc_entry_members(const char *entry);

/* PoP: w5_toolsetdist_3d9ebc_sample @ toolset_distributions.py:sample_toolsets_from_distribution
 * rolls is a JSON array of float draws consumed left-to-right, one per valid
 * entry; valid is a JSON array of names for which validate_toolset() is true.
 * Returns the sampled toolset-name array, or JSON null for an unknown name. */
struct json_t *w5_toolsetdist_3d9ebc_sample(const char *name,
                                            const struct json_t *rolls,
                                            const struct json_t *valid);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_TOOLSETDIST_3D9EBC_H */
