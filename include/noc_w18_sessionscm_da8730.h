/*
 * noc_w18_sessionscm_da8730.h -- public C API for the deterministic row
 * helpers of hermes_cli/sessions_cmd_browse.py (wave18 gate w18_sessionscm_da8730).
 */
#ifndef NOC_W18_SESSIONSCM_DA8730_H
#define NOC_W18_SESSIONSCM_DA8730_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionscm_da8730_status_tag @ hermes_cli/sessions_cmd_browse.py:_session_status_tag */
char *w18_sessionscm_da8730_status_tag(const char *status);

/* PoP: w18_sessionscm_da8730_label @ hermes_cli/sessions_cmd_browse.py:_label */
char *w18_sessionscm_da8730_label(json_t *session);

/* PoP: w18_sessionscm_da8730_clip @ hermes_cli/sessions_cmd_browse.py:_clip */
char *w18_sessionscm_da8730_clip(const char *text, int n);

/* PoP: w18_sessionscm_da8730_msgs_str @ hermes_cli/sessions_cmd_browse.py:_msgs_str */
char *w18_sessionscm_da8730_msgs_str(json_t *session);

/* PoP: w18_sessionscm_da8730_match @ hermes_cli/sessions_cmd_browse.py:_match */
bool w18_sessionscm_da8730_match(json_t *session, const char *query);

/* All char* returns are malloc'd; caller frees. */
#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SESSIONSCM_DA8730_H */
