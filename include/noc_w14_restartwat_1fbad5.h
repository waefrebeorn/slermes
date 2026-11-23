#ifndef NOC_W14_RESTARTWAT_1FBAD5_H
#define NOC_W14_RESTARTWAT_1FBAD5_H

#include "hermes_json.h"

/* C port of gateway/restart_watcher_env.py (gate w14_restartwat_1fbad5).
 *
 * The live module reads the running checkout root and HERMES_HOME from the
 * process/filesystem; both are injected as explicit args so the POSIX shell
 * line and the PYTHONPATH rewrite replay deterministically. os.pathsep is ';'
 * on the Windows host the live code runs on.
 *
 * Both functions return a NEW json_t* string value.
 */
json_t *w14_restartwat_1fbad5_detached_restart_shell(const char *cmd,
                                                     const char *current_pid,
                                                     double restart_after_s,
                                                     const char *log_path);
json_t *w14_restartwat_1fbad5_keep_importable(const char *pythonpath,
                                              const char *root);

#endif /* NOC_W14_RESTARTWAT_1FBAD5_H */
