#ifndef NOC_W18_UPDATEPAUS_C43DE1_H
#define NOC_W18_UPDATEPAUS_C43DE1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/update_pause_record.py pure token helpers.
 * PoP: w18_updatepaus_c43de1_without @ hermes_cli/update_pause_record.py:_without
 * PoP: w18_updatepaus_c43de1_has_work @ hermes_cli/update_pause_record.py:_has_work
 * PoP: w18_updatepaus_c43de1_relaunch_keys @ hermes_cli/update_pause_record.py:_relaunch_keys
 */

/* RECORD_STEM = ".hermes-update-paused-gateways" mirrored from the Python source. */
extern const char *w18_updatepaus_c43de1_RECORD_STEM;

/* _without(): copy of token with profiles/unmapped/posix_units entries whose
 * str(pid) is in `pids` (a JSON array of strings) removed. */
json_t *w18_updatepaus_c43de1_without(const json_t *token, const json_t *pids);

/* _has_work(): Python truthiness of the six owed-work fields. */
int w18_updatepaus_c43de1_has_work(const json_t *token);

/* _relaunch_keys(): sorted JSON array of profile:/unmapped:/unit: keys. */
json_t *w18_updatepaus_c43de1_relaunch_keys(const json_t *token);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_UPDATEPAUS_C43DE1_H */
