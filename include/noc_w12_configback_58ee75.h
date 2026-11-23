#ifndef NOC_W12_CONFIGBACK_58EE75_H
#define NOC_W12_CONFIGBACK_58EE75_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/config_backups.py (gate w12_configback_58ee75). */

/* backups_dir(config_path) -> the <home>/backups/config directory, POSIX form. */
json_t *w12_configback_58ee75_backups_dir(const char *config);

/* list_config_backups(config_path, reason) restricted to the injected `files`
 * listing (the live function iterates the real directory). Newest (lexically
 * greatest name) first; when `reason` is empty/NULL, no reason filter applies. */
json_t *w12_configback_58ee75_list(const char *config, const char *reason,
                                   const json_t *files);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_CONFIGBACK_58EE75_H */
