#ifndef NOC_W5_PACKAGE_78A049_H
#define NOC_W5_PACKAGE_78A049_H
#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* C home for upstream Hermes pm/package.py (gate w5_package_78a049).
 * The pure declarative helpers are ported; fetch/unpack/verify I/O stays
 * in Python. Every symbol is prefixed with the gate id. */

typedef struct { const char *key; const char *value; } w5_package_78a049_kv_t;

typedef struct {
    const char *key;
    bool is_list;                  /* PATH value may be a list of dirs */
    const char *value;             /* used when !is_list */
    const char *const *items;      /* used when is_list */
    size_t n_items;
} w5_package_78a049_diff_item_t;

typedef struct {
    const w5_package_78a049_diff_item_t *items;
    size_t n_items;
} w5_package_78a049_diff_t;

/* PoP: w5_package_78a049_compose_env @ pm/package.py:compose_env */
json_t *w5_package_78a049_compose_env(const w5_package_78a049_diff_t *diffs,
                                      size_t n_diffs,
                                      const w5_package_78a049_kv_t *base,
                                      size_t n_base);

/* PoP: w5_package_78a049_install_error @ pm/package.py:InstallError.__init__ */
char *w5_package_78a049_install_error(const char *package, const char *cause,
                                      const char *remedy);

/* PoP: w5_package_78a049_store_entry @ pm/package.py:Package.store_entry */
char *w5_package_78a049_store_entry(const char *name, const char *version,
                                    const char *target);

/* PoP: w5_package_78a049_fetch_url @ pm/package.py:Package.fetch_url */
char *w5_package_78a049_fetch_url(const char *url, const char *version,
                                  const char *target);

/* PoP: w5_package_78a049_is_optional @ pm/package.py:Package.is_optional */
bool w5_package_78a049_is_optional(bool optional,
                                   const char *const *on_demand, size_t n_on_demand,
                                   const char *target);

/* PoP: w5_package_78a049_missing_reason @ pm/package.py:Package.missing_reason */
char *w5_package_78a049_missing_reason(const char *const *gap_targets,
                                       const char *const *gap_reasons, size_t n,
                                       const char *target);

/* PoP: w5_package_78a049_free @ pm/package.py:<module> */
void w5_package_78a049_free(char *s);

#endif /* NOC_W5_PACKAGE_78A049_H */
