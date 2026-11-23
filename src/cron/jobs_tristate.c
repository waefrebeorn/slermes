/*
 * jobs_tristate.c — C port of upstream Hermes cron/jobs_tristate.py.
 * PoP: crontri_normalize_catch_up @ cron/jobs_tristate.py:normalize_catch_up
 * PoP: crontri_normalize_wrap_response @ cron/jobs_tristate.py:normalize_wrap_response
 */
#include "noc_crontri.h"
#include "hermes_json.h"

static int tri(const json_t *v) {
    if (!v || v->type == JSON_NULL) return CRONTRI_NULL;
    if (v->type == JSON_BOOL) return v->bool_val ? CRONTRI_TRUE : CRONTRI_FALSE;
    return CRONTRI_INVALID;
}
int crontri_normalize_catch_up(const json_t *v) { return tri(v); }
int crontri_normalize_wrap_response(const json_t *v) { return tri(v); }
