#ifndef NOC_W6_TURNFACADE_BF7ACE_H
#define NOC_W6_TURNFACADE_BF7ACE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_turnfacade_bf7ace_chat @ agent/turn_facade.py:TurnFacadeMixin.chat
 * Mirrors `run_conversation(...)["final_response"]`.  A missing key maps to
 * JSON null (Python would raise KeyError); returns a NEW json_t (caller frees). */
struct json_t *w6_turnfacade_bf7ace_chat(const struct json_t *result);

/* PoP: w6_turnfacade_bf7ace_outcome @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * The relay outcome derived from a terminal result: cancelled when
 * result["interrupted"] is True, failed when result["failed"] is True, else
 * success.  Returns a borrowed static string. */
const char *w6_turnfacade_bf7ace_outcome(const struct json_t *terminal);

/* PoP: w6_turnfacade_bf7ace_exception_outcome @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * Exception classification in the `except BaseException` block.  `exc_type` is
 * Python's type(exc).__name__.  Returns a borrowed static string. */
const char *w6_turnfacade_bf7ace_exception_outcome(const char *exc_type);

/* PoP: w6_turnfacade_bf7ace_task_context @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * Builds the {"session_id", "task_id", "platform"} task context. Caller frees. */
struct json_t *w6_turnfacade_bf7ace_task_context(const char *session_id,
                                                 const char *task_id,
                                                 const char *platform);

/* PoP: w6_turnfacade_bf7ace_relay_turn_id @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * f"{session_id or 'session'}:{task_id}:{uuid4().hex[:8]}"; the uuid hex is
 * injected as a seam. Caller frees. */
char *w6_turnfacade_bf7ace_relay_turn_id(const char *session_id, const char *task_id,
                                         const char *uuid_hex);

/* PoP: w6_turnfacade_bf7ace_relay_parent @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * Parent session id when platform == "subagent", else the empty string.
 * Caller frees. */
char *w6_turnfacade_bf7ace_relay_parent(const char *parent_session_id, const char *platform);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_TURNFACADE_BF7ACE_H */
