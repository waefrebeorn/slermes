/* turn_truncation.c — C home for upstream Hermes agent/turn_truncation.py.
 *
 * PoP: w7_turntruncat_repetition_copy @ agent/turn_truncation.py:repetition_copy
 * PoP: w7_turntruncat_partial_result @ agent/turn_truncation.py:partial_result
 * PoP: w7_turntruncat_boosted_output_cap @ agent/turn_truncation.py:boosted_output_cap
 * Pure truncation-decision helpers; the agent/I/O driven recovery phases stay in Python.
 */
#include "noc_w7_turntruncat_79d683.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w7_turntruncat_repetition_copy(const char *stopping, const char *outcome,
                                       const char *refusal) {
    char log[1024], user[2048], err[1024];
    if (!stopping) stopping = "";
    if (!outcome) outcome = "";
    if (!refusal) refusal = "";
    snprintf(log, sizeof(log),
             "🔁 Response dominated by repeated text — stopping %s.", stopping);
    snprintf(user, sizeof(user),
             "⚠️ **Response Stopped — Repetition Detected**\n\n"
             "The model fell into a repetition loop while writing this response, %s\n\n"
             "→ Switch to a different model with `/model`\n"
             "→ Or resend your message (your conversation history is preserved)",
             outcome);
    snprintf(err, sizeof(err),
             "Model output entered a repetition loop%s degenerate response.", refusal);
    json_t *a = json_array();
    json_append(a, json_string(log));
    json_append(a, json_string(user));
    json_append(a, json_string(err));
    return a;
}

json_t *w7_turntruncat_partial_result(const char *final_response, const char *api_calls_str,
                                      bool has_error, const char *error, bool failed,
                                      bool compression_exhausted) {
    if (!final_response) final_response = "";
    long long calls = api_calls_str ? strtoll(api_calls_str, NULL, 10) : 0;
    json_t *r = json_object();
    json_set(r, "final_response", json_string(final_response));
    json_set(r, "messages", json_array());
    json_set(r, "api_calls", json_number((double)calls));
    json_set(r, "completed", json_bool(false));
    json_set(r, failed ? "failed" : "partial", json_bool(true));
    json_set(r, "error", json_string(has_error && error ? error : final_response));
    if (compression_exhausted)
        json_set(r, "compression_exhausted", json_bool(true));
    return r;
}

long long w7_turntruncat_boosted_output_cap(const char *requested_cap_str, bool has_requested_cap,
                                            const char *n_str, const char *max_tokens_str,
                                            const char *api_mode, bool has_model_limit,
                                            const char *model_limit_str, bool has_base,
                                            const char *base_str) {
    long long req = (has_requested_cap && requested_cap_str) ? strtoll(requested_cap_str, NULL, 10) : 0;
    long long mt  = max_tokens_str ? strtoll(max_tokens_str, NULL, 10) : 0;
    long long n   = n_str ? strtoll(n_str, NULL, 10) : 0;
    long long base = has_base && base_str ? strtoll(base_str, NULL, 10)
                                          : ((mt != 0) ? mt : ((req != 0) ? req : 4096));
    long long anchor = (req != 0) ? req : base;
    long long limit = 0;
    if (api_mode && !strcmp(api_mode, "anthropic_messages") && has_model_limit && model_limit_str)
        limit = strtoll(model_limit_str, NULL, 10);

    if (limit && anchor >= limit) return anchor;

    /* Python base * (2 ** n); keep n bounded like real retry counts (1..4). */
    long long p = 1;
    if (n > 62) n = 62;
    if (n > 0) for (long long i = 0; i < n; i++) p *= 2;
    long long boost = base * p;
    if (boost < req) boost = req;             /* max(..., requested_cap or 0) */
    long long ceiling = (anchor * 2 > 32768) ? anchor * 2 : 32768;
    if (boost > ceiling) boost = ceiling;
    if (limit && boost > limit) boost = limit;
    return boost;
}
