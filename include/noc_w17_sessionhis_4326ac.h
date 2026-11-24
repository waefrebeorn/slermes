/*
 * noc_w17_sessionhis_4326ac.h — public C API for the faithful C11 port of
 * upstream Hermes gateway/session_history_mount.py (gate w17_sessionhis_4326ac).
 */
#ifndef NOC_W17_SESSIONHIS_4326AC_H
#define NOC_W17_SESSIONHIS_4326AC_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessionhis_4326ac_reader @ gateway/session_history_mount.py:_reader */
json_t *w17_sessionhis_4326ac_reader(json_t *authority, json_t *actor, const char *sid,
        json_t *io, json_t *state);
/* PoP: w17_sessionhis_4326ac_mount_history @ gateway/session_history_mount.py:mount_history */
json_t *w17_sessionhis_4326ac_mount_history(json_t *authority, json_t *actor, const char *sid,
        json_t *io, json_t *state);
/* PoP: w17_sessionhis_4326ac_is_history_mount @ gateway/session_history_mount.py:is_history_mount */
json_t *w17_sessionhis_4326ac_is_history_mount(json_t *live);
/* PoP: w17_sessionhis_4326ac_require_history_leaf @ gateway/session_history_mount.py:require_history_leaf */
json_t *w17_sessionhis_4326ac_require_history_leaf(json_t *authority, const char *sid,
        json_t *io, json_t *state);
/* PoP: w17_sessionhis_4326ac_read_only @ gateway/session_history_mount.py:read_only */
json_t *w17_sessionhis_4326ac_read_only(void);
/* PoP: w17_sessionhis_4326ac_require_history_access @ gateway/session_history_mount.py:require_history_access */
json_t *w17_sessionhis_4326ac_require_history_access(json_t *authority, json_t *actor,
        const char *sid, const char *capability, json_t *io, json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W17_SESSIONHIS_4326AC_H */
