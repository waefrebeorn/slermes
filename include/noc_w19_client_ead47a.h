#ifndef NOC_W19_CLIENT_EAD47A_H
#define NOC_W19_CLIENT_EAD47A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_client_ead47a_bare_model_id @ agent/gemini_native_adapter.py:bare_gemini_model_id
 * Strip Gemini's own provider prefix ("google/", "gemini/") from an aggregator-style id.
 * Returns a pointer to an internal rotating buffer; copy it before the next call. */
const char *w19_client_ead47a_bare_model_id(const char *model);

/* PoP: w19_client_ead47a_uses_thinking_level @ agent/gemini_native_adapter.py:gemini_uses_thinking_level
 * Gemini 3+ (version gate) and the "gemini-*-latest" aliases steer thinking with thinkingLevel. */
int w19_client_ead47a_uses_thinking_level(const char *model);

/* PoP: w19_client_ead47a_configured_api @ plugins/model-providers/gemini/client.py:_configured_api
 * "interactions" | "generate_content" | "auto" for this model. `config_ok` is 0 when
 * load_config_readonly() raised (the default routing then applies); `config` is the
 * decoded config mapping. Per-model setting wins over the provider-level one. */
const char *w19_client_ead47a_configured_api(const char *model, const char *bare,
                                             int config_ok, struct json_t *config);

/* PoP: w19_client_ead47a_uses_interactions @ plugins/model-providers/gemini/client.py:uses_interactions
 * The AI Studio host only; an explicit choice wins, "auto" asks the version gate. */
int w19_client_ead47a_uses_interactions(const char *model, const char *base_url,
                                        int config_ok, struct json_t *config);

/* PoP: w19_client_ead47a_continues @ plugins/model-providers/gemini/client.py:GeminiClient._continues
 * A response asking for another hop: status, a non-empty id and a non-empty string token. */
int w19_client_ead47a_continues(struct json_t *interaction);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CLIENT_EAD47A_H */
