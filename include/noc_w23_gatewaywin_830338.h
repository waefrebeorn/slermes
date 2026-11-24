/*
 * noc_w23_gatewaywin_830338.h - C surface for upstream
 * hermes_cli/gateway_windows_legacy.py (gate w23_gatewaywin_830338).
 *
 * Pre-suffix Windows gateway launchers: the Scheduled Task `Hermes_Gateway`,
 * the Startup `Hermes_Gateway.vbs`/`.cmd` and the `gateway-service\
 * Hermes_Gateway.{vbs,cmd}` pair written before launcher names carried a
 * profile suffix. Every other gateway operation is keyed on the suffixed
 * `get_task_name()`, so those strays are never queried, rewritten or removed:
 * they keep starting a second gateway at logon. This module enumerates them
 * once; `status` warns, `uninstall` / `install --force` remove.
 *
 * Seams (the upstream module reads them off `hermes_cli.gateway_windows`, the
 * documented facade binding, and off `pathlib.Path`):
 *   - `_TASK_NAME_DEFAULT`, `get_task_name()`, `_hermes_home()`,
 *     `_startup_dir()`, `_normalize_windows_path()` answer from `world`;
 *   - the virtual file system is `world.files` (normalized path -> text,
 *     read as utf-8-sig), with `world.unreadable` naming files whose read
 *     raises OSError and `world.unlink_fail` naming files whose unlink raises
 *     OSError(message);
 *   - `_exec_schtasks()` answers `world.schtasks` {code,out,err} and
 *     `_is_access_denied()` answers `world.access_denied`.
 */
#ifndef NOC_W23_GATEWAYWIN_830338_H
#define NOC_W23_GATEWAYWIN_830338_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _TASK_NAME_DEFAULT: the bare identity the default profile owns. */
#define W23_GATEWAYWIN_830338_TASK_NAME_DEFAULT "Hermes_Gateway"

/* PoP: w23_gatewaywin_830338_normalize_path @ hermes_cli/gateway_windows.py:_normalize_windows_path
 * `value.strip().lstrip('"').replace("\\", "/").rstrip("/").casefold()`. Caller frees. */
char *w23_gatewaywin_830338_normalize_path(const char *value);

/* The `Path / part` join the module performs (base without its trailing
 * separator, one "/" between). Caller frees. */
char *w23_gatewaywin_830338_join_path(const char *base, const char *part);

/* PoP: w23_gatewaywin_830338_targets_this_home @ hermes_cli/gateway_windows_legacy.py:_targets_this_home
 * A bare-named launcher is OURS only when its action points into THIS home's
 * `gateway-service` dir (the bare name is also the default profile's live
 * identity). 1 when the normalized text contains the normalized marker. */
int w23_gatewaywin_830338_targets_this_home(const char *text, const char *hermes_home);

/* PoP: w23_gatewaywin_830338_artifacts @ hermes_cli/gateway_windows_legacy.py:legacy_launcher_artifacts
 * `[[kind, label, target], ...]` for every pre-suffix launcher of THIS home
 * still present, in the module's order: startup .vbs, startup .cmd,
 * gateway-service .vbs, gateway-service .cmd, then the Scheduled Task.
 * Empty when this home owns the bare name. */
json_t *w23_gatewaywin_830338_artifacts(const json_t *world);

/* PoP: w23_gatewaywin_830338_warn @ hermes_cli/gateway_windows_legacy.py:warn_legacy_launchers
 * `hermes gateway status`: name every stray.
 *   {"result":bool, "lines":[...]} */
json_t *w23_gatewaywin_830338_warn(const json_t *world);

/* PoP: w23_gatewaywin_830338_remove @ hermes_cli/gateway_windows_legacy.py:remove_legacy_launchers
 * `uninstall` / `install --force`: delete the strays; a task that needs
 * elevation is named with the exact elevated command.
 *   {"lines":[...], "remaining":[normalized path, ...] (sorted)} */
json_t *w23_gatewaywin_830338_remove(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_GATEWAYWIN_830338_H */
