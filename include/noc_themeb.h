#ifndef NOC_THEMEB_H
#define NOC_THEMEB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: themeb_theme_request_ex @ tui_gateway/theme_bridge.py:theme_request
 * Injects the renderer reply and elapsed time; returns the answer dict (caller frees). */
struct json_t *themeb_theme_request_ex(const struct json_t *payload, const char *answer_json,
                                       const struct json_t *result, double elapsed);
#ifdef __cplusplus
}
#endif
#endif /* NOC_THEMEB_H */
