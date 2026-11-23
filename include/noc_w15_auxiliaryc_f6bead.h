#ifndef NOC_W15_AUXILIARYC_F6BEAD_H
#define NOC_W15_AUXILIARYC_F6BEAD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_auxiliaryc_f6bead_embedded_provider_error @ agent/auxiliary_call_outcome.py:embedded_provider_error
 * Returns the embedded error object (or null). `error_obj_mode` selects the
 * attribute-object path with the optional message/code strings (NULL = absent). Caller frees. */
struct json_t *w15_auxiliaryc_f6bead_embedded_provider_error(const struct json_t *response,
                                                             int error_obj_mode,
                                                             const char *error_message,
                                                             const char *error_code);

/* PoP: w15_auxiliaryc_f6bead_is_cancellation @ agent/auxiliary_call_outcome.py:is_cancellation
 * `kind` names the exception type: base/keyboard/system_exit/interrupted => true. */
int w15_auxiliaryc_f6bead_is_cancellation(const char *kind);

/* PoP: w15_auxiliaryc_f6bead_error_class @ agent/auxiliary_call_outcome.py:error_class
 * Non-Exception kinds => "unknown"; otherwise the classifier's reason value. Caller frees. */
struct json_t *w15_auxiliaryc_f6bead_error_class(const char *kind, const char *stub_reason);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_AUXILIARYC_F6BEAD_H */
