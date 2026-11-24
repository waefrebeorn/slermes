/*
 * noc_w25_toolavaila_80adee.h - C surface for upstream
 * hermes_cli/tool_availability_notices.py (gate w25_toolavaila_80adee).
 *
 * Pure rendering: the startup lines that say which toolsets are switched off
 * because their requirements are not met, plus the selection filter that keeps
 * only the entries this session would actually load.  Everything here is a
 * function of values: the ``unavailable`` rows from
 * ``check_tool_availability()``, the CLI's configured toolset selection, and
 * the tool names ``toolsets.resolve_toolset`` expands it to.  Nothing prints,
 * nothing reads the environment, nothing starts.
 */
#ifndef NOC_W25_TOOLAVAILA_80ADEE_H
#define NOC_W25_TOOLAVAILA_80ADEE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* request: {"unavailable": [{"name","tools","env_vars"}...],
 *           "enabled": [<toolset name>...],
 *           "resolve": {<toolset name>: [<tool name>...]}}
 * -> the subset of ``unavailable`` this selection would load.  An empty
 * selection means "everything"; a name ``resolve`` does not know is skipped
 * exactly like the live ``except Exception: continue``. */
json_t *w25_toolavaila_80adee_filter_enabled(const json_t *request);

/* request: {"unavailable": [...], "terminal_reason": <str|null>,
 *           "terminal_backend": <str>} -> the rich-markup startup lines. */
json_t *w25_toolavaila_80adee_warning_lines(const json_t *request);

/* request: {"backend": <str>, "reason": <str|null>} -> the terminal notice. */
json_t *w25_toolavaila_80adee_terminal_line(const json_t *request);

/* The module's literal tables: the multi-provider notice map and the footer. */
json_t *w25_toolavaila_80adee_constants(void);

#ifdef __cplusplus
}
#endif
#endif
