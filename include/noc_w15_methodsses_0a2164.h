#ifndef NOC_W15_METHODSSES_0A2164_H
#define NOC_W15_METHODSSES_0A2164_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_methodsses_0a2164_str_param @ tui_gateway/methods_session.py:_str_param */
struct json_t *w15_methodsses_0a2164_str_param(const struct json_t *params,
                                                const char *key, const char *def);
/* PoP: w15_methodsses_0a2164_int_param @ tui_gateway/methods_session.py:_int_param */
struct json_t *w15_methodsses_0a2164_int_param(const struct json_t *params,
                                                const char *key, long long def);
/* PoP: w15_methodsses_0a2164_denied_source @ tui_gateway/methods_session.py:_denied_source */
struct json_t *w15_methodsses_0a2164_denied_source(const struct json_t *row);
/* PoP: w15_methodsses_0a2164_auto_resume_denied_source @ tui_gateway/methods_session.py:_auto_resume_denied_source */
struct json_t *w15_methodsses_0a2164_auto_resume_denied_source(const struct json_t *row);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_METHODSSES_0A2164_H */
