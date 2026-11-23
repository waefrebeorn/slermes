#ifndef NOC_W14_REDACTSECR_02F95A_H
#define NOC_W14_REDACTSECR_02F95A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_redactsecr_02f95a_pem_texts @ agent/redact_secret_sources.py:_pem_texts
 * Every PEM-bearing text in `value`: itself when armored, plus strings found inside
 * a JSON-encoded value (including \\n-unescaped duplicates). JSON array of strings. */
struct json_t *w14_redactsecr_02f95a_pem_texts(const char *value);

/* PoP: w14_redactsecr_02f95a_multiline_fragments @ agent/redact_secret_sources.py:_multiline_fragments
 * Joined multiline form (when long enough) plus PEM body lines of base64 shape. */
struct json_t *w14_redactsecr_02f95a_multiline_fragments(const char *value);

/* PoP: w14_redactsecr_02f95a_redact_exact @ agent/redact_secret_sources.py:redact_secret_source_values
 * Exact-substring scrub. `values` is the already-classified registered-value tuple
 * (longest first); the per-home registry and hermes_home_key() are injected seams. */
const char *w14_redactsecr_02f95a_redact_exact(const char *text, const struct json_t *values);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_REDACTSECR_02F95A_H */
