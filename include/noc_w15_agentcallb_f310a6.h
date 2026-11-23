#ifndef NOC_W15_AGENTCALLB_F310A6_H
#define NOC_W15_AGENTCALLB_F310A6_H
/*
 * Public C API for the port of tui_gateway/agent_callbacks.py
 * (gate w15_agentcallb_f310a6). All exported symbols are gate-prefixed.
 *
 * PoP: w15_agentcallb_f310a6_parse_tui_skills_env          @ tui_gateway/agent_callbacks.py:_parse_tui_skills_env
 * PoP: w15_agentcallb_f310a6_preview_tool_result_preview   @ tui_gateway/agent_callbacks.py:_preview_tool_result_preview
 * PoP: w15_agentcallb_f310a6_preview_restart_history       @ tui_gateway/agent_callbacks.py:_preview_restart_history
 *
 * The three pure data transforms of the module are ported: the env-string
 * skill parser (env read is a resolved-fact seam), the terminal tool-result
 * previewer and the preview-restart history distiller. The callback wiring
 * needs the live server object graph and is not claimed as a PoP export.
 */
#include "hermes_json.h"

/* list(dict.fromkeys(p.strip() for p in raw.replace("\n", ",").split(",") if p.strip())) */
json_t *w15_agentcallb_f310a6_parse_tui_skills_env(const char *raw);

/* _preview_tool_result_preview(name, result): returns a newly allocated string. */
char *w15_agentcallb_f310a6_preview_tool_result_preview(const char *name, const char *result);

/* _preview_restart_history(messages, max_messages, max_tool_chars): new JSON array. */
json_t *w15_agentcallb_f310a6_preview_restart_history(const json_t *messages,
        int max_messages, int max_tool_chars);

#endif /* NOC_W15_AGENTCALLB_F310A6_H */
