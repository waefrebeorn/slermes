#ifndef NOC_W12_BOTMODEDm_EFCB4F_H
#define NOC_W12_BOTMODEDm_EFCB4F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_botmodedm_efcb4f_outcome_line @ tools/bot_mode_dm_outcome.py:outcome_line
 * entry: JSON object {status, reply?, error?, detail?, target_state?}. Returns malloc'd string. */
char *w12_botmodedm_efcb4f_outcome_line(const char *to, const struct json_t *entry);

/* PoP: w12_botmodedm_efcb4f_combined_summary @ tools/bot_mode_dm_outcome.py:combined_summary
 * entries: JSON array of entry objects; returns malloc'd joined summary. */
char *w12_botmodedm_efcb4f_combined_summary(const struct json_t *entries);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_BOTMODEDm_EFCB4F_H */
