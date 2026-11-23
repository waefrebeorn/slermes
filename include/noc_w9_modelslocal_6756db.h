#ifndef NOC_W9_MODELSLOCAL_6756DB_H
#define NOC_W9_MODELSLOCAL_6756DB_H

#ifdef __cplusplus
extern "C" {
#endif

struct json_t;

/* PoP: w9_modelslocal_6756db_normalize_openai_base_url @ hermes_cli/models_local.py:_normalize_openai_base_url */
struct json_t *w9_modelslocal_6756db_normalize_openai_base_url(const char *base_url);
/* PoP: w9_modelslocal_6756db_root_for_ollama_native_api @ hermes_cli/models_local.py:_root_for_ollama_native_api */
struct json_t *w9_modelslocal_6756db_root_for_ollama_native_api(const char *base_url);
/* PoP: w9_modelslocal_6756db_ollama_host_from_env @ hermes_cli/models_local.py:_ollama_host_from_env */
struct json_t *w9_modelslocal_6756db_ollama_host_from_env(const char *env_host);
/* PoP: w9_modelslocal_6756db_parse_ollama_tags @ hermes_cli/models_local.py:_parse_ollama_tags */
struct json_t *w9_modelslocal_6756db_parse_ollama_tags(const struct json_t *payload);
/* PoP: w9_modelslocal_6756db_strip_ollama_cloud_suffix @ hermes_cli/models_local.py:_strip_ollama_cloud_suffix */
struct json_t *w9_modelslocal_6756db_strip_ollama_cloud_suffix(const char *model_id);
/* PoP: w9_modelslocal_6756db_positive_int @ hermes_cli/models_local.py:_positive_int */
struct json_t *w9_modelslocal_6756db_positive_int(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_MODELSLOCAL_6756DB_H */
