#ifndef HERMES_PLUGIN_PROMPT_SECTIONS_H
#define HERMES_PLUGIN_PROMPT_SECTIONS_H

/*
 * plugin_prompt_sections.h — frozen plugin system-prompt sections.
 *
 * Port of agent/system_prompt.py (_restore_plugin_prompt_sections,
 * _plugin_section_blocks, restore_plugin_prompt_sections) plus the
 * container constants from hermes_cli/plugins.py.
 */

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#define PLUGIN_SECTIONS_START "<!-- hermes-plugin-sections:start -->"
#define PLUGIN_SECTIONS_END   "<!-- hermes-plugin-sections:end -->"
#define MAX_SYSTEM_PROMPT_SECTION_CHARS 4000

/* One rendered section: id + framed content. Strings malloc'd. */
typedef struct {
    char *id;
    char *content;
} plugin_section_t;

/* PoP: format_system_prompt_section @ hermes_cli/plugins.py
 * "## Plugin Context: <id>\n<!-- hermes-plugin-section-chars:<len> -->\n\n<content>"
 * malloc'd; caller frees. */
char *plugin_sections_format_one(const char *section_id, const char *content);

/* PoP: format_system_prompt_sections @ hermes_cli/plugins.py
 * sections is a NULL-terminated array of {id,content} pairs.
 * Empty/NULL → malloc'd "". Caller frees. */
char *plugin_sections_format(const plugin_section_t *sections);

/*
 * PoP: _restore_plugin_prompt_sections @ agent/system_prompt.py
 * Recover frozen section frames from the persisted full prompt. Returns a
 * malloc'd JSON array of {"id":..., "content":...} objects (empty array
 * when the prompt carries no valid section frame), or NULL on OOM.
 * Sections over MAX_SYSTEM_PROMPT_SECTION_CHARS are skipped, matching the
 * LIVE length guard. Caller frees the array.
 */
json_t *plugin_sections_restore(const char *prompt);

/* PoP: restore_plugin_prompt_sections @ agent/system_prompt.py
 * Seed the snapshot attribute: stores the restored array under
 * "_plugin_system_prompt_sections_snapshot" on the agent object (a json_t
 * object in the C port). */
void plugin_sections_restore_into(json_t *agent_state, const char *prompt);

#endif /* HERMES_PLUGIN_PROMPT_SECTIONS_H */
