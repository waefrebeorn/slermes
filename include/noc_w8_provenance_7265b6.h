#ifndef NOC_W8_PROVENANCE_7265B6_H
#define NOC_W8_PROVENANCE_7265B6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_provenance_7265b6_build @ acp_adapter/provenance.py:build_session_provenance
 * Injectable persistence seam: `sessions` is a JSON object mapping session id
 * to {"parent_session_id": str|null, "end_reason": str|null}.  `previous` may be
 * NULL/empty (absent).  Returns a provenance object or JSON null. */
struct json_t *w8_provenance_7265b6_build(const struct json_t *sessions,
                                          const char *acp_id,
                                          const char *current_id,
                                          const char *previous);

/* PoP: w8_provenance_7265b6_meta @ acp_adapter/provenance.py:session_provenance_meta
 * Wraps build() into {"hermes": {"sessionProvenance": ...}} or returns null. */
struct json_t *w8_provenance_7265b6_meta(const struct json_t *sessions,
                                         const char *acp_id,
                                         const char *current_id,
                                         const char *previous);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_PROVENANCE_7265B6_H */
