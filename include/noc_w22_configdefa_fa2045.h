#ifndef NOC_W22_CONFIGDEFA_FA2045_H
#define NOC_W22_CONFIGDEFA_FA2045_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/config_defaults_auxiliary.py (gate w22_configdefa_fa2045).
 *
 * The module is one pure-data factory: the standard auxiliary-task model block
 * that DEFAULT_CONFIG["auxiliary"] and the MoA slot blocks are built from.
 * There is no I/O, no environment and no clock in it, so the whole module is
 * ported:
 *
 *   _aux(timeout, *, reasoning_effort=True, **extra)
 *       -> {"provider": "auto", "model": "", "base_url": "", "api_key": "",
 *           "timeout": <timeout>, "extra_body": {}}
 *          (+ "reasoning_effort": "" when reasoning_effort is true)
 *          + every **extra key, appended after the standard ones.
 *
 * Call shape: the C entry point takes the same three things the Python call
 * site supplies -- the positional timeout, the keyword-only reasoning_effort
 * flag, and the **extra mapping. A call that binds reasoning_effort twice
 * (the flag *and* an "reasoning_effort" key inside extra) is the Python
 * `TypeError: got multiple values for keyword argument`, and a non-mapping
 * extra is the Python `TypeError: argument after ** must be a mapping`;
 * both report {"ok": false, "error": "TypeError"}.
 */

/* PoP: w22_configdefa_fa2045_aux @ hermes_cli/config_defaults_auxiliary.py:_aux
 * timeout: the value stored under "timeout" (any JSON value; NULL reads as
 * None). extra: the **extra mapping, or NULL/absent for none.
 * -> {"ok": true, "value": {...}} or {"ok": false, "error": "TypeError"}. */
json_t *w22_configdefa_fa2045_aux(const json_t *timeout, int reasoning_effort,
                                  const json_t *extra);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_CONFIGDEFA_FA2045_H */
