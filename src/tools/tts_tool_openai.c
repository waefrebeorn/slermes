/*
 * tts_tool_openai.c -- C home for upstream Hermes tools/tts_tool_openai.py
 * (wave8 gate w8_ttstoolope_a2db02).
 *
 * PoP: w8_ttstoolope_a2db02_openai_extra_body @ tools/tts_tool_openai.py:_openai_extra_body
 * PoP: w8_ttstoolope_a2db02_is_response_format_rejection @ tools/tts_tool_openai.py:_is_response_format_rejection
 *
 * Faithful C11 ports of the two side-effect-free helpers that need no network,
 * config or client object.  The injected status/message seam stands in for the
 * exception object of _is_response_format_rejection.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w8_ttstoolope_a2db02.h"

/* Python truthiness for the JSON value types. */
static int w8_tts_py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default:          return 0;
    }
}

json_t *w8_ttstoolope_a2db02_openai_extra_body(const json_t *oai_config) {
    json_t *out = json_object();
    if (!oai_config || oai_config->type != JSON_OBJECT) return out;

    json_t *language = json_obj_get(oai_config, "language");
    if (w8_tts_py_truthy(language))
        json_set(out, "lang_code", json_copy(language));

    json_t *consent = json_obj_get(oai_config, "consent_attestation");
    if (w8_tts_py_truthy(consent))
        json_set(out, "consent_attestation", json_copy(consent));

    return out;
}

json_t *w8_ttstoolope_a2db02_is_response_format_rejection(int status_code,
                                                          const char *message) {
    int ok = 0;
    if ((status_code == 400 || status_code == 422) && message && strstr(message, "response_format"))
        ok = 1;
    return json_bool(ok ? true : false);
}
