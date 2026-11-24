#ifndef NOC_W17_DESKTOPBUI_731B67_H
#define NOC_W17_DESKTOPBUI_731B67_H

#include "hermes_json.h"

/* Faithful C11 port of the checkout-key derivation in
 * hermes_cli/desktop_build_lock.py (gate w17_desktopbui_731b67).
 * The upstream __init__ keys the lock by
 *   sha256(os.path.normcase(str(Path(root).resolve(strict=False))).encode())[:24]
 * which on Windows resolves to a backslash-separated lower-case path. */
json_t *w17_desktopbui_731b67_checkout_key(const char *project_root);
json_t *w17_desktopbui_731b67_lock_name(const char *project_root);

#endif /* NOC_W17_DESKTOPBUI_731B67_H */
