#ifndef NOC_W6_MEDIAFETCH_D0155C_H
#define NOC_W6_MEDIAFETCH_D0155C_H
#include <stddef.h>

/* C home for upstream Hermes gateway/media_fetch.py (gate w6_mediafetch_d0155c).
 * The fetch itself is environment I/O; the pre-flight path screen is pure. */

/* PoP: w6_mediafetch_remote_path_is_denied @ gateway/media_fetch.py:remote_path_is_denied
 * Applies the host denylist to a sandbox path. remote_home == NULL/"" means
 * "unknown", which conservatively matches home-relative entries at ANY
 * component. Returns 1 when denied, 0 when allowed. */
int w6_mediafetch_remote_path_is_denied(const char *path, const char *remote_home);

#endif /* NOC_W6_MEDIAFETCH_D0155C_H */
