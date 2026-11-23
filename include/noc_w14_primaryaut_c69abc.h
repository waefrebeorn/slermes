#ifndef NOC_W14_PRIMERAUT_C69ABC_H
#define NOC_W14_PRIMERAUT_C69ABC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_primaryaut_c69abc_route_snapshot @ agent/primary_auth_fallback.py:route_snapshot
 * JSON array [provider, base_url, api_key] (JSON null for absent keys). */
struct json_t *w14_primaryaut_c69abc_route_snapshot(const struct json_t *runtime_kwargs);

/* PoP: w14_primaryaut_c69abc_failure_for_route @ agent/primary_auth_fallback.py:failure_for_route
 * Returns a copy of `failure` when the current route snapshot equals `resolved`, else JSON null. */
struct json_t *w14_primaryaut_c69abc_failure_for_route(const struct json_t *failure,
        const struct json_t *resolved, const struct json_t *runtime_kwargs);

/* PoP: w14_primaryaut_c69abc_primary_auth_lead @ agent/primary_auth_fallback.py:primary_auth_lead
 * `failure` is an object with provider/error/relogin/backup string fields.
 * provider_label_for, short_detail and redact_sensitive_text are injected as
 * deterministic seams: "provider:"+p, str(e)[:n], and identity respectively. */
const char *w14_primaryaut_c69abc_primary_auth_lead(const struct json_t *failure,
        const char *backup, const struct json_t *failure_reason, const struct json_t *error);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_PRIMERAUT_C69ABC_H */
