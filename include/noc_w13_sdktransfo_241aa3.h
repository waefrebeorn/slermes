#ifndef NOC_W13_SDKTRANSFO_241AA3_H
#define NOC_W13_SDKTRANSFO_241AA3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_sdktransfo_241aa3_is_plain_json_data @ agent/sdk_transform_bypass.py:_is_plain_json_data
 * True when value consists purely of JSON wire types. */
int w13_sdktransfo_241aa3_is_plain_json_data(const struct json_t *value);

/* PoP: w13_sdktransfo_241aa3_bypass @ agent/sdk_transform_bypass.py:bypass_sdk_request_transform
 * Move wire-format bulk fields into extra_body. `fields` is an array of field
 * names (NULL => the module's RESPONSES_BYPASS_FIELDS = ["input","tools"]);
 * `env_value` is the injected HERMES_CODEX_SDK_TRANSFORM value (escape hatch). */
struct json_t *w13_sdktransfo_241aa3_bypass(const struct json_t *request_kwargs,
                                           const struct json_t *fields, int keep_slots,
                                           const char *env_value);

/* PoP: w13_sdktransfo_241aa3_bypass_chat @ agent/sdk_transform_bypass.py:bypass_chat_sdk_request_transform
 * Chat-completions bypass gated on the real OpenAI SDK (`is_openai` models
 * type(completions).__module__.startswith("openai.")). Uses CHAT_COMPLETIONS_BYPASS_FIELDS
 * = ["messages","tools"] and keep_slots=True. */
struct json_t *w13_sdktransfo_241aa3_bypass_chat(const struct json_t *request_kwargs,
                                                int is_openai, const char *env_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SDKTRANSFO_241AA3_H */
