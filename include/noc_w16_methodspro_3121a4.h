#ifndef NOC_W16_METHODSPRO_3121A4_H
#define NOC_W16_METHODSPRO_3121A4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_methodspro_3121a4_pick @ tui_gateway/methods_projects.py:_pick */
struct json_t *w16_methodspro_3121a4_pick(const struct json_t *params,
                                           const struct json_t *keys);
/* PoP: w16_methodspro_3121a4_messaging_sources @ tui_gateway/methods_projects.py:_MESSAGING_SESSION_SOURCES */
struct json_t *w16_methodspro_3121a4_messaging_sources(void);
/* PoP: w16_methodspro_3121a4_tree_excluded_sources @ tui_gateway/methods_projects.py:_PROJECT_TREE_EXCLUDED_SOURCES */
struct json_t *w16_methodspro_3121a4_tree_excluded_sources(void);
/* PoP: w16_methodspro_3121a4_error_codes @ tui_gateway/methods_projects.py:_E_PROJECTS/_E_NO_PROJECT/_E_PROJECT_ARG */
struct json_t *w16_methodspro_3121a4_error_codes(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_METHODSPRO_3121A4_H */
