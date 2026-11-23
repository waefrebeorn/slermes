#ifndef NOC_W10_SKILLSTOOL_52FA6B_H
#define NOC_W10_SKILLSTOOL_52FA6B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stateful skill_view dedup of tools/skills_tool_dedup.py.
 * dedup_skill_view returns JSON null for a full view, or a JSON string holding
 * the exact "unchanged" stub produced by json.dumps(..., ensure_ascii=False).
 * reset_skill_view_dedup clears one task (task_id given) or all (null). */
json_t *w10_skillstool_52fa6b_dedup_skill_view(const char *task_id, const char *name,
        const char *file_path, const json_t *payload, const char *result);
json_t *w10_skillstool_52fa6b_reset_skill_view_dedup(const char *task_id, int task_given);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SKILLSTOOL_52FA6B_H */
