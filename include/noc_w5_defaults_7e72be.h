#ifndef NOC_W5_DEFAULTS_7E72BE_H
#define NOC_W5_DEFAULTS_7E72BE_H
#include <stddef.h>

#define W5DEFAULTS_DECLINED_FILENAME "declined-packages.json"

/* PoP: w5defaults_canonical @ pm/defaults.py:canonical_default_name
 * Writes the canonical name into out and returns it. */
const char *w5defaults_canonical(const char *name, char *out, size_t cap);

/* PoP: w5defaults_declined @ pm/defaults.py:declined (set reconstruction)
 * Parses the declined-packages JSON text, canonicalises string entries and
 * returns a sorted, deduplicated list. 0 means no record / no names. */
int w5defaults_declined_from_json(const char *text, char ***out, size_t *outn);

/* Injected per-package metadata for default_packages (registry is IO-bound). */
typedef struct {
    const char *name;
    int is_default;   /* Package.default */
    int is_internal;  /* Package.internal */
    int missing;      /* Package.missing_reason(target) is not None */
    int found;        /* get_package(name) did not raise KeyError */
} w5defaults_pkg_t;

/* PoP: w5defaults_default_packages @ pm/defaults.py:default_packages
 * `declined` is the already-canonicalised refused set injected by the gate. */
int w5defaults_default_packages(const char **names, size_t nnames,
                                const w5defaults_pkg_t *pkgs, size_t npkgs,
                                const char **declined, size_t ndeclined,
                                char ***out, size_t *outn);
void w5defaults_free_list(char **arr, size_t n);

#endif /* NOC_W5_DEFAULTS_7E72BE_H */
