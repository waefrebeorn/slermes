/*
 * noc_w26_pluginscmd_167cdc.h - C surface for upstream
 * hermes_cli/plugins_cmd_update_all.py (gate w26_pluginscmd_167cdc).
 *
 * ``hermes plugins update`` with no name: which checked rows ``update`` can
 * actually move (a catalog re-pin, a git pull), which it must skip and why, and
 * how many were already current.  The probe that produced the rows
 * (run_checks) and the update itself (update_one) are seams; the decision is
 * pure over the rows and the install records.
 */
#ifndef NOC_W26_PLUGINSCMD_167CDC_H
#define NOC_W26_PLUGINSCMD_167CDC_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* plan_update_all: {"results": [row...], "installs": {name: record}}
 * -> {"movable": [name...], "skipped": [[name, reason]...], "current": n} */
json_t *w26_pluginscmd_167cdc_plan_update_all(const json_t *req);

/* _short: one clipped line for a status detail. */
json_t *w26_pluginscmd_167cdc_short(const json_t *req);

/* _UPDATE_KIND and the held-with-ref message. */
json_t *w26_pluginscmd_167cdc_constants(void);

#ifdef __cplusplus
}
#endif
#endif
