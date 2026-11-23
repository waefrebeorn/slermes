/* Public C API for the tools/set_theme_tool.py parity port. */
#ifndef NOC_W7_SETTEMETO_EE2735_H
#define NOC_W7_SETTEMETO_EE2735_H
#include "hermes_json.h"

/* Both return an already-decoded JSON value (the live Python returns
 * json.dumps(...) strings; the oracle json.loads() those strings). */
json_t *w7_setthemeto_ee2735_accent_error(const char *accent);
json_t *w7_setthemeto_ee2735_set_theme_tool(json_t *accent, json_t *mode, json_t *layout,
                                            json_t *picker, int callback);
#endif /* NOC_W7_SETTEMETO_EE2735_H */
