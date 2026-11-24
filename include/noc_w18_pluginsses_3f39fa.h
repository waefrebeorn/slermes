/*
 * noc_w18_pluginsses_3f39fa.h -- public C API for the deterministic name
 * handling in hermes_cli/plugins_session_env.py (wave18 gate w18_pluginsses_3f39fa).
 */
#ifndef NOC_W18_PLUGINSSES_3F39FA_H
#define NOC_W18_PLUGINSSES_3F39FA_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_pluginsses_3f39fa_env_key @ hermes_cli/plugins_session_env.py:_env_key
 * Windows (os.name == "nt") child-env key: name.upper(). Caller frees. */
char *w18_pluginsses_3f39fa_env_key(const char *name);

/* PoP: w18_pluginsses_3f39fa_name_valid @ hermes_cli/plugins_session_env.py:register_session_env
 * The registration name gate: isinstance(str) and fullmatch
 * [A-Za-z_][A-Za-z0-9_]{0,127} and not name.upper().startswith("HERMES_"). */
bool w18_pluginsses_3f39fa_name_valid(json_t *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_PLUGINSSES_3F39FA_H */
