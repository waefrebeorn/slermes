#ifndef NOC_W12_CLIINFOMI_EA6329_H
#define NOC_W12_CLIINFOMI_EA6329_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_cliinfomi_ea6329_tool_progress_label @ hermes_cli/cli_info_mixin.py:_tool_progress_label
 * Colors and translation are injected as deterministic stubs. */
struct json_t *w12_cliinfomi_ea6329_tool_progress_label(const char *mode);

/* PoP: w12_cliinfomi_ea6329_reload_mcp_choices @ hermes_cli/cli_info_mixin.py:_reload_mcp_choices */
struct json_t *w12_cliinfomi_ea6329_reload_mcp_choices(void);

/* PoP: w12_cliinfomi_ea6329_help_section_title @ hermes_cli/cli_info_mixin.py:_help_section_title */
struct json_t *w12_cliinfomi_ea6329_help_section_title(const char *category);

/* PoP: w12_cliinfomi_ea6329_skill_line @ hermes_cli/cli_info_mixin.py:_skill_line */
struct json_t *w12_cliinfomi_ea6329_skill_line(const struct json_t *item);

/* PoP: w12_cliinfomi_ea6329_toolset_map @ hermes_cli/cli_info_mixin.py:_toolset_map
 * get_toolset_for_tool is injected as a name -> toolset id map. */
struct json_t *w12_cliinfomi_ea6329_toolset_map(const struct json_t *tools,
        const struct json_t *availability, const struct json_t *toolset_of);

/* PoP: w12_cliinfomi_ea6329_ascii_box @ hermes_cli/cli_info_mixin.py:_ascii_box
 * Returns the three printed lines as a JSON array (the Python original prints them). */
struct json_t *w12_cliinfomi_ea6329_ascii_box(const char *title, int width);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_CLIINFOMI_EA6329_H */
