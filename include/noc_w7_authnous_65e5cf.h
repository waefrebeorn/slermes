#ifndef NOC_W7_AUTHNOUS_65E5CF_H
#define NOC_W7_AUTHNOUS_65E5CF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_authnous_65e5cf_scope_values @ hermes_cli/auth_nous.py:_scope_values
 * Space/comma-separated string (or a nested list of strings) -> sorted set as a
 * JSON array (set order made deterministic for differential testing). */
struct json_t *w7_authnous_65e5cf_scope_values(const struct json_t *raw_scope);

/* PoP: w7_authnous_65e5cf_model_priority @ hermes_cli/auth_nous.py:_model_priority
 * Sort key (rank, mid): opus > pro > sonnet > other. */
struct json_t *w7_authnous_65e5cf_model_priority(const char *mid);

/* PoP: w7_authnous_65e5cf_validate_inference_url @ hermes_cli/auth_nous.py:_validate_nous_inference_url_from_network
 * Portal-returned inference URL accepted only on https + allowlisted host (or
 * the host the operator named in NOUS_INFERENCE_BASE_URL). `override` is the
 * explicit seam for that env value; NULL/"" means unset. Returns string or null. */
struct json_t *w7_authnous_65e5cf_validate_inference_url(const char *url, const char *override);

/* PoP: w7_authnous_65e5cf_portal_host @ hermes_cli/auth_nous.py:_portal_host
 * urlparse(portal_url or DEFAULT_NOUS_PORTAL_URL).hostname or "portal.nousresearch.com". */
struct json_t *w7_authnous_65e5cf_portal_host(const char *portal_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_AUTHNOUS_65E5CF_H */
