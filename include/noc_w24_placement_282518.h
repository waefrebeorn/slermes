/*
 * noc_w24_placement_282518.h - C surface for upstream
 * tools/bot_desktop/placement.py (gate w24_placement_282518).
 *
 * Where this profile's desktop lives: the gateway host, inside the configured
 * terminal backend, or refused.  ``resolve()`` is pure config + backend-class
 * reasoning, so that is exactly what is ported: the settings normalisation,
 * the local-is-the-gateway shortcut, the stream-capable backend list, the
 * plugin-host isolation refusal and both refusal sentences.
 *
 * Seams: the config load, the terminal env config and the plugin flags are
 * arguments here; no container, no sandbox and no environment object is ever
 * acquired (``terminal_environment()`` is not ported at all).
 */
#ifndef W24_PLACEMENT_282518_H
#define W24_PLACEMENT_282518_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* backend: terminal.backend (str|null), setting: bot_desktop.placement as
 * configured (any), plugin_streams: the backend's supports_streams flag (only
 * a real true counts, as `is True` does upstream), host_isolated: whether
 * plugins run in the isolated host.
 * -> {"where": "gateway"|"terminal"|"refused", "backend": str, "reason": str}. */
json_t *w24_placement_282518_resolve(const json_t *backend, const json_t *setting,
                                     const json_t *plugin_streams,
                                     const json_t *host_isolated);

/* The literal placement names and the stream-capable backend list. */
json_t *w24_placement_282518_constants(void);

#ifdef __cplusplus
}
#endif
#endif
