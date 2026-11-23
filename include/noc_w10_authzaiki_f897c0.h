#ifndef NOC_W10_AUTHZAIKI_F897C0_H
#define NOC_W10_AUTHZAIKI_F897C0_H

/* PoP: w10_authzaiki_f897c0_resolve_kimi_base_url @ hermes_cli/auth_zai_kimi.py:_resolve_kimi_base_url
 * PoP: w10_authzaiki_f897c0_normalize_lmstudio_runtime_base_url @ hermes_cli/auth_zai_kimi.py:_normalize_lmstudio_runtime_base_url */
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Kimi Code / explicit-override base URL resolution.
 * api_key, default_url, env_override are JSON strings (or JSON null for None).
 * Returns a JSON string (or a copy of default_url when no override/prefix applies). */
struct json_t *w10_authzaiki_f897c0_kimi_base(const struct json_t *api_key,
                                              const struct json_t *default_url,
                                              const struct json_t *env_override);

/* LM Studio base-URL normalization: str(v or "").strip().rstrip("/"), strip one
 * of /api/v1, /api, /v1, then append /v1 with an http://127.0.0.1:1234 fallback. */
struct json_t *w10_authzaiki_f897c0_lmstudio_base(const struct json_t *base_url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_AUTHZAIKI_F897C0_H */
