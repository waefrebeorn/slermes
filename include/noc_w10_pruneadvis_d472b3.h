#ifndef NOC_W10_PRUNEADVIS_D472B3_H
#define NOC_W10_PRUNEADVIS_D472B3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_pruneadvis_d472b3_low @ hermes_cli/prune_advisor.py:DiskSnapshot.low */
json_t *w10_pruneadvis_d472b3_low(const json_t *db_bytes, const json_t *free_bytes, const json_t *total_bytes);

/* PoP: w10_pruneadvis_d472b3_retention_days @ hermes_cli/prune_advisor.py:_retention_days */
json_t *w10_pruneadvis_d472b3_retention_days(const json_t *sessions);

/* PoP: w10_pruneadvis_d472b3_gb @ hermes_cli/prune_advisor.py:_gb */
json_t *w10_pruneadvis_d472b3_gb(const json_t *n);

/* PoP: w10_pruneadvis_d472b3_sessions @ hermes_cli/prune_advisor.py:_sessions */
json_t *w10_pruneadvis_d472b3_sessions(const json_t *n);

/* PoP: w10_pruneadvis_d472b3_helps @ hermes_cli/prune_advisor.py:Advice.helps */
json_t *w10_pruneadvis_d472b3_helps(const json_t *db_bytes, const json_t *free_bytes,
                                   const json_t *total_bytes, const json_t *reclaim_bytes,
                                   const json_t *prunable_sessions);

/* PoP: w10_pruneadvis_d472b3_advice_lines @ hermes_cli/prune_advisor.py:advice_lines */
json_t *w10_pruneadvis_d472b3_advice_lines(const json_t *advice, const char *command_prefix);

/* PoP: w10_pruneadvis_d472b3_notice_payload @ hermes_cli/prune_advisor.py:notice_payload */
json_t *w10_pruneadvis_d472b3_notice_payload(const json_t *advice);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_PRUNEADVIS_D472B3_H */
