/*
 * noc_w25_profilesse_1c3aa6.h - C surface for upstream
 * hermes_cli/profiles_service_cleanup.py (gate w25_profilesse_1c3aa6).
 *
 * The pure decision core of profile delete/rename cleanup: whether a service
 * definition belongs to the profile being removed (or to another install), and
 * exactly which notices a systemd unit or a Windows Scheduled Task removal
 * leaves behind.  The seams -- pinned_home() reading the unit/plist, Path
 * resolution, geteuid(), systemctl/schtasks and the startup folder -- stay in
 * the caller; this file owns only what the values decide.
 */
#ifndef NOC_W25_PROFILESSE_1C3AA6_H
#define NOC_W25_PROFILESSE_1C3AA6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* gateway._SYSTEM_UNIT_DIR and the unit name suffix. */
#define W25_PROFILESSE_1C3AA6_SYSTEM_UNIT_DIR "/etc/systemd/system"
#define W25_PROFILESSE_1C3AA6_UNIT_SUFFIX ".service"
/* the systemctl call timeout, and gateway_windows._SCHTASKS_TIMEOUT_S */
#define W25_PROFILESSE_1C3AA6_RUN_TIMEOUT_S 30.0
#define W25_PROFILESSE_1C3AA6_SCHTASKS_TIMEOUT_S 15.0

/* {"definition": <string>, "profile_dir": <string>, "raw": <string|null>,
 *  "resolved_raw": <string|null>, "resolved_profile": <string|null>} ->
 * {"owned": bool, "notice": <string|null>}. */
json_t *w25_profilesse_1c3aa6_owned_by_profile(const json_t *req);

/* {"svc_name", "unit", "exists", "owned", "is_root", "stop_rc"} ->
 * {"removed": bool, "messages": [<string>...]}. */
json_t *w25_profilesse_1c3aa6_system_unit(const json_t *req);

/* {"task_name", "registered", "delete_code", "err",
 *  "entries": [[<path>, <exists>], ...]} -> {"removed": bool, "messages": [...]}. */
json_t *w25_profilesse_1c3aa6_windows_task(const json_t *req);

/* The layout/timeout literal tables, as an object. */
json_t *w25_profilesse_1c3aa6_constants(void);

#ifdef __cplusplus
}
#endif
#endif
