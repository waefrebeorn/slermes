#ifndef NOC_W5_FEATURES_C0A63B_H
#define NOC_W5_FEATURES_C0A63B_H
#include <stddef.h>

#define W5FEATURES_FILENAME "enabled-features.json"

/* PoP: w5features_write_payload @ pm/features.py:write_features
 * Returns the exact bytes written by write_features (json.dumps indent=2,
 * sorted(set(extras)), trailing newline). Malloc'd; free with free(). */
char *w5features_write_payload(const char **extras, size_t n);

/* PoP: w5features_read_payload @ pm/features.py:read_features
 * Core of read_features: returns 1 with a sorted, deduplicated list when
 * "extras" is an array, 0 (None) otherwise (missing file / bad JSON / wrong
 * type). List is malloc'd; free with w5features_free_list. */
int w5features_read_payload(const char *text, char ***out, size_t *outn);

/* PoP: w5features_opt_in @ pm/features.py:opt_in_extras
 * Returns 1 with sorted(set(names)) when every name is declared, 0 when an
 * undeclared name would raise ValueError. List is malloc'd. */
int w5features_opt_in(const char **names, size_t nn,
                      const char **declared, size_t nd,
                      char ***out, size_t *outn);

/* PoP: w5features_path @ pm/features.py:features_path (base_dir branch). */
int w5features_path(const char *base_dir, char *out, size_t cap);

void w5features_free_list(char **arr, size_t n);

#endif /* NOC_W5_FEATURES_C0A63B_H */
