/*
 * port_producer_lifecycle.h — pure helpers from
 * gateway/session_producer_lifecycle.py. The asyncio production bracket
 * (_open/close/producer_turn) and _turns registry stay unported.
 */
#ifndef PORT_PRODUCER_LIFECYCLE_H
#define PORT_PRODUCER_LIFECYCLE_H

/* _executed_outcome(captured): classify an admission's execution_result into a
 * ProcessingOutcome. Returns a static literal:
 *   "cancelled" when result.interrupted is truthy,
 *   "failure"  when result.failed or result.error is truthy,
 *   "success"  otherwise.
 * captured_json is the captured dict; its "terminal_result" wins over "result"
 * even when that key is present-but-null (Python .get(default) semantics). */
const char *spl_executed_outcome(const char *captured_json);

#endif /* PORT_PRODUCER_LIFECYCLE_H */
