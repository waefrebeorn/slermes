#ifndef NOC_SESSCLOSE_H
#define NOC_SESSCLOSE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: sessclose_closed_idle_unviewed @ gateway/session_local_close.py:closed_idle_unviewed
 * PoP: sessclose_closed_and_unviewed @ gateway/session_local_close.py:closed_and_unviewed
 *
 * Faithful ports of the two picker-view verdicts. Runtime collaborators are
 * injected as data:
 *   live            - JSON null or {"subscribers":<n>,"detached":<bool>}
 *   execution_state - handle.execution_state ("idle" is the only passing value)
 *   target_ended_at - the row's ended_at (JSON null == None)
 *   history_mount   - scripted is_history_mount(live)
 *   target          - scripted receipt.current_target (null => not a local session)
 *   receipt_raises  - inject the AuthorityRefusal caught by _local_target
 *   no_row          - list_row(target) returned None
 *   row_ended_at    - list_row(target).ended_at
 *
 * Each function returns a NEWLY allocated json_t BOOL that the caller frees.
 */
struct json_t *sessclose_closed_idle_unviewed(const struct json_t *live,
                                              const char *execution_state,
                                              const struct json_t *target_ended_at,
                                              int history_mount);

struct json_t *sessclose_closed_and_unviewed(const struct json_t *live,
                                             const char *execution_state,
                                             int history_mount,
                                             const char *target,
                                             int receipt_raises, int no_row,
                                             const struct json_t *row_ended_at);

#ifdef __cplusplus
}
#endif

#endif /* NOC_SESSCLOSE_H */
