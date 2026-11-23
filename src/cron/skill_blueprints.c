/* skill_blueprints.c — C home for upstream Hermes cron/skill_blueprints.py.
 *
 * PoP: w7_skillbluepr_blueprint_to_job_spec @ cron/skill_blueprints.py:blueprint_to_job_spec
 * A blueprint is an ordinary skill; this bridge builds the create_job kwargs exactly as
 * the Python dataclass-to-dict projection does (frontmatter/YAML and cron I/O stay out).
 */
#include "noc_w7_skillbluepr_859fa5.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

json_t *w7_skillbluepr_blueprint_to_job_spec(const char *skill_name, const char *schedule,
                                             const char *deliver,
                                             bool has_prompt, const char *prompt,
                                             bool no_agent,
                                             bool has_model, const char *model,
                                             bool has_provider, const char *provider,
                                             const json_t *enabled_toolsets,
                                             bool has_name, const char *name) {
    if (!skill_name) skill_name = "";
    if (!schedule) schedule = "";
    if (!deliver) deliver = "";
    json_t *r = json_object();

    json_set(r, "prompt", has_prompt && prompt ? json_string(prompt) : json_null());
    json_set(r, "schedule", json_string(schedule));

    char default_name[1024];
    if (has_name && name && name[0]) {
        json_set(r, "name", json_string(name));
    } else {
        snprintf(default_name, sizeof(default_name), "blueprint:%s", skill_name);
        json_set(r, "name", json_string(default_name));
    }

    json_set(r, "deliver", json_string(deliver));
    json_t *skills = skill_name[0] ? json_array() : json_null();
    if (skills) json_append(skills, json_string(skill_name));
    json_set(r, "skills", skills);
    json_set(r, "model", has_model && model ? json_string(model) : json_null());
    json_set(r, "provider", has_provider && provider ? json_string(provider) : json_null());
    json_set(r, "enabled_toolsets",
             (enabled_toolsets && enabled_toolsets->type == JSON_ARRAY)
                 ? json_copy(enabled_toolsets) : json_null());
    json_set(r, "no_agent", json_bool(no_agent));
    return r;
}
