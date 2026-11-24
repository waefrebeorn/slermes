/*
 * noc_w25_profilesex_ccad7d.h - C surface for upstream
 * hermes_cli/profiles_export_scrub.py (gate w25_profilesex_ccad7d).
 *
 * What a profile export scrubs, as values: which staged file names carry
 * secret-shaped text, the structural masking of Hermes' ``{"encoding",
 * "value"}`` secret envelopes, and the machine-wide saved-gateways stores a
 * profile must never carry.  The tree walk, the symlink materialization and
 * ``agent.redact.redact_sensitive_text`` are seams and stay in the caller.
 */
#ifndef NOC_W25_PROFILESEX_CCAD7D_H
#define NOC_W25_PROFILESEX_CCAD7D_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* path: "<path>" -> the ``_should_redact_export_file`` verdict
 * (.cursorrules, *.env.example, config.yaml.* and the text/config suffix set). */
json_t *w25_profilesex_ccad7d_should_redact(const json_t *path);

/* node -> node with every {"encoding", "value"} envelope's non-empty string
 * value replaced by "***"; everything else is rebuilt unchanged. */
json_t *w25_profilesex_ccad7d_masked_envelopes(const json_t *node);

/* text -> the same text when JSON parsing fails or nothing was masked,
 * otherwise ``json.dumps(masked, indent=2)`` (ensure_ascii, keys in order). */
json_t *w25_profilesex_ccad7d_mask_envelopes_text(const json_t *text);

/* name -> whether it fullmatches
 * ``gateway-connections?(?:-tokens)?\.json(?:\..+)?`` (the .lock dir and
 * .corrupt-<ms> sidecars included). */
json_t *w25_profilesex_ccad7d_gateway_store_match(const json_t *name);

/* The export scrub's literal tables, as an object (sorted, so it is stable). */
json_t *w25_profilesex_ccad7d_constants(void);

#ifdef __cplusplus
}
#endif
#endif
