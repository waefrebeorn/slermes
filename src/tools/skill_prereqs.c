/*
 * skill_prereqs.c — Faithful C11 port of pure helpers from
 * tools/skills_tool.py:
 *   - _normalize_prerequisite_values(value)  -> List[str]
 *   - _collect_prerequisite_values(frontmatter) -> (env_vars, commands)
 *   - _normalize_setup_metadata(frontmatter) -> dict
 *
 * The Python helpers operate on arbitrary dict/list/str/Any.  In C we model
 * the frontmatter as a parsed JSON object (libjson) and the "value" of a
 * prerequisite as either a JSON string or JSON array of strings, matching
 * the YAML-frontmatter shapes skills actually carry.
 *
 * Angel-coder note: only the three pure, IO-free helpers are ported here.
 * Env-gated / session-coupled helpers (_is_gateway_surface, _get_terminal_backend_name,
 * _is_env_var_persisted) are left as REAL_GAP — they need env/session state,
 * not a façade.
 */

#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "skill_prereqs.h"

/* PoP: skill_prereqs_normalize_values @ tools/skills_tool.py:_normalize_prerequisite_values */
json_t *skill_prereqs_normalize_values(const json_t *value)
{
    json_t *out = json_array();
    if (!value) return out;
    if (value->type == JSON_STRING) {
        const char *s = value->str_val;
        /* filter items whose str(item).strip() is truthy */
        if (s && strspn(s, " \t\r\n") != strlen(s)) {
            json_append(out, json_string(s));
        }
        return out;
    }
    if (value->type == JSON_ARRAY) {
        size_t n = json_len(value);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(value, i);
            if (!item) continue;
            const char *s = NULL;
            if (item->type == JSON_STRING) s = item->str_val;
            if (s && strspn(s, " \t\r\n") != strlen(s)) {
                json_append(out, json_string(s));
            }
        }
        return out;
    }
    return out;
}

/* PoP: skill_prereqs_collect_values @ tools/skills_tool.py:_collect_prerequisite_values */
void skill_prereqs_collect_values(const json_t *frontmatter,
                                   json_t **out_env_vars, json_t **out_commands)
{
    *out_env_vars = json_array();
    *out_commands = json_array();
    if (!frontmatter || frontmatter->type != JSON_OBJECT) return;
    json_t *prereqs = json_obj_get(frontmatter, "prerequisites");
    if (!prereqs || prereqs->type != JSON_OBJECT) return;
    json_t *env = json_obj_get(prereqs, "env_vars");
    json_t *cmd = json_obj_get(prereqs, "commands");
    *out_env_vars = skill_prereqs_normalize_values(env);
    *out_commands = skill_prereqs_normalize_values(cmd);
}

/* PoP: skill_prereqs_normalize_setup @ tools/skills_tool.py:_normalize_setup_metadata */
json_t *skill_prereqs_normalize_setup(const json_t *frontmatter)
{
    json_t *out = json_object();
    json_set(out, "help", json_null());
    json_set(out, "collect_secrets", json_array());
    if (!frontmatter || frontmatter->type != JSON_OBJECT) return out;
    json_t *setup = json_obj_get(frontmatter, "setup");
    if (!setup || setup->type != JSON_OBJECT) return out;

    /* help: str and stripped -> str, else None (json_null) */
    json_t *help_val = json_obj_get(setup, "help");
    if (help_val && help_val->type == JSON_STRING) {
        const char *s = help_val->str_val;
        size_t sp = strspn(s, " \t\r\n");
        if (s[sp] != '\0') {
            json_set(out, "help", json_string(s));
        }
    }

    /* collect_secrets: dict -> [dict], list -> list (copied), else [] */
    json_t *cs = json_obj_get(setup, "collect_secrets");
    json_t *secrets = json_array();
    if (cs) {
        if (cs->type == JSON_OBJECT) {
            json_append(secrets, json_copy(cs));
        } else if (cs->type == JSON_ARRAY) {
            size_t n = json_len(cs);
            for (size_t i = 0; i < n; i++) {
                json_append(secrets, json_copy(json_get(cs, i)));
            }
        }
    }
    json_set(out, "collect_secrets", secrets);
    return out;
}
