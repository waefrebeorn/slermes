/*
 * noc_w25_register_bdae35.h - C surface for upstream
 * plugins/platforms/signal/register.py (gate w25_register_bdae35).
 *
 * The value core of Signal's plugin registration: the "group:<id>" target-ref
 * parser (with its NOT_EXPLICIT sentinel), the configured/connected predicate
 * over config.extra + scoped env, the cached platform hint, and the literal
 * PlatformEntry table register() hands the plugin system.  The adapter, the
 * setup flow and the gateway facade are seams.
 */
#ifndef NOC_W25_REGISTER_BDAE35_H
#define NOC_W25_REGISTER_BDAE35_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"ref": <str>} -> ["group:<id>", null] | "NOT_EXPLICIT" | null. */
json_t *w25_register_bdae35_parse_send_target(const json_t *req);

/* {"extra": {"http_url","account"}, "env": {"SIGNAL_HTTP_URL","SIGNAL_ACCOUNT"}}
 * -> bool: validate_signal_config() or _env_configured(). */
json_t *w25_register_bdae35_is_connected(const json_t *req);

/* The cached _PLATFORM_HINT bytes. */
json_t *w25_register_bdae35_platform_hint(void);

/* The literal PlatformEntry.register_platform() kwargs, as an object. */
json_t *w25_register_bdae35_register_spec(void);

#ifdef __cplusplus
}
#endif
#endif
