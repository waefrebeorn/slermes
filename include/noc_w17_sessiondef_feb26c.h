/*
 * noc_w17_sessiondef_feb26c.h — public C API for the faithful C11 port of
 * upstream Hermes gateway/session_deferred_pick.py (gate w17_sessiondef_feb26c).
 */
#ifndef NOC_W17_SESSIONDEF_FEB26C_H
#define NOC_W17_SESSIONDEF_FEB26C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessiondef_feb26c_defer_pick @ gateway/session_deferred_pick.py:defer_pick */
json_t *w17_sessiondef_feb26c_defer_pick(json_t *authority, json_t *actor, const char *sid,
        json_t *params, json_t *io, json_t *state);
/* PoP: w17_sessiondef_feb26c_reask_on_attach @ gateway/session_deferred_pick.py:reask_on_attach */
json_t *w17_sessiondef_feb26c_reask_on_attach(json_t *authority, const char *sid,
        json_t *live, json_t *io, json_t *state);
/* PoP: w17_sessiondef_feb26c_commit @ gateway/session_deferred_pick.py:_commit */
json_t *w17_sessiondef_feb26c_commit(json_t *authority, const char *sid, json_t *entry,
        const char *operation, json_t *io, json_t *state);
/* PoP: w17_sessiondef_feb26c_commit_level @ gateway/session_deferred_pick.py:_commit_level */
json_t *w17_sessiondef_feb26c_commit_level(json_t *authority, const char *sid, json_t *raw,
        json_t *pick, json_t *io, json_t *state);
/* PoP: w17_sessiondef_feb26c_apply_deferred_pick @ gateway/session_deferred_pick.py:apply_deferred_pick */
json_t *w17_sessiondef_feb26c_apply_deferred_pick(json_t *authority, const char *sid,
        json_t *live, json_t *io, json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W17_SESSIONDEF_FEB26C_H */
