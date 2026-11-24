/*
 * noc_w25_windowssto_d94391.h - C surface for upstream
 * hermes_cli/windows_store_update.py (gate w25_windowssto_d94391).
 *
 * The packaged Store helper: every real step (WinRT apartment, StoreContext,
 * the consent window, the async operation's completion wait) is a seam.  What
 * is ported is what those steps decide: the JSON document each invocation
 * prints, and the exit code that tells the desktop whether an update was found
 * (2), the request succeeded (0) or it failed (1).
 */
#ifndef NOC_W25_WINDOWSSTO_D94391_H
#define NOC_W25_WINDOWSSTO_D94391_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* plan: {
 *   mode:            "check" | "download" | "install",
 *   packages:        [full_name, ...]        # updates the context reported
 *   overall_state:   "<StateName>"           # outcome.overall_state.name
 *   completed_state: "<StateName>"           # StorePackageUpdateState.COMPLETED
 *   hwnd:            <int> | null            # desktop window (consent owner)
 *   init_error:      "<text>" | null         # apartment init raised
 *   get_error:       "<text>" | null          # the updates query raised
 *   request_error:   "<text>" | null         # the download/install request raised
 * } -> {"result": <the document main() prints>, "exit": <0|1|2>} */
json_t *w25_windowssto_d94391_main_result(const json_t *plan);

/* The CLI/Store contract as an object: modes, default, --hwnd, and the
 * responsiveness budget (_wait's deadline and message-pump quantum). */
json_t *w25_windowssto_d94391_constants(void);

#ifdef __cplusplus
}
#endif
#endif
