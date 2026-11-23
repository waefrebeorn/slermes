#ifndef NOC_W14_SESSIONLLM_2068E9_H
#define NOC_W14_SESSIONLLM_2068E9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sessionllm_2068e9_validate @ gateway/session_llm_oneshot.py:_validate
 *
 * Faithful port of the module's deterministic parameter-validation core.
 * `params` is the JSON object of request parameters. `templates` is a JSON
 * array of the currently registered template names (fixture-injected seam for
 * agent.oneshot.PROMPT_TEMPLATES). Returns a JSON object: {"ok":true} on
 * success, otherwise {"ok":false,"reason":"invalid_params",
 * "field":<string|null>,"detail":<string|null>}. Caller frees. */
struct json_t *w14_sessionllm_2068e9_validate(const struct json_t *params,
                                              const struct json_t *templates);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONLLM_2068E9_H */
