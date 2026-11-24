/*
 * noc_w17_sessionbus_30eb49.h — public C API for the faithful C11 port of
 * upstream Hermes gateway/session_busy_controls.py (gate w17_sessionbus_30eb49).
 */
#ifndef NOC_W17_SESSIONBUS_30EB49_H
#define NOC_W17_SESSIONBUS_30EB49_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessionbus_30eb49_handlers @ gateway/session_busy_controls.py:handlers */
json_t *w17_sessionbus_30eb49_handlers(void);
/* PoP: w17_sessionbus_30eb49_authorize @ gateway/session_busy_controls.py:authorize */
json_t *w17_sessionbus_30eb49_authorize(json_t *connection, const char *sid, json_t *params,
        const char *capability, json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_verbose_config @ gateway/session_busy_controls.py:verbose_config */
json_t *w17_sessionbus_30eb49_verbose_config(json_t *connection, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_show_reasoning_config @ gateway/session_busy_controls.py:show_reasoning_config */
json_t *w17_sessionbus_30eb49_show_reasoning_config(json_t *connection, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_service_tier_config @ gateway/session_busy_controls.py:service_tier_config */
json_t *w17_sessionbus_30eb49_service_tier_config(json_t *connection, const char *sid, json_t *params,
        json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_yolo_config @ gateway/session_busy_controls.py:yolo_config */
json_t *w17_sessionbus_30eb49_yolo_config(json_t *connection, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_adopts_owner_agent @ gateway/session_busy_controls.py:adopts_owner_agent */
json_t *w17_sessionbus_30eb49_adopts_owner_agent(json_t *authority, const char *sid, json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_correct @ gateway/session_busy_controls.py:correct */
json_t *w17_sessionbus_30eb49_correct(json_t *connection, const char *sid, json_t *params,
        const char *verb, json_t *io, json_t *state);
/* PoP: w17_sessionbus_30eb49_busy_config @ gateway/session_busy_controls.py:busy_config */
json_t *w17_sessionbus_30eb49_busy_config(json_t *connection, const char *sid, json_t *params,
        bool write, json_t *io, json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W17_SESSIONBUS_30EB49_H */
