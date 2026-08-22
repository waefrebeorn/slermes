/*
 * computer_use_tool.h — C11 port of tools/computer_use_tool.py
 *
 * Python name -> C symbol mapping (module prefix `computer_use_tool_`):
 *
 *   registry.register(name="computer_use", ...)  -> computer_use_tool_register
 *   handle_computer_use (re-export)              -> computer_use_tool_handle_computer_use
 *   check_computer_use_requirements (re-export)  -> computer_use_tool_check_computer_use_requirements
 *   release_computer_use_session (re-export)     -> computer_use_tool_release_computer_use_session
 *   set_approval_callback (re-export)            -> cut_set_approval_callback
 *                                                   (src/tools/computer_use/computer_use_tool_ports.c)
 *
 * The real implementation lives in src/tools/computer_use.c (port of the
 * tools/computer_use/ package); this shim exists so tool discovery can
 * register `computer_use` from a top-level module, exactly like Python.
 */

#ifndef HERMES_COMPUTER_USE_TOOL_H
#define HERMES_COMPUTER_USE_TOOL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: module-level registry.register @ tools/computer_use_tool.py
 * Idempotent registration of the `computer_use` tool into the "computer_use"
 * toolset with COMPUTER_USE_SCHEMA and requires_env=[]. Returns true when the
 * registration succeeded. */
bool computer_use_tool_register(void);

/* PoP: handle_computer_use @ tools/computer_use_tool.py (re-export of
 * tools.computer_use.tool.handle_computer_use). Forwards to the real
 * implementation in src/tools/computer_use.c. */
char *computer_use_tool_handle_computer_use(const char *args_json,
                                            const char *task_id);

/* PoP: check_computer_use_requirements @ tools/computer_use/tool.py:1811
 * True iff computer_use can run on this host: Linux (always, for this C11
 * target) AND the cua-driver binary resolves via env/PATH/install paths. */
int computer_use_tool_check_computer_use_requirements(void);

/* PoP: release_computer_use_session @ tools/computer_use/tool.py:373
 * Release one session-owned computer-use backend. The C backend layer caches
 * a single global backend (there are no per-session backends to pop), so a
 * release drops that cached instance; returns 1 when a backend was found and
 * released, 0 when none was live. Safe to call repeatedly. */
int computer_use_tool_release_computer_use_session(const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_COMPUTER_USE_TOOL_H */
