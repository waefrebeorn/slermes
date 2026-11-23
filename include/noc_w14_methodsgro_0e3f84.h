#ifndef NOC_W14_METHODSGRO_0E3F84_H
#define NOC_W14_METHODSGRO_0E3F84_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_methodsgro_0e3f84_grant_expiry @ tui_gateway/methods_groups.py:_grant_expiry */
struct json_t *w14_methodsgro_0e3f84_grant_expiry(const struct json_t *claims);
/* PoP: w14_methodsgro_0e3f84_include_disbanded @ tui_gateway/methods_groups.py:_include_disbanded */
struct json_t *w14_methodsgro_0e3f84_include_disbanded(const struct json_t *params);
/* PoP: w14_methodsgro_0e3f84_methods @ tui_gateway/methods_groups.py:_METHODS */
struct json_t *w14_methodsgro_0e3f84_methods(void);
/* PoP: w14_methodsgro_0e3f84_long_handlers @ tui_gateway/methods_groups.py:LONG_HANDLERS */
struct json_t *w14_methodsgro_0e3f84_long_handlers(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_METHODSGRO_0E3F84_H */
