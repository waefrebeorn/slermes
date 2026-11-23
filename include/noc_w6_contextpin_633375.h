#ifndef NOC_W6_CONTEXTPIN_633375_H
#define NOC_W6_CONTEXTPIN_633375_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Python-ish scalar kinds used by the port to mirror isinstance() checks.
 * W6_CP_INT=0 (decimal string), W6_CP_BOOL=1 ("true"/"false"),
 * W6_CP_FLOAT=2 (decimal), W6_CP_NONE=3 (ignored), W6_CP_STR=4 (literal). */
enum {
    W6_CP_INT = 0,
    W6_CP_BOOL = 1,
    W6_CP_FLOAT = 2,
    W6_CP_NONE = 3,
    W6_CP_STR = 4
};

/* PoP: w6_contextpin_633375_is_context_pinned @ agent/context_pin.py:is_context_pinned
 * True when the shown window equals a positive int model.context_length pin. */
int w6_contextpin_633375_is_context_pinned(int context_kind, const char *context_value,
                                           int config_kind, const char *config_value);

/* PoP: w6_contextpin_633375_context_pin_suffix @ agent/context_pin.py:context_pin_suffix */
const char *w6_contextpin_633375_context_pin_suffix(int context_kind, const char *context_value,
                                                    int config_kind, const char *config_value);

/* PoP: w6_contextpin_633375_context_window_marker @ agent/context_pin.py:context_window_marker
 * Fallback needs the live agent.context_default_record DB; fixtures use an empty model
 * so the fallback branch is False by the function's own early guard. */
const char *w6_contextpin_633375_context_window_marker(int context_kind, const char *context_value,
                                                       int config_kind, const char *config_value,
                                                       const char *model);

/* PoP: w6_contextpin_633375_context_window_suffix @ agent/context_pin.py:context_window_suffix */
const char *w6_contextpin_633375_context_window_suffix(int context_kind, const char *context_value,
                                                       int config_kind, const char *config_value,
                                                       const char *model);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_CONTEXTPIN_633375_H */
