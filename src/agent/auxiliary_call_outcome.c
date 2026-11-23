/*
 * auxiliary_call_outcome.c — C home for upstream Hermes agent/auxiliary_call_outcome.py.
 *
 * PoP: w15_auxiliaryc_f6bead_embedded_provider_error @ agent/auxiliary_call_outcome.py:embedded_provider_error
 * PoP: w15_auxiliaryc_f6bead_is_cancellation @ agent/auxiliary_call_outcome.py:is_cancellation
 * PoP: w15_auxiliaryc_f6bead_error_class @ agent/auxiliary_call_outcome.py:error_class
 *
 * The lazy `classify_api_error` and the exception *type* are external to the
 * wire format; the oracle injects the classifier reason and the kind name so
 * the C side can reproduce the module's own branches exactly.
 */
#include "noc_w15_auxiliaryc_f6bead.h"
#include "hermes_json.h"

#include <stdio.h>
#include <string.h>

static const char *const PROVIDER_ERROR_ATTR = "provider_error";

json_t *w15_auxiliaryc_f6bead_embedded_provider_error(const struct json_t *response,
                                                      int error_obj_mode,
                                                      const char *error_message,
                                                      const char *error_code) {
    (void)PROVIDER_ERROR_ATTR;
    if (error_obj_mode) {
        /* getattr(error, "message"/"code", None) path: keep present (non-None) fields. */
        json_t *o = json_object();
        int any = 0;
        if (error_message) { json_set(o, "message", json_string(error_message)); any = 1; }
        if (error_code) { json_set(o, "code", json_string(error_code)); any = 1; }
        if (!any) { json_free(o); return json_null(); }
        return o;
    }
    const json_t *r = (const json_t *)response;
    if (!r || r->type != JSON_OBJECT) return json_null();
    const json_t *err = json_obj_get(r, "error");
    if (err && err->type == JSON_OBJECT) {
        if (json_object_size(err) == 0) return json_null();  /* `error or None` */
        return json_copy(err);
    }
    return json_null();
}

int w15_auxiliaryc_f6bead_is_cancellation(const char *kind) {
    if (!kind) return 0;
    if (!strcmp(kind, "base") || !strcmp(kind, "keyboard") ||
        !strcmp(kind, "system_exit") || !strcmp(kind, "interrupted")) return 1;
    return 0;
}

json_t *w15_auxiliaryc_f6bead_error_class(const char *kind, const char *stub_reason) {
    if (!kind || !strcmp(kind, "base") || !strcmp(kind, "keyboard") ||
        !strcmp(kind, "system_exit")) {
        return json_string("unknown");
    }
    if (!stub_reason || !stub_reason[0]) return json_string("unknown");
    return json_string(stub_reason);
}
