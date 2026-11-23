#ifndef NOC_W8_TURNLOOPER_2EE1AA_H
#define NOC_W8_TURNLOOPER_2EE1AA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: turnlooper_handle_outer_loop_error @ agent/turn_loop_errors.py:handle_outer_loop_error
 *
 * The live function's loop-internal collaborators are injected as fixtures
 * (the oracle registers the same stubs in sys.modules): _MAX_OUTER_LOOP_ERRORS,
 * _is_interpreter_shutdown_error, sys.is_finalizing, short_detail, site_copy,
 * append_message and _ra().AIAgent._get_tool_call_name_static.  The returned
 * object holds the five OuterErrorVerdict fields plus the mutated messages. */
struct json_t *turnlooper_handle_outer_loop_error(
    int outer_error_count, int api_call_count, int max_iterations,
    int max_outer_errors, int is_finalizing, int shutdown_error,
    const char *e_text, struct json_t *tb_files, const char *session_id,
    int suppress_status_output, struct json_t *messages,
    const char *turn_exit_reason, int failed, struct json_t *final_response);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_TURNLOOPER_2EE1AA_H */
