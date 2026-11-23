#ifndef NOC_STATUSOUT_H
#define NOC_STATUSOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: statusout_should_emit_quiet_tool_messages @ agent/status_output.py:StatusOutputMixin._should_emit_quiet_tool_messages */
int statusout_should_emit_quiet_tool_messages(int suppress_status_output, int quiet_mode,
                                              int has_tool_progress_callback, const char *platform);

/* PoP: statusout_should_start_quiet_spinner @ agent/status_output.py:StatusOutputMixin._should_start_quiet_spinner */
int statusout_should_start_quiet_spinner(int has_print_fn, int stdout_isatty);

/* PoP: statusout_safe_print @ agent/status_output.py:StatusOutputMixin._safe_print
 * 1 when the print sinks is reached (before the sink itself can throw). */
int statusout_safe_print(int mute_notification_reply, int diagnostic, int warning_enabled);

/* PoP: statusout_vprint @ agent/status_output.py:StatusOutputMixin._vprint
 * 1 when the message reaches the print sink. */
int statusout_vprint(int suppress_status_output, int diagnostic, int warning_enabled,
                     int force, int mute_post_response, int has_stream_consumers,
                     int executing_tools, int mute_notification_reply);

#ifdef __cplusplus
}
#endif
#endif /* NOC_STATUSOUT_H */
