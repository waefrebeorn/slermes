#ifndef NOC_W20_WINDOWSAPP_53B288_H
#define NOC_W20_WINDOWSAPP_53B288_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/windows_appinstaller_update.py (gate w20_windowsapp_53b288).
 *
 * The module is a thin probe over the WinRT projection; the port keeps its
 * decision table and payload shapes and takes the projection's collisions in
 * as a script instead of importing winrt:
 *   spec = {"import_error": str|null,
 *           "identity": {"winerror": int|null, "str": str}|null,   # null = packaged
 *           "source": {"uri": str}|null,                           # null = none registered
 *           "check_error": str|null,
 *           "availability": int}
 * Answer: {"exit": int, "payload": {...}} -- exactly the json.dumps object
 * main() prints, plus the exit code it returns.
 */

/* PoP: w20_windowsapp_53b288_run @ hermes_cli/windows_appinstaller_update.py:main
 * 0 = no update / not packaged, 2 = update available, 1 = unknown or error.
 * Only NO_UPDATES is a "no"; UNKNOWN and ERROR stay unknown. */
json_t *w20_windowsapp_53b288_run(const json_t *spec);

/* PoP: w20_windowsapp_53b288_availability_name @ hermes_cli/windows_appinstaller_update.py:main
 * The PackageUpdateAvailability member name for 0..4, or NULL for anything the
 * enum would reject (Python raises ValueError there). */
const char *w20_windowsapp_53b288_availability_name(double value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_WINDOWSAPP_53B288_H */
