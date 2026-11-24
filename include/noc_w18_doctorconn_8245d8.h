#ifndef NOC_W18_DOCTORCONN_8245D8_H
#define NOC_W18_DOCTORCONN_8245D8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_doctorconn_8245d8_row @ hermes_cli/doctor_connectivity.py:_row
 * The color() helper is identity in the oracle, so lines carry the raw glyphs.
 * Returns {"label":..., "lines":[[glyph,label,detail]], "issues":[...]}. */
struct json_t *w18_doctorconn_8245d8_row(const char *name, const char *status,
                                          const char *detail, const struct json_t *label);

/* PoP: w18_doctorconn_8245d8_openrouter_status @ hermes_cli/doctor_connectivity.py:_OPENROUTER_STATUS
 * Returns [detail, issue_or_null] for the status code. */
struct json_t *w18_doctorconn_8245d8_openrouter_status(int status);

/* PoP: w18_doctorconn_8245d8_probe_openrouter @ hermes_cli/doctor_connectivity.py:_probe_openrouter
 * key / error / status are injected seams for os.getenv and httpx.get. */
struct json_t *w18_doctorconn_8245d8_probe_openrouter(const char *key,
                                                       const char *error, int status);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_DOCTORCONN_8245D8_H */
