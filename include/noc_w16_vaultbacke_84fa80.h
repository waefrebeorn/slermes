#ifndef NOC_W16_VAULTBACKE_84FA80_H
#define NOC_W16_VAULTBACKE_84FA80_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/vault_backend_registry.py.
 * Builtin namespaces and the registry snapshot are injected explicitly. */
json_t *w16_vaultbacke_84fa80_namespace_error(const char *name, const json_t *builtin_names,
                                              const json_t *builtin_prefixes);
json_t *w16_vaultbacke_84fa80_static_name(const json_t *provider);
json_t *w16_vaultbacke_84fa80_enabled_plugin_backends(const json_t *registry, const json_t *cfg,
                                                      const json_t *builtin_names,
                                                      const json_t *builtin_prefixes);

#endif /* NOC_W16_VAULTBACKE_84FA80_H */
