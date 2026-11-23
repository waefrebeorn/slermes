#ifndef NOC_W8_PLUGINHOST_B8D10B_H
#define NOC_W8_PLUGINHOST_B8D10B_H
#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/plugin_host.py (gate w8_pluginhost_b8d10b).
 *
 * PoP: w8_pluginhost_import_ref_ex @ hermes_cli/plugin_host.py:_import_ref
 *
 * Faithful port of the reference-splitting logic in _import_ref(ref):
 *   module, attr = ref.split(":")     # ValueError unless exactly one ':'
 *   return getattr(importlib.import_module(module), attr)
 * The oracle stubs import_module so any attribute resolves to its own name,
 * so the observed result is `attr`.  Malformed refs (not exactly one ':')
 * raise ValueError; the C port reports "__VALUE_ERROR__" for those.
 */
json_t *w8_pluginhost_import_ref_ex(const char *ref);

#endif /* NOC_W8_PLUGINHOST_B8D10B_H */
