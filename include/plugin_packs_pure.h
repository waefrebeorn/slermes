#ifndef HERMES_PLUGIN_PACKS_PURE_H
#define HERMES_PLUGIN_PACKS_PURE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/plugin_packs.py.
 * validate_config_seed returns a malloc'd JSON string (validated dict on
 * success, {"error":"..."} on rejection) — free() it. */

int   plugin_pack_is_exact_sha(const char *ref);
char *plugin_pack_entry_label(json_t *item_json, int index);
char *plugin_pack_validate_config_seed(const char *plugin_id, const char *seed_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PLUGIN_PACKS_PURE_H */
