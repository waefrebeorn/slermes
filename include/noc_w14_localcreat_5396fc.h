#ifndef NOC_W14_LOCALCREAT_5396FC_H
#define NOC_W14_LOCALCREAT_5396FC_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_localcreat_5396fc_is_secret_env_name @ session_rules/local_create.py:is_secret_env_name */
struct json_t *w14_localcreat_5396fc_is_secret_env_name(const struct json_t *name);
/* PoP: w14_localcreat_5396fc_travels @ session_rules/local_create.py:_travels */
struct json_t *w14_localcreat_5396fc_travels(const struct json_t *name, const struct json_t *secret);
/* PoP: w14_localcreat_5396fc_kept @ session_rules/local_create.py:_kept */
struct json_t *w14_localcreat_5396fc_kept(const struct json_t *name, const struct json_t *rules);
/* PoP: w14_localcreat_5396fc_normalise_client_env @ session_rules/local_create.py:normalise_client_env */
struct json_t *w14_localcreat_5396fc_normalise_client_env(const struct json_t *value, const struct json_t *rules);
/* PoP: w14_localcreat_5396fc_client_request @ session_rules/local_create.py:client_request */
struct json_t *w14_localcreat_5396fc_client_request(const struct json_t *request_json, const struct json_t *derived);
/* PoP: w14_localcreat_5396fc_same_create_request @ session_rules/local_create.py:same_create_request */
struct json_t *w14_localcreat_5396fc_same_create_request(const struct json_t *saved_json, const struct json_t *retry_json);
/* PoP: w14_localcreat_5396fc_retry_matches @ session_rules/local_create.py:retry_matches */
struct json_t *w14_localcreat_5396fc_retry_matches(const struct json_t *committed, const struct json_t *request);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_LOCALCREAT_5396FC_H */
