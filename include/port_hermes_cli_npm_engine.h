/*
 * port_hermes_cli_npm_engine.h — C11 port of hermes_cli/npm_engine.py
 */
#ifndef PORT_HERMES_CLI_NPM_ENGINE_H
#define PORT_HERMES_CLI_NPM_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: is_ebadengine @ hermes_cli/npm_engine.py:is_ebadengine */
bool ne_is_ebadengine(const char *output);

/* PoP: _iter_required_blocks @ hermes_cli/npm_engine.py:_iter_required_blocks */
/* Scan output for "Required: {json}" blocks. Returns JSON array of dicts. */
json_t *ne_iter_required_blocks(const char *output);

/* PoP: required_npm_range @ hermes_cli/npm_engine.py:required_npm_range */
char *ne_required_npm_range(const char *output);

/* PoP: actual_npm_version @ hermes_cli/npm_engine.py:actual_npm_version */
char *ne_actual_npm_version(const char *output);

/* PoP: managed_npm_prefix @ hermes_cli/npm_engine.py:managed_npm_prefix */
char *ne_managed_npm_prefix(const char *npm);

/* PoP: _upgrade_env @ hermes_cli/npm_engine.py:_upgrade_env */
char *ne_upgrade_env(void);

/* PoP: upgrade_managed_npm @ hermes_cli/npm_engine.py:upgrade_managed_npm */
bool ne_upgrade_managed_npm(const char *npm, const char *npm_range,
                            const char *prefix, bool quiet);

/* PoP: _probe_version @ hermes_cli/npm_engine.py:_probe_version */
char *ne_probe_version(const char *npm);

/* PoP: _print_manual_fix @ hermes_cli/npm_engine.py:_print_manual_fix */
void ne_print_manual_fix(const char *npm, const char *npm_range, const char *actual);

/* PoP: _provision_managed_npm @ hermes_cli/npm_engine.py:_provision_managed_npm */
char *ne_provision_managed_npm(const char *npm_range, bool quiet);

/* PoP: maybe_repair_npm_engine @ hermes_cli/npm_engine.py:maybe_repair_npm_engine */
char *ne_maybe_repair_npm_engine(const char *npm, const char *output, bool quiet);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_NPM_ENGINE_H */
