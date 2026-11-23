#ifndef NOC_W7_SKILLBLUEPR_859FA5_H
#define NOC_W7_SKILLBLUEPR_859FA5_H
#include "hermes_json.h"
#include <stdbool.h>

/* C home for upstream Hermes cron/skill_blueprints.py (gate w7_skillbluepr_859fa5). */

/* PoP: w7_skillbluepr_blueprint_to_job_spec @ cron/skill_blueprints.py:blueprint_to_job_spec
 * Emits the exact cron.jobs.create_job kwargs dict for a BlueprintSpec:
 *   prompt, schedule, name (name or f"blueprint:{skill_name}"), deliver,
 *   skills ([skill_name] or None), model, provider, enabled_toolsets, no_agent.
 * has_prompt/has_model/has_provider/has_name distinguish None from ""/0. */
json_t *w7_skillbluepr_blueprint_to_job_spec(const char *skill_name, const char *schedule,
                                             const char *deliver,
                                             bool has_prompt, const char *prompt,
                                             bool no_agent,
                                             bool has_model, const char *model,
                                             bool has_provider, const char *provider,
                                             const json_t *enabled_toolsets,
                                             bool has_name, const char *name);

#endif /* NOC_W7_SKILLBLUEPR_859FA5_H */
