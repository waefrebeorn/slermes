/*
 * model_cost_guard.h — C11 port of hermes_cli/model_cost_guard.py.
 *
 * Expensive-model confirmation helpers for model selection surfaces.
 */
#ifndef HERMES_CLI_MODEL_COST_GUARD_H
#define HERMES_CLI_MODEL_COST_GUARD_H

#include <stddef.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Existing helpers (model_cost_guard.c) */
void model_cost_guard_format_money(const double *value, char *out, size_t out_sz);
int model_cost_guard_pricing_from_model_info(const json_t *model_info,
                                             double *in_out, double *out_out,
                                             char *source, size_t source_sz);

/* PoP: _known_models_dev_provider @ hermes_cli/model_cost_guard.py:_known_models_dev_provider */
/* PROVIDER_TO_MODELS_DEV lookup after strip().lower(); NULL when unmapped. */
const char *model_cost_guard_known_models_dev_provider(const char *provider);

/* PoP: _can_trust_model_info_pricing @ hermes_cli/model_cost_guard.py:_can_trust_model_info_pricing */
int model_cost_guard_can_trust_model_info_pricing(const char *provider,
                                                  const json_t *model_info);

/* Seam callbacks mirroring the network-coupled fallbacks in
 * expensive_model_warning (agent.models_dev.get_model_info /
 * agent.usage_pricing.get_pricing_entry). NULL seams = Python's
 * except-pass / entry=None path (no pricing found). The oracle injects
 * deterministic tables here; production callers pass NULL until the models.dev
 * cache port is wired to them. */
typedef json_t *(*mcg_models_dev_lookup_fn)(void *ctx, const char *provider,
                                            const char *model);
typedef int (*mcg_pricing_lookup_fn)(void *ctx, const char *model,
                                     const char *provider, const char *base_url,
                                     const char *api_key,
                                     double *input_cost, double *output_cost,
                                     const char **source);

typedef struct {
    mcg_models_dev_lookup_fn models_dev_lookup;  /* may be NULL */
    void *models_dev_ctx;
    mcg_pricing_lookup_fn pricing_lookup;        /* may be NULL */
    void *pricing_ctx;
} model_cost_guard_seams;

/* ExpensiveModelWarning dataclass. Strings owned; free with
 * expensive_model_warning_free. */
typedef struct {
    char *model;
    char *provider;
    int has_input_cost;  double input_cost_per_million;  /* Decimal -> double */
    int has_output_cost; double output_cost_per_million;
    char *source;
    char *message;
} ExpensiveModelWarning;

/* PoP: expensive_model_warning @ hermes_cli/model_cost_guard.py:expensive_model_warning */
ExpensiveModelWarning *expensive_model_warning(
    const char *model_name, const char *provider, const char *base_url,
    const char *api_key, const json_t *model_info,
    const model_cost_guard_seams *seams);

void expensive_model_warning_free(ExpensiveModelWarning *w);

#endif /* HERMES_CLI_MODEL_COST_GUARD_H */
