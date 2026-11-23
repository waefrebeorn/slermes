#ifndef NOC_W9_SESSIONSURF_B02F14_H
#define NOC_W9_SESSIONSURF_B02F14_H
#include "hermes_json.h"

/* PoP port of gateway/session_surface.py — per-turn surface admission. */
json_t *w9_sessionsurf_b02f14_submit_surface_fields(json_t *params);
json_t *w9_sessionsurf_b02f14_admit_surface(json_t *params);
json_t *w9_sessionsurf_b02f14_restore_surface(json_t *committed);
#endif /* NOC_W9_SESSIONSURF_B02F14_H */
