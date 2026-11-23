#ifndef NOC_W15_PLUGINSDIS_6F9432_H
#define NOC_W15_PLUGINSDIS_6F9432_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the pure system-prompt-section / hook-identity helpers in
 * hermes_cli/plugins_dispatch.py (gate w15_pluginsdis_6f9432).
 * Returned char* values are malloc'd and caller-owned. */

/* PoP: w15_pluginsdis_6f9432_is_valid_system_prompt_section_id @ hermes_cli/plugins_dispatch.py:is_valid_system_prompt_section_id
 * ^[a-z0-9][a-z0-9._-]{0,127}$ fullmatch on a str. */
int w15_pluginsdis_6f9432_is_valid_system_prompt_section_id(const char *value);

/* PoP: w15_pluginsdis_6f9432_format_system_prompt_section @ hermes_cli/plugins_dispatch.py:format_system_prompt_section
 * The char count in the marker is len(content) in Unicode code points. */
char *w15_pluginsdis_6f9432_format_system_prompt_section(const char *section_id,
                                                        const char *content);

/* PoP: w15_pluginsdis_6f9432_format_system_prompt_sections @ hermes_cli/plugins_dispatch.py:format_system_prompt_sections
 * `sections` is a JSON array of {"id":..., "content":...}; "" when empty. */
char *w15_pluginsdis_6f9432_format_system_prompt_sections(struct json_t *sections);

/* PoP: w15_pluginsdis_6f9432_hook_call_identity @ hermes_cli/plugins_dispatch.py:_hook_call_identity
 * Non-empty string identity from kwargs, or NULL. `kwargs` is borrowed. */
char *w15_pluginsdis_6f9432_hook_call_identity(struct json_t *kwargs,
                                              const char *hook_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PLUGINSDIS_6F9432_H */
