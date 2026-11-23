#ifndef NOC_W8_RESTARTNOTI_29F67A_H
#define NOC_W8_RESTARTNOTI_29F67A_H

/*
 * Public C API for the port of gateway/restart_notice.py (gate w8_restartnoti_29f67a).
 * All exported symbols are restartnoti_-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/* PoP: restartnoti_planned_restart_marker_path @ gateway/restart_notice.py:planned_restart_marker_path
 * NULL `home` means get_process_hermes_home(), injected as `default_home`.
 * Path joining follows WindowsPath semantics ('/' normalised to '\'). */
json_t *restartnoti_planned_restart_marker_path(const char *home, const char *default_home);

/* PoP: restartnoti_marker_payload @ gateway/restart_notice.py:write_planned_restart_marker
 * The JSON body the writer stamps: {"requested_at": now, "via_service": bool, "detached": bool}. */
json_t *restartnoti_marker_payload(double now, int via_service, int detached);

#endif /* NOC_W8_RESTARTNOTI_29F67A_H */
