#ifndef NOC_W22_STATE_FD0065_H
#define NOC_W22_STATE_FD0065_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/pet/state.py (gate w22_state_fd0065).
 *
 * "What is the agent doing?" -> "which sprite row?" lives here. The module is
 * already pure: `derive_pet_state` ranks seven coarse boolean signals and
 * `todos_all_done` folds a todo list into the JUMP celebrate beat. Both run
 * verbatim against the live quarry module in tests/oracle_w22_state_fd0065.py.
 */

/* PoP: w22_state_fd0065_derive_pet_state @ agent/pet/state.py:derive_pet_state
 * First truthy signal in priority order wins:
 *   error -> "FAILED", celebrate -> "JUMP", just_completed -> "WAVE",
 *   awaiting_input -> "WAITING", tool_running -> "RUN", reasoning -> "REVIEW",
 *   busy -> "RUN", otherwise "IDLE".
 * Signals arrive as a kwargs object; Python truthiness applies (None/False/0/""/
 * empty container are false). A missing key reads as the default False. */
const char *w22_state_fd0065_derive_pet_state(const json_t *flags);

/* PoP: w22_state_fd0065_todos_all_done @ agent/pet/state.py:todos_all_done
 * 1 iff the iterable is non-empty and every item's status is "completed" or
 * "cancelled", else 0. -1 when the live call raises TypeError (`list(todos)` on
 * a truthy non-iterable such as a number or True). Dict items are read with
 * `.get("status")`, every other item with `getattr(item, "status", None)` --
 * which is None for JSON scalars, arrays and strings. */
int w22_state_fd0065_todos_all_done(const json_t *todos);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_STATE_FD0065_H */
