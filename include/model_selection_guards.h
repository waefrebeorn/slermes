/*
 * model_selection_guards.h — C11 port of
 * hermes_cli/model_selection_guards.py.
 *
 * Unified selection-time guard registry for model switching surfaces: one
 * evaluation point that runs every registered guard and returns the warnings
 * that fired. Surfaces render with their own confirm UX.
 */
#ifndef HERMES_CLI_MODEL_SELECTION_GUARDS_H
#define HERMES_CLI_MODEL_SELECTION_GUARDS_H

#include <json.h>
#include "model_cost_guard.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: selection_warnings @ hermes_cli/model_selection_guards.py:selection_warnings */
/* Run every registered selection guard; return a JSON array of
 * SelectionWarning objects {"kind","title","model","provider","message"}
 * (empty when none fired). include_kinds: JSON array of kinds to restrict to,
 * or NULL for all. seams may be NULL (no network pricing fallbacks). Caller
 * frees. */
json_t *selection_warnings(const char *model_name, const char *provider,
                           const char *base_url, const char *api_key,
                           const json_t *model_info,
                           const json_t *include_kinds,
                           const model_cost_guard_seams *seams);

/* PoP: combined_message @ hermes_cli/model_selection_guards.py:combined_message */
/* Join multiple warnings into one confirm-prompt body ("\n\n"-joined
 * messages). Caller frees. */
char *combined_message(const json_t *warnings);

/* PoP: combined_selection_warning @ hermes_cli/model_selection_guards.py:combined_selection_warning */
/* Drop-in replacement for expensive_model_warning call sites: NULL when no
 * guard fired, the single warning when exactly one fired, else a merged
 * warning with kind="multiple". Caller frees. */
json_t *combined_selection_warning(const char *model_name, const char *provider,
                                   const char *base_url, const char *api_key,
                                   const json_t *model_info,
                                   const model_cost_guard_seams *seams);

#endif /* HERMES_CLI_MODEL_SELECTION_GUARDS_H */
