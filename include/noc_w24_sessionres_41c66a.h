/*
 * noc_w24_sessionres_41c66a.h - C surface for upstream
 * hermes_cli/session_reset_retirement.py (gate w24_sessionres_41c66a).
 *
 * Point configs that still declare an idle/daily ``session_reset`` at the
 * plugin that restores it.  Retired-mode detection and the notice text are
 * pure functions of the config mapping; discovering whether the restoring
 * plugin is actually installed is the seam (a plugin scan, not a decision).
 */
#ifndef NOC_W24_SESSIONRES_41C66A_H
#define NOC_W24_SESSIONRES_41C66A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_SESSIONRES_41C66A_TIMED_MODES 3

/* config: <any> -> ["session_reset"|"gateway.session_reset", mode] or null.
 * A timed mode under the top-level block wins over the gateway: form; a
 * non-mapping config, or a block whose mode is not timed, is null. */
json_t *w24_sessionres_41c66a_retired_reset_policy(const json_t *config);

/* path, mode: <str> -> the operator notice, exactly as the f-string renders
 * it (the "move the block" tail appears only for the gateway: form). */
json_t *w24_sessionres_41c66a_format_notice(const json_t *path, const json_t *mode);

/* PLUGIN_NAME and the timed-mode set, as an object. */
json_t *w24_sessionres_41c66a_constants(void);

#ifdef __cplusplus
}
#endif
#endif
