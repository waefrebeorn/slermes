/*
 * noc_w21_webwhatsap_8973fb.h -- public C API for the C11 port of upstream Hermes
 *   hermes_cli/web_whatsapp_session.py
 *
 * WhatsApp session re-pairing guards: refuse to touch the session while the
 * bridge may still own it (pid + process start-time fingerprint), refuse a
 * symbolic link, then rename the session aside into a fresh timestamped backup
 * directory and recreate it 0700.
 *
 * The filesystem, the process table and the clock are host seams. The C port
 * receives them as injected state -- the same values the Python code reads
 * back from pathlib, gateway.status and datetime.now(UTC) -- and reproduces the
 * 409/500 HTTPException projection (status + detail) plus the ordered fs
 * operations the success path performs.
 *
 * Every exported symbol carries the w21_webwhatsap_8973fb_ prefix.
 */
#ifndef NOC_W21_WEBWHATSAP_8973FB_H
#define NOC_W21_WEBWHATSAP_8973FB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * _assert_whatsapp_bridge_stopped(). state:
 *   pid_file_exists(bool), pid_text(str), read_raises(bool),
 *   pid_exists(bool), current_start(number|null)
 * -> {"raised":bool,"status":int|null,"detail":str|null}
 */
json_t *w21_webwhatsap_8973fb_bridge_check(const json_t *state);

/*
 * _backup_whatsapp_session(). state adds:
 *   is_symlink(bool), exists(bool),
 *   backup_dir(str)       -- tempfile.mkdtemp(prefix="session-backup-<stamp>-", dir=parent)
 *   rename_error(str|null), mkdir_error(str|null)
 * -> {"raised":bool,"status":int|null,"detail":str|null,"actions":[str]}
 * actions are "rename" and "mkdir" in the order the live code performs them.
 */
json_t *w21_webwhatsap_8973fb_backup(const json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_WEBWHATSAP_8973FB_H */
