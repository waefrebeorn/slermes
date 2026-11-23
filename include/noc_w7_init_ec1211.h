/* Public C API for the tui_gateway/__init__.py parity port. */
#ifndef NOC_W7_INIT_EC1211_H
#define NOC_W7_INIT_EC1211_H
#include "hermes_json.h"

/* The live module is an empty package initializer (zero bytes). Its entire
 * observable behaviour is an empty public surface and no module docstring. */
json_t *w7_init_ec1211_public_names(void);
json_t *w7_init_ec1211_has_attr(const char *name);
json_t *w7_init_ec1211_module_doc(void);
#endif /* NOC_W7_INIT_EC1211_H */
