/*
 * noc_w24_sudopasswo_63ce58.h - C surface for upstream
 * tools/environments/sudo_password_note.py (gate w24_sudopasswo_63ce58).
 *
 * The advisory a sudo password being handed to a non-local terminal backend
 * earns.  Ported here: the backend name derivation (a plugin's stamped name,
 * else a snake_case of the environment class), the once-per-backend-class
 * decision, and the exact warning text.  The locks, the logger and the
 * process-wide ``_noted_backend_types`` set are seams: the caller owns the
 * once-ness, this file answers "would the FIRST send on this backend log,
 * and with what text".
 *
 * An ``env`` is a value descriptor:
 *   {"class_name": <str>, "backend_name": <any>, "is_local": <any>}
 * A missing/None env is Python None (class name "NoneType"); a non-object env
 * is its Python type name, which is what ``type(env).__name__`` would yield.
 */
#ifndef W24_SUDOPASSWO_63CE58_H
#define W24_SUDOPASSWO_63CE58_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The backend name the user configured: the stamped plugin name, else
 * ``DockerEnvironment`` -> ``docker``, ``SSHEnvironment`` -> ``ssh``. */
json_t *w24_sudopasswo_63ce58_backend_label(const json_t *env);

/* {"logged": bool, "message": str|null}: whether the first send on this
 * backend class logs, and the exact warning text (never the password). */
json_t *w24_sudopasswo_63ce58_note(const json_t *env, const json_t *sudo_stdin);

/* The literal regex/constants this module is tuned with. */
json_t *w24_sudopasswo_63ce58_constants(void);

#ifdef __cplusplus
}
#endif
#endif
