/*
 * noc_w26_pluginscmd_1f9092.h - C surface for upstream
 * hermes_cli/plugins_cmd_capabilities.py (gate w26_pluginscmd_1f9092).
 *
 * The capability-consent surface of ``hermes plugins capabilities``: the
 * manifest declaration normalizer, the registry filter, and the capability
 * registry the consent screen and the declared-vs-granted table are built from.
 * The console, the config file, the disk manifests and the TTY prompt are
 * seams; what is ported is the value->value core.
 */
#ifndef NOC_W26_PLUGINSCMD_1F9092_H
#define NOC_W26_PLUGINSCMD_1F9092_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declarable capability ids this build mints (VALID_CAPABILITY_IDS). */
#define W26_PLUGINSCMD_1F9092_CAPABILITY_COUNT 9

/* req: {"manifest": {...}|null, "plugin_name": <str>} -> [capability id, ...]
 * The manifest's "capabilities:" value normalized: strings only, stripped,
 * known ids only, order preserved, duplicates dropped. */
json_t *w26_pluginscmd_1f9092_declared(const json_t *req);

/* req: {"capabilities": [...]} -> order-preserving deduplicated subset with a
 * registry entry (plugin_capabilities._known). */
json_t *w26_pluginscmd_1f9092_known(const json_t *req);

/* The capability registry: [{id, legacy_path, description}, ...] in declaration
 * order, plus the two config keys the consent state is written under. */
json_t *w26_pluginscmd_1f9092_constants(void);

#ifdef __cplusplus
}
#endif
#endif
