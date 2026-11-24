#ifndef NOC_W16_METHODSCOM_D1B360_H
#define NOC_W16_METHODSCOM_D1B360_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_methodscom_d1b360_item @ tui_gateway/methods_complete.py:_item */
struct json_t *w16_methodscom_d1b360_item(const struct json_t *text,
                                           const struct json_t *meta,
                                           const struct json_t *display);
/* PoP: w16_methodscom_d1b360_at_hints @ tui_gateway/methods_complete.py:_AT_DIRECTIVE_HINTS */
struct json_t *w16_methodscom_d1b360_at_hints(void);
/* PoP: w16_methodscom_d1b360_builtin_prefixes @ tui_gateway/methods_complete.py:_BUILTIN_AT_PREFIXES */
struct json_t *w16_methodscom_d1b360_builtin_prefixes(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_METHODSCOM_D1B360_H */
