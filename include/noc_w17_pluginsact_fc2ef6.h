/*
 * noc_w17_pluginsact_fc2ef6.h -- public C API for the deterministic helpers of
 * hermes_cli/plugins_activation.py (wave17 gate w17_pluginsact_fc2ef6).
 */
#ifndef NOC_W17_PLUGINSACT_FC2EF6_H
#define NOC_W17_PLUGINSACT_FC2EF6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The summary for `name`: manifest name, canonical key, or bare leaf of the key.
 * Returns a fresh JSON object copy, or NULL when nothing matches. */
json_t *w17_pluginsact_fc2ef6_find_activation(json_t *summaries, const char *name);

/* One honest sentence after a reload/refused/no-gateway answer. Caller frees. */
char *w17_pluginsact_fc2ef6_gateway_reload_hint(json_t *answer, const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSACT_FC2EF6_H */
