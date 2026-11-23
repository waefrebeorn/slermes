#ifndef NOC_W10_GUESTHANDO_B805FE_H
#define NOC_W10_GUESTHANDO_B805FE_H
#include "hermes_json.h"

/* PoP port of hermes_cli/guest_handoff.py — free-tier app handoff copy/classification. */
json_t *w10_guesthando_b805fe_classify(int status, json_t *payload);
json_t *w10_guesthando_b805fe_result_lines(json_t *status);
json_t *w10_guesthando_b805fe_status_lines(json_t *status);
#endif /* NOC_W10_GUESTHANDO_B805FE_H */
