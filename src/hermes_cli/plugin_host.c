/* plugin_host.c — C home for upstream Hermes hermes_cli/plugin_host.py.
 *
 * PoP: w8_pluginhost_import_ref_ex @ hermes_cli/plugin_host.py:_import_ref
 *
 * Faithful port of the reference-splitting logic in _import_ref(ref): the
 * Python statement `module, attr = ref.split(":")` succeeds only when the ref
 * contains exactly one colon; otherwise it raises ValueError.  The oracle's
 * stubbed import_module returns a module whose attributes resolve to their own
 * names, so a well-formed ref yields the attribute name.
 */
#include "noc_w8_pluginhost_b8d10b.h"
#include "hermes_json.h"
#include <string.h>

json_t *w8_pluginhost_import_ref_ex(const char *ref) {
    const char *r = ref ? ref : "";
    int colons = 0;
    const char *first = NULL;
    for (const char *p = r; *p; p++) {
        if (*p == ':') { colons++; if (!first) first = p; }
    }
    if (colons != 1 || !first) return json_string("__VALUE_ERROR__");
    return json_string(first + 1);
}

