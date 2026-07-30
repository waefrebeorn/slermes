/*
 * Faithful port of hermes_cli/model_cost_guard.py helpers used by
 * expensive_model_warning. These are pure (no live agent/thread objects):
 *   - _format_money(value)        -> "$%.2f/M" or "unknown" for NULL
 *   - _pricing_from_model_info(m) -> (input, output, "models.dev") or
 *                                    (NULL,NULL,"") when cost data absent
 *
 * In Python ModelInfo carries cost_input/cost_output (float). The C side
 * represents a resolved model as a models.dev JSON entry whose `cost` object
 * holds `input`/`output` numbers (see provider_metadata.c format_cost). We
 * mirror that shape so the helper slots into the existing resolution path.
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Format a per-million cost. value==NULL -> "unknown". */
/* PoP: _format_money @ hermes_cli/model_cost_guard.py:_format_money */
void model_cost_guard_format_money(const double *value, char *out, size_t out_sz)
{
    if (!out) return;
    if (!value) { snprintf(out, out_sz, "unknown"); return; }
    snprintf(out, out_sz, "$%.2f/M", *value);
}

/* Extract (input_cost, output_cost, source) from a model info object.
 * Mirrors Python ModelInfo (flat cost_input/cost_output floats), which is also
 * the shape provider_metadata.c emits for the API response. Returns 1 if cost
 * data present, 0 (and empties the out values) otherwise. */
/* PoP: _pricing_from_model_info @ hermes_cli/model_cost_guard.py:_pricing_from_model_info */
int model_cost_guard_pricing_from_model_info(const json_t *model_info,
                                              double *in_out, double *out_out,
                                              char *source, size_t source_sz)
{
    if (source && source_sz) source[0] = '\0';
    if (in_out) *in_out = 0;
    if (out_out) *out_out = 0;
    if (!model_info || model_info->type != JSON_OBJECT) return 0;
    json_t *inp = json_obj_get((json_t *)model_info, "cost_input");
    json_t *out = json_obj_get((json_t *)model_info, "cost_output");
    int has = 0;
    if (inp && inp->type == JSON_NUMBER && inp->num_val > 0) has = 1;
    if (out && out->type == JSON_NUMBER && out->num_val > 0) has = 1;
    if (!has) return 0;
    if (in_out)  *in_out  = (inp && inp->type == JSON_NUMBER)  ? inp->num_val  : 0.0;
    if (out_out) *out_out = (out && out->type == JSON_NUMBER) ? out->num_val : 0.0;
    if (source && source_sz) snprintf(source, source_sz, "models.dev");
    return 1;
}
