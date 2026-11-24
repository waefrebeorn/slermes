#ifndef NOC_W18_CLICHATTU_3F16F7_H
#define NOC_W18_CLICHATTU_3F16F7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_clichattu_3f16f7_chat_resolve_interrupt @ hermes_cli/cli_chat_turn_mixin.py:CLIChatTurnMixin._chat_resolve_interrupt
 * Returns a NEW json_t* array [pending_message, show_marker] matching the
 * method's return value.  `result` is turn.result (JSON object or null);
 * `interrupt_msg`/`response` are injected strings; `thread_alive` and
 * `interrupt_requested` are the flags the method reads (they only drive the
 * clear_interrupt side effect, which has no representation here).  Caller frees. */
struct json_t *w18_clichattu_3f16f7_chat_resolve_interrupt(
    const struct json_t *result, const char *interrupt_msg, const char *response,
    int thread_alive, int interrupt_requested);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CLICHATTU_3F16F7_H */
