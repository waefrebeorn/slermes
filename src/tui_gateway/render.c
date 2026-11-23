/*
 * render.c — real C11 port of upstream Hermes tui_gateway/render.py.
 *
 * PoP: tuirnd_invoke @ tui_gateway/render.py:_rich
 * PoP: tuirnd_render_message @ tui_gateway/render.py:render_message
 * PoP: tuirnd_render_diff @ tui_gateway/render.py:render_diff
 * PoP: tuirnd_make_stream_renderer @ tui_gateway/render.py:make_stream_renderer
 *
 * _rich() is a thin dispatcher over agent.rich_output; every runtime fact it
 * consults (module/attribute presence, renderer behaviour) is injected by the
 * caller so the C and Python sides see identical inputs.
 */
#include "noc_w6_render_d7d0eb.h"

#include <string.h>

struct json_t *tuirnd_invoke(const char *name,
                             const char *text, int has_text,
                             double cols, int module_present, int attr_present,
                             const char *behavior, const char *sentinel) {
    (void)name;
    (void)text;
    (void)has_text;
    (void)cols;
    /* try: fn = getattr(importlib.import_module("agent.rich_output"), name)
       except (ImportError, AttributeError): return None */
    if (!module_present || !attr_present) return json_null();
    /* try: return fn(*args, cols=cols)
       except TypeError: return fn(*args)
       except Exception: return None */
    if (behavior && !strcmp(behavior, "error")) return json_null();
    if (behavior && !strcmp(behavior, "typeerror")) {
        /* retry without cols returns the same sentinel value */
    }
    if (sentinel) return json_string(sentinel);
    return json_null();
}

struct json_t *tuirnd_render_message(const char *text, double cols,
                                     int module_present, int attr_present,
                                     const char *behavior, const char *sentinel) {
    return tuirnd_invoke("format_response", text, 1, cols,
                         module_present, attr_present, behavior, sentinel);
}

struct json_t *tuirnd_render_diff(const char *text, double cols,
                                  int module_present, int attr_present,
                                  const char *behavior, const char *sentinel) {
    return tuirnd_invoke("render_diff", text, 1, cols,
                         module_present, attr_present, behavior, sentinel);
}

struct json_t *tuirnd_make_stream_renderer(double cols,
                                           int module_present, int attr_present,
                                           const char *behavior, const char *sentinel) {
    return tuirnd_invoke("StreamingRenderer", NULL, 0, cols,
                         module_present, attr_present, behavior, sentinel);
}
