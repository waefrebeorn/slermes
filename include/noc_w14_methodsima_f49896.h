#ifndef NOC_W14_METHODSIMA_F49896_H
#define NOC_W14_METHODSIMA_F49896_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_methodsima_f49896_image_generate @ tui_gateway/methods_images.py:image.generate
 * env carries injected backend availability/result (no network in a gate). */
struct json_t *w14_methodsima_f49896_image_generate(const struct json_t *params, const struct json_t *env);
/* PoP: w14_methodsima_f49896_image_render @ tui_gateway/methods_images.py:image.render
 * env carries injected display config and render result. */
struct json_t *w14_methodsima_f49896_image_render(const struct json_t *params, const struct json_t *env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_METHODSIMA_F49896_H */
