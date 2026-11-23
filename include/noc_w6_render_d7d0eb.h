#ifndef NOC_W6_RENDER_D7D0EB_H
#define NOC_W6_RENDER_D7D0EB_H

#include "hermes_json.h"

/* Public C API for the port of tui_gateway/render.py (gate w6_render_d7d0eb).
 * live._rich(name, *args, cols=cols) calls agent.rich_output.<name>(*args,
 * cols=cols), retries without cols on TypeError, and returns None when the
 * module/renderer is missing or the renderer raises. The runtime facts that
 * live in Python (module/attribute presence, renderer behaviour) are injected
 * by the caller from the fixture — see PORT-KIT env/seam guidance.
 *
 * behavior: "ok" | "typeerror" | "error" | "nomodule" | "noattr"
 * Returns a JSON string (the renderer's return value) or JSON null. Caller frees.
 */
struct json_t *tuirnd_invoke(const char *name,
                             const char *text, int has_text,
                             double cols, int module_present, int attr_present,
                             const char *behavior, const char *sentinel);

struct json_t *tuirnd_render_message(const char *text, double cols,
                                     int module_present, int attr_present,
                                     const char *behavior, const char *sentinel);

struct json_t *tuirnd_render_diff(const char *text, double cols,
                                  int module_present, int attr_present,
                                  const char *behavior, const char *sentinel);

struct json_t *tuirnd_make_stream_renderer(double cols,
                                           int module_present, int attr_present,
                                           const char *behavior, const char *sentinel);

#endif /* NOC_W6_RENDER_D7D0EB_H */
