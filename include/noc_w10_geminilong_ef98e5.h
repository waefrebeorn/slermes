#ifndef NOC_W10_GEMINILONG_EF98E5_H
#define NOC_W10_GEMINILONG_EF98E5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_geminilong_ef98e5_long_decode_max_output_tokens @ agent/gemini_long_decode.py:long_decode_max_output_tokens
 * Returns the total output budget for a Gemini 4 reasoning effort, or -1 for None. */
long long w10_geminilong_ef98e5_long_decode_max_output_tokens(const char *model, const struct json_t *config);

/* PoP: w10_geminilong_ef98e5_long_decode_hops @ agent/gemini_long_decode.py:long_decode_hops */
long long w10_geminilong_ef98e5_long_decode_hops(const struct json_t *max_output_tokens);

/* PoP: w10_geminilong_ef98e5_long_decode_stale_multiplier @ agent/gemini_long_decode.py:long_decode_stale_multiplier */
long long w10_geminilong_ef98e5_long_decode_stale_multiplier(const struct json_t *api_payload);

/* PoP: w10_geminilong_ef98e5_continuation_token @ agent/gemini_long_decode.py:continuation_token
 * Malloc'd token string, or NULL when the hop ended on any other finish reason. */
char *w10_geminilong_ef98e5_continuation_token(const struct json_t *payload);

/* PoP: w10_geminilong_ef98e5_merge_hop_usage @ agent/gemini_long_decode.py:merge_hop_usage
 * Sums every usage count across hops; caller frees the returned object. */
struct json_t *w10_geminilong_ef98e5_merge_hop_usage(const struct json_t *hops);

/* PoP: w10_geminilong_ef98e5_merge_hop_responses @ agent/gemini_long_decode.py:merge_hop_responses
 * Joins every hop's parts into one model-turn response; caller frees. */
struct json_t *w10_geminilong_ef98e5_merge_hop_responses(const struct json_t *hops);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_GEMINILONG_EF98E5_H */
