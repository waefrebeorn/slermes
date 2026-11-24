/*
 * noc_w25_browserpre_4511a0.h - C surface for upstream
 * hermes_cli/browser_preview_signins.py (gate w25_browserpre_4511a0).
 *
 * The cookie-export decision core behind ``hermes browser export-signins``:
 * which CDP cookie records the Desktop partition can hold faithfully, what the
 * export reports when consent is off / the browser cannot serve / the read
 * fails, and the CLI's exit code.  The CDP websocket, the keychain decryption
 * and the Electron pipe are seams -- they arrive as fixed inputs.
 */
#ifndef NOC_W25_BROWSERPRE_4511A0_H
#define NOC_W25_BROWSERPRE_4511A0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_BROWSERPRE_4511A0_CDP_TIMEOUT_S 20.0
#define W25_BROWSERPRE_4511A0_FIELDS 9

/* One CDP cookie record -> the portable dict, or null for a record the
 * partition cannot hold faithfully (partitioned / nameless / domainless). */
json_t *w25_browserpre_4511a0_portable(const json_t *cookie);

/* {"cdp_url","err","read_error","raw"} -> the export_signins() result dict. */
json_t *w25_browserpre_4511a0_export(const json_t *state);

/* {"json":bool,"isatty":bool,"result":{"status":..}} -> run_export_signins() rc. */
json_t *w25_browserpre_4511a0_run_code(const json_t *state);

/* The module constants, as an object. */
json_t *w25_browserpre_4511a0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
