/*
 * port_close_terminal_tool.h — Public API for the faithful C port of
 * tools/close_terminal_tool.py. Self-contained; mirrors only what external
 * callers (registry init, tests) need.
 */
#ifndef PORT_CLOSE_TERMINAL_TOOL_H
#define PORT_CLOSE_TERMINAL_TOOL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* check_close_terminal_requirements @ tools/close_terminal_tool.py
 * Returns true only when HERMES_DESKTOP is set to 1/true/yes (matches the
 * Python check_fn gating the tool to the desktop GUI). */
bool check_close_terminal_requirements(void);

/* Registry registration for the close_terminal tool. Wires the handler and
 * the desktop-only check_fn. Idempotent-safe to call at startup. */
void registry_init_close_terminal(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CLOSE_TERMINAL_TOOL_H */
