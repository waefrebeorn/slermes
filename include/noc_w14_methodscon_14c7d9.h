#ifndef NOC_W14_METHODSCON_14C7D9_H
#define NOC_W14_METHODSCON_14C7D9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_methodscon_14c7d9_safe_client_label @ tui_gateway/methods_config.py:_safe_client_label */
struct json_t *w14_methodscon_14c7d9_safe_client_label(const struct json_t *label);
/* PoP: w14_methodscon_14c7d9_display_word @ tui_gateway/methods_config.py:_display_word
 * display_raw is the injected _display_raw() mapping seam. */
struct json_t *w14_methodscon_14c7d9_display_word(const struct json_t *key,
                                                  const struct json_t *default_value,
                                                  const struct json_t *allowed,
                                                  const struct json_t *display_raw);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_METHODSCON_14C7D9_H */
