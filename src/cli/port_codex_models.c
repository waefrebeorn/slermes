/*
 * port_codex_models.c
 *
 * Pure helpers ported from hermes_cli/codex_models.py - the picker's Codex
 * model catalog synthesis.
 *
 *   cm_dedupe                <- codex_models.py:_dedupe
 *   cm_add_forward_compat    <- codex_models.py:_add_forward_compat_models
 *
 * The API fetch, the on-disk cache and the large-context variant synthesis
 * stay in Python: _add_context_variants depends on agent.model_metadata's
 * -900k eligibility table, which is a separate port.
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* codex_models.py:_FORWARD_COMPAT_TEMPLATE_MODELS
 * (synthetic_model, template_models...) - a synthetic slug is surfaced when any
 * one of its templates is present. NULL-terminated. */
static const char *const CM_FORWARD_COMPAT[][4] = {
    { "gpt-6-sol",           "gpt-5.6-sol", "gpt-5.5",      NULL },
    { "gpt-6-luna",          "gpt-5.6-luna", "gpt-5.5",     NULL },
    { "gpt-5.6-sol",         "gpt-5.5", "gpt-5.4",          NULL },
    { "gpt-5.6-terra",       "gpt-5.5", "gpt-5.4",          NULL },
    { "gpt-5.6-luna",        "gpt-5.5", "gpt-5.4",          NULL },
    { "gpt-5.5",             "gpt-5.4", "gpt-5.4-mini",     NULL },
    /* Spark surfaces whenever a compatible template is present; the backend
     * (not Hermes) gates real availability by ChatGPT Pro entitlement. */
    { "gpt-5.3-codex-spark", "gpt-5.4", "gpt-5.5",          NULL },
    { NULL, NULL, NULL, NULL }
};

/* ===========================================================================
 *  _dedupe  (codex_models.py:_dedupe)
 *
 *      list(dict.fromkeys(model_ids))
 *
 *  Order-preserving dedupe. Returns a NEW NULL-terminated array of the same
 *  string pointers (borrowed, not copied); caller frees the ARRAY only.
 * =========================================================================== */

/* PoP: _dedupe @ hermes_cli/codex_models.py:_dedupe */
const char **cm_dedupe(const char *const *model_ids, int *out_count)
{
    int n = 0;
    if (model_ids) while (model_ids[n]) n++;

    const char **out = (const char **)calloc((size_t)n + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    int k = 0;
    for (int i = 0; i < n; i++) {
        const char *id = model_ids[i];
        if (!id) continue;
        int seen = 0;
        for (int j = 0; j < k; j++) {
            if (strcmp(out[j], id) == 0) { seen = 1; break; }
        }
        if (!seen) out[k++] = id;
    }
    if (out_count) *out_count = k;
    return out;
}

/* ===========================================================================
 *  _add_forward_compat_models  (codex_models.py:_add_forward_compat_models)
 *
 *      ordered = _dedupe(model_ids)
 *      seen = set(ordered)
 *      for synthetic_model, template_models in _FORWARD_COMPAT_TEMPLATE_MODELS:
 *          if synthetic_model not in seen and any(template in seen
 *                                                 for template in template_models):
 *              ordered.append(synthetic_model); seen.add(synthetic_model)
 *      return ordered
 *
 *  A synthetic slug is appended when ANY of its templates is already present,
 *  and once appended it is itself "seen" (so a later template entry can match
 *  it). Returns a NEW NULL-terminated array of borrowed pointers.
 * =========================================================================== */

/* PoP: _add_forward_compat_models @ hermes_cli/codex_models.py:_add_forward_compat_models */
const char **cm_add_forward_compat(const char *const *model_ids, int *out_count)
{
    int n = 0;
    const char **base = cm_dedupe(model_ids, &n);

    /* worst case: every synthetic entry is appended */
    int cap = n + 8;
    const char **out = (const char **)calloc((size_t)cap + 1, sizeof(char *));
    if (!out) { free(base); if (out_count) *out_count = 0; return NULL; }
    memcpy(out, base, (size_t)n * sizeof(char *));
    free(base);

    for (int t = 0; CM_FORWARD_COMPAT[t][0]; t++) {
        const char *synthetic = CM_FORWARD_COMPAT[t][0];

        int already = 0;
        for (int i = 0; i < n; i++) if (strcmp(out[i], synthetic) == 0) { already = 1; break; }
        if (already) continue;

        int any_template = 0;
        for (int k = 1; CM_FORWARD_COMPAT[t][k]; k++) {
            for (int i = 0; i < n; i++) {
                if (strcmp(out[i], CM_FORWARD_COMPAT[t][k]) == 0) { any_template = 1; break; }
            }
            if (any_template) break;
        }
        if (any_template) out[n++] = synthetic;
    }
    if (out_count) *out_count = n;
    return out;
}
