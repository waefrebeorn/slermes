#ifndef NOC_W9_VERIFICATION_1C81C6_H
#define NOC_W9_VERIFICATION_1C81C6_H

#include "hermes_json.h"

/* agent/verification_stop.py pure policy helpers. */
int  w9_verification_1c81c6_is_non_code_path(const char *raw);
json_t *w9_verification_1c81c6_format_changed_paths(json_t *paths);
/* env_value == NULL means HERMES_VERIFY_ON_STOP is absent; non-NULL (even "") means set. */
int  w9_verification_1c81c6_verify_on_stop_enabled_ex(const char *env_value,
                                                       json_t *config, int messaging_surface);

#endif /* NOC_W9_VERIFICATION_1C81C6_H */
