#ifndef NOC_W8_TTSTOOLOPE_A2DB02_H
#define NOC_W8_TTSTOOLOPE_A2DB02_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_ttstoolope_a2db02_openai_extra_body @ tools/tts_tool_openai.py:_openai_extra_body
 * Return the optional extra_body dict for an ``tts.openai`` config:
 * truthy "language" -> "lang_code", truthy "consent_attestation" -> itself.
 * Caller frees the returned json object. */
struct json_t *w8_ttstoolope_a2db02_openai_extra_body(const struct json_t *oai_config);

/* PoP: w8_ttstoolope_a2db02_is_response_format_rejection @ tools/tts_tool_openai.py:_is_response_format_rejection
 * True when status_code is 400 or 422 and the message mentions response_format.
 * A status_code of -1 represents a missing/None status attribute. */
struct json_t *w8_ttstoolope_a2db02_is_response_format_rejection(int status_code,
                                                                const char *message);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_TTSTOOLOPE_A2DB02_H */
