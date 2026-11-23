#ifndef NOC_REDHOME_H
#define NOC_REDHOME_H
#include <stddef.h>

/* PoP: redhome_is_under_hermes_home @ agent/redact_hermes_home.py:_is_under_hermes_home
 * path: an absolute config.yaml path; homes: active Hermes home/root paths.
 * Outside a hosted scope sandbox_lexical_path() is None, so the host path is
 * judged as expanded+realpath'd (Windows lexical normalisation, no symlinks). */
int redhome_is_under_hermes_home(const char *path, const char *const *homes, size_t nhomes);

/* Pure Windows path primitives used by the guard (also exported for the gate).
 * PoP: redhome_win_norm @ agent/redact_hermes_home.py:_is_under_hermes_home
 * PoP: redhome_normcase  @ agent/redact_hermes_home.py:_is_under_hermes_home */
char *redhome_win_norm(const char *path);  /* malloc'd */
char *redhome_normcase(const char *path);  /* malloc'd */

#endif /* NOC_REDHOME_H */
