/*
 * noc_w23_cuabackend_bf7126.h - C surface for upstream
 * tools/computer_use/cua_backend_capture.py (gate w23_cuabackend_bf7126).
 *
 * The driver transport is a seam: `list_windows` / `list_apps` payloads and the
 * X11 `_NET_ACTIVE_WINDOW` probe arrive as the fixture's snapshot.  This
 * surface owns window discovery and capture-target selection -- the off-screen
 * and self-window filters, the Linux real-app filter, the shared-z_index
 * fallback to the active window, payload normalisation and the base64/PNG/JPEG
 * metric sniff -- plus the `app=` resolution tiers and the list_apps payload
 * ladder.
 */
#ifndef NOC_W23_CUABACKEND_BF7126_H
#define NOC_W23_CUABACKEND_BF7126_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The module's windows are never taller than this many rows in a fixture. */
#define W23_CUABACKEND_BF7126_MAX_ROWS 4096

/* _is_cua_driver_self_window(w): the authorization daemon's own window
 * (app name normalised by dropping whitespace / '_' / '-'). */
int w23_cuabackend_bf7126_self_window(const json_t *w);

/* _is_desktop_window(w, names): app_name+title (lower-cased) contains one of
 * the shell names.  backdrop=1 selects _DESKTOP_BACKDROP_NAMES instead. */
int w23_cuabackend_bf7126_desktop_window(const json_t *w, int backdrop);

/* _select_capture_target: req keys windows (list_windows rows, z-sorted),
 * app_requested, exact_target, platform ("linux" | "darwin" | "win32"),
 * active_id (the _NET_ACTIVE_WINDOW probe result, null when unavailable).
 * Owned copy of the chosen window, or JSON null for an empty list. */
json_t *w23_cuabackend_bf7126_select_target(const json_t *req);

/* _sorted_windows(out): normalised rows, z_index DESCENDING.  `out` is the
 * list_windows tool result (structuredContent.windows / data.windows /
 * _legacy_windows / windows).  Owned array. */
json_t *w23_cuabackend_bf7126_sorted_windows(const json_t *out);

/* _png_metrics: req keys png_b64 (str), width, height -> [bytes, width,
 * height]; the sniffed size wins when the bytes carry a PNG/JPEG header. */
json_t *w23_cuabackend_bf7126_png_metrics(const json_t *req);

/* _match_windows_for_app: req keys windows, app, apps (what list_apps would
 * return; the enumeration seam).  Exact names, then exact list_apps aliases,
 * then substrings, then nameless-title rows.  Owned array. */
json_t *w23_cuabackend_bf7126_match_windows(const json_t *req);

/* list_apps(): the candidate ladder over one list_apps payload --
 * structuredContent.apps, data, data.apps, out.apps, apps derived from
 * windows -- then the "(name) (pid N)" text fallback.  Owned array. */
json_t *w23_cuabackend_bf7126_list_apps(const json_t *payload);

/* app.strip().lower() in the whole-screen / desktop-shell sentinel sets. */
int w23_cuabackend_bf7126_is_full_screen(const json_t *app);
int w23_cuabackend_bf7126_is_desktop_shell(const json_t *app);

/* The capture-refusal prose, with `{app!r}` filled in.  Caller frees. */
char *w23_cuabackend_bf7126_no_app_match_message(const json_t *app);
char *w23_cuabackend_bf7126_no_desktop_message(const json_t *app);

/* The sentinel / shell-name catalogues and the three refusal notes. */
json_t *w23_cuabackend_bf7126_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_CUABACKEND_BF7126_H */
