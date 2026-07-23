#ifndef HERMES_LAZY_DEPS_H
#define HERMES_LAZY_DEPS_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void lazy_pkg_name_from_spec(const char *spec, char *out, size_t out_cap);
void lazy_specifier_from_spec(const char *spec, char *out, size_t out_cap);
json_t *lazy_feature_specs(const char *feature);
json_t *lazy_feature_missing(const char *feature);
char *lazy_feature_install_command(const char *feature);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LAZY_DEPS_H */
