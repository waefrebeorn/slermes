/* noc_w11_modelswitc_9ef4ed.h — public C API for faithful port of upstream
 * Hermes tui_gateway/model_switch.py pure composition helpers.
 *
 * PoP: w11_modelswitc_9ef4ed_lazy_info_route @ tui_gateway/model_switch.py:_lazy_info_route
 * PoP: w11_modelswitc_9ef4ed_switch_away_provider @ tui_gateway/model_switch.py:_switch_away_provider
 * PoP: w11_modelswitc_9ef4ed_apply_result @ tui_gateway/model_switch.py:_apply_model_switch
 */
#ifndef NOC_W11_MODELSWITC_9EF4ED_H
#define NOC_W11_MODELSWITC_9EF4ED_H

#include "hermes_json.h"

/* Module constants: LARGE_MODEL_NOTICE_KEY and _RUNTIME_KEYS. */
json_t *w11_modelswitc_9ef4ed_constants(void);

/* _lazy_info_route(session, override) with a truthy override: the client's
 * sticky pick wins; {"model": m} plus "provider" only when truthy. */
json_t *w11_modelswitc_9ef4ed_lazy_info_route(json_t *override);

/* _current_model_runtime(agent, ...) with a live agent:
 * [provider, model, base_url, api_key] with "" for falsy fields. */
json_t *w11_modelswitc_9ef4ed_current_runtime(json_t *agent);

/* Scope expression from _apply_model_switch's return. */
const char *w11_modelswitc_9ef4ed_result_scope(bool one_turn, bool persist_global);

/* Final response dict of _apply_model_switch. */
json_t *w11_modelswitc_9ef4ed_apply_result(const char *new_model, const char *warning,
                                            bool one_turn, bool persist_global);

/* _expensive_model_confirm message composition:
 * f"{warning_message}\n\n{result_warning}" if result_warning else warning_message.
 * Returns a JSON string, or JSON null when warning_message is null. */
json_t *w11_modelswitc_9ef4ed_confirm_message(const char *warning_message,
                                               const char *result_warning);

/* _switch_away_provider with every external source injected as a seam.
 * All string args may be null (= Python None / empty). */
json_t *w11_modelswitc_9ef4ed_switch_away_provider(bool has_agent,
                                                    const char *explicit_provider,
                                                    const char *current_provider,
                                                    const char *env_provider,
                                                    bool model_seed,
                                                    const char *config_target);

#endif /* NOC_W11_MODELSWITC_9EF4ED_H */
