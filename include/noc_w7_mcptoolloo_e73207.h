#ifndef NOC_W7_MCPTOOLLOO_E73207_H
#define NOC_W7_MCPTOOLLOO_E73207_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure decision logic of tools/mcp_tool_loop.py. Asyncio objects and the
 * shared _core state are injected as booleans/JSON by the caller. */

/* PoP: w7_mcptoolloo_e73207_should_forward @ tools/mcp_tool_loop.py:_mcp_loop_exception_handler
 * 1 when the context should be forwarded to loop.default_exception_handler
 * (all exceptions except a RuntimeError whose text contains
 * "Event loop is closed"). */
int w7_mcptoolloo_e73207_should_forward(int exc_is_runtime, const char *message);

/* PoP: w7_mcptoolloo_e73207_wait_ready @ tools/mcp_tool_loop.py:_wait_for_server_session_ready
 * timeout gives the iteration count max(1, int(max(timeout,0)/0.25)). `script`
 * is a JSON array of per-iteration observations: "session" (0=None, 1=old,
 * 2=distinct), "ready_present", "ready", "ready_raises". Returns 1 when a
 * fresh, ready session is observed. */
int w7_mcptoolloo_e73207_wait_ready(double timeout, const json_t *script);

/* PoP: w7_mcptoolloo_e73207_signal_reconnect @ tools/mcp_tool_loop.py:_signal_reconnect
 * 0=no reconnect event, 1=direct event.set(), 2=loop.call_soon_threadsafe. */
int w7_mcptoolloo_e73207_signal_reconnect(int has_event, int event_is_asyncio, int loop_running);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_MCPTOOLLOO_E73207_H */
