/*
 * port_comp_telemetry.h — pure helpers from agent/context_compressor_telemetry.py.
 * The mutating mixin entry points stay unported; these take the runtime facts
 * as parameters.
 */
#ifndef PORT_COMP_TELEMETRY_H
#define PORT_COMP_TELEMETRY_H

/* _compression_method: how the summary was produced, from two flags. Returns
 * static literal: "deterministic_fallback" if summary_fallback_used,
 * "aux_fallback_main" if has_aux_failure_error, else "llm_summary". */
const char *ctele_method(int summary_fallback_used, int has_aux_failure_error);

/* _note_aux_route_fallback's failure truncation: `failure` if len<=220, else
 * the first 217 chars rstripped + "...". Returns malloc'd string, caller frees. */
char *ctele_clamp_failure(const char *failure);

/* fit_margin = effective_aux_context - aux_prompt_tokens - (aux_output_reservation
 * or 0). resv_present=0 means the reservation was None (treated as 0). */
long long ctele_fit_margin(long long effective_aux_context,
                           long long aux_prompt_tokens,
                           long long aux_output_reservation, int resv_present);

#endif /* PORT_COMP_TELEMETRY_H */
