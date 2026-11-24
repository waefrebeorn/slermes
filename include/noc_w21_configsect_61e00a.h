#ifndef NOC_W21_CONFIGSECT_61E00A_H
#define NOC_W21_CONFIGSECT_61E00A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/config_section_guard.py (gate w21_configsect_61e00a).
 *
 * `hermes config set` refuses a single-segment key that would replace a whole
 * configuration section with a scalar, redirects the bare `model` shorthand to
 * `model.default`, and lets `--force` through. The port returns everything the
 * live function's two observable effects carry: the exit code and the text each
 * stream received.
 */

/* PoP: w21_configsect_61e00a_guard @ hermes_cli/config_section_guard.py:_guard_section_overwrite
 * `_guard_section_overwrite(key, value, user_config, force)` with the
 * `_exit_invalid` sink folded in (the config/CLI seam):
 *   {"status":"ok","key":<returned key>,"stdout":<...>,"stderr":<...>}
 *   {"status":"exit","code":<n>,"stdout":<...>,"stderr":<...>}
 * `force` is Python truthiness. NULL user_config reads as {}; a NULL key as "". */
json_t *w21_configsect_61e00a_guard(const char *key, const json_t *value,
                                    const json_t *user_config, int force);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_CONFIGSECT_61E00A_H */
