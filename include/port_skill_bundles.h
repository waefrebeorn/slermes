/*
 * port_skill_bundles.h -- pure helpers ported from agent/skill_bundles.py.
 *
 * The scan / cache / YAML load half stays in Python.
 */
#ifndef PORT_SKILL_BUNDLES_H
#define PORT_SKILL_BUNDLES_H

#include <stddef.h>

/* skill_bundles.py:get_bundle's key form -- "/" + slugified name. Caller frees. */
char *sb_bundle_key(const char *name);

/* skill_bundles.py:bundle_path_for -- <dir>/<slug>.yaml. `err` receives 1 when
 * the name normalizes to an empty slug (the Python raises ValueError). Caller
 * frees. */
char *sb_bundle_path_for(const char *name, const char *dir, int *err);

/* skill_bundles.py:_bundles_sig -- sorted (path, sha256 hex) of the bundle
 * files. `paths` and `contents` are parallel NULL-terminated arrays; a NULL
 * content is the OSError case and is skipped. On return *out_paths and
 * *out_digests are parallel NULL-terminated arrays the caller frees (arrays
 * and strings). */
void sb_bundles_sig(const char *const *paths, const char *const *contents,
                    char ***out_paths, char ***out_digests, int *out_count);

/* skill_bundles.py:list_bundles -- the indices of `bundles` sorted by `slugs`
 * (parallel arrays). Returns a malloc'd order array of n entries. */
int *sb_list_bundles_order(const char *const *slugs, int n);

/* skill_bundles.py:resolve_bundle_command_key -- resolve_slash_key over the
 * bundle table. Caller frees. */
char *sb_resolve_command_key(const char *command, const char *const *table);

#endif /* PORT_SKILL_BUNDLES_H */
