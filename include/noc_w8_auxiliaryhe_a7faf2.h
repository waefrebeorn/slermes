#ifndef NOC_W8_AUXILIARYHE_A7FAF2_H
#define NOC_W8_AUXILIARYHE_A7FAF2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_auxiliaryhe_a7faf2_quarantine_ttl @ agent/auxiliary_health.py:fallback_candidate_quarantine_ttl
 * reason may be NULL/empty (Python None/"").  Known transient labels return
 * 60.0; everything else (including None) returns JSON null. */
struct json_t *w8_auxiliaryhe_a7faf2_quarantine_ttl(const char *reason);

/* PoP: w8_auxiliaryhe_a7faf2_custom_health_base_url @ agent/auxiliary_health.py:_custom_health_base_url
 * Injectable seams: `label` = _normalize_chain_label(provider),
 * `current_custom` = _current_custom_base_url(), `resolves` = _resolves_to_custom(label),
 * `named_entry` = _get_named_custom_provider(provider) (object or null). */
struct json_t *w8_auxiliaryhe_a7faf2_custom_health_base_url(
    const char *provider, const char *explicit, const char *label,
    const char *current_custom, int resolves, const struct json_t *named_entry);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_AUXILIARYHE_A7FAF2_H */
