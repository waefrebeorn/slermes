#ifndef NOC_W8_REASONINGPA_062303_H
#define NOC_W8_REASONINGPA_062303_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_reasoningpa_062303_should_sanitize_tool_calls @ agent/reasoning_params.py:ReasoningParamsMixin._should_sanitize_tool_calls */
int w8_reasoningpa_062303_should_sanitize_tool_calls(const struct json_t *api_mode);

/* PoP: w8_reasoningpa_062303_reasoning_echo_opt_in @ agent/reasoning_params.py:ReasoningParamsMixin._reasoning_echo_opt_in */
int w8_reasoningpa_062303_reasoning_echo_opt_in(const struct json_t *flag);

/* PoP: w8_reasoningpa_062303_needs_kimi_tool_reasoning @ agent/reasoning_params.py:ReasoningParamsMixin._needs_kimi_tool_reasoning */
int w8_reasoningpa_062303_needs_kimi_tool_reasoning(const struct json_t *args);

/* PoP: w8_reasoningpa_062303_needs_deepseek_tool_reasoning @ agent/reasoning_params.py:ReasoningParamsMixin._needs_deepseek_tool_reasoning */
int w8_reasoningpa_062303_needs_deepseek_tool_reasoning(const struct json_t *args);

/* PoP: w8_reasoningpa_062303_needs_mimo_tool_reasoning @ agent/reasoning_params.py:ReasoningParamsMixin._needs_mimo_tool_reasoning */
int w8_reasoningpa_062303_needs_mimo_tool_reasoning(const struct json_t *args);

/* PoP: w8_reasoningpa_062303_sanitize_tool_calls_for_strict_api @ agent/reasoning_params.py:ReasoningParamsMixin._sanitize_tool_calls_for_strict_api
 * keep_signature is the injected _model_consumes_thought_signature result. */
struct json_t *w8_reasoningpa_062303_sanitize_tool_calls_for_strict_api(
    const struct json_t *api_msg, int keep_signature);

/* PoP: w8_reasoningpa_062303_supports_reasoning_extra_body @ agent/reasoning_params.py:ReasoningParamsMixin._supports_reasoning_extra_body
 * All external collaborators are injected via args (matches, profile,
 * ask_profile_value, lmstudio_options, ollama_thinking, is_openrouter, caps). */
int w8_reasoningpa_062303_supports_reasoning_extra_body(const struct json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_REASONINGPA_062303_H */
