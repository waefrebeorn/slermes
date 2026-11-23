#ifndef NOC_W9_SESSIONDISP_B6CFF2_H
#define NOC_W9_SESSIONDISP_B6CFF2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_sessiondisp_b6cff2_submit_fields @ gateway/session_display.py:submit_display_fields */
struct json_t *w9_sessiondisp_b6cff2_submit_fields(const struct json_t *params);

/* PoP: w9_sessiondisp_b6cff2_admit @ gateway/session_display.py:admit_display */
struct json_t *w9_sessiondisp_b6cff2_admit(const struct json_t *params);

/* PoP: w9_sessiondisp_b6cff2_restore @ gateway/session_display.py:restore_display */
struct json_t *w9_sessiondisp_b6cff2_restore(const struct json_t *committed);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_SESSIONDISP_B6CFF2_H */
