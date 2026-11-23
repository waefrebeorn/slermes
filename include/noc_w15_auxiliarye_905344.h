#ifndef NOC_W15_AUXILIARYE_905344_H
#define NOC_W15_AUXILIARYE_905344_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Verdict JSON: {reason, retryable, should_fallback, should_rotate_credential,
 * status_code, error_context:{...}}. Failure JSON: {verdict, rejects_parameter}. */

/* PoP: w15_auxiliarye_905344_same_provider_retry @ agent/auxiliary_error_policy.py:same_provider_retry */
int w15_auxiliarye_905344_same_provider_retry(const struct json_t *verdict);
/* PoP: w15_auxiliarye_905344_auth_refresh @ agent/auxiliary_error_policy.py:auth_refresh */
int w15_auxiliarye_905344_auth_refresh(const struct json_t *verdict);
/* PoP: w15_auxiliarye_905344_same_key_retry @ agent/auxiliary_error_policy.py:same_key_retry */
int w15_auxiliarye_905344_same_key_retry(const struct json_t *verdict);
/* PoP: w15_auxiliarye_905344_resend_cannot_help @ agent/auxiliary_error_policy.py:resend_cannot_help */
int w15_auxiliarye_905344_resend_cannot_help(const struct json_t *verdict);
/* PoP: w15_auxiliarye_905344_benches_provider @ agent/auxiliary_error_policy.py:benches_provider */
int w15_auxiliarye_905344_benches_provider(const struct json_t *verdict);
/* PoP: w15_auxiliarye_905344_stream_negotiation_reraises @ agent/auxiliary_error_policy.py:stream_negotiation_reraises */
int w15_auxiliarye_905344_stream_negotiation_reraises(const struct json_t *verdict);
/* PoP: w15_auxiliarye_905344_fallback_label @ agent/auxiliary_error_policy.py:fallback_label
 * Returns a JSON string, or null for Python None. Caller frees. */
struct json_t *w15_auxiliarye_905344_fallback_label(const struct json_t *failure, int rung_spent);
/* PoP: w15_auxiliarye_905344_ladder_accepts @ agent/auxiliary_error_policy.py:ladder_accepts */
int w15_auxiliarye_905344_ladder_accepts(const struct json_t *failure, int rung_spent);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_AUXILIARYE_905344_H */
