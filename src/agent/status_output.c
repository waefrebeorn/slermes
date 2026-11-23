/*
 * status_output.c — C port of the pure gating decisions in upstream Hermes
 * agent/status_output.py (StatusOutputMixin).
 *
 * PoP: statusout_should_emit_quiet_tool_messages @ agent/status_output.py:StatusOutputMixin._should_emit_quiet_tool_messages
 * PoP: statusout_should_start_quiet_spinner @ agent/status_output.py:StatusOutputMixin._should_start_quiet_spinner
 * PoP: statusout_safe_print @ agent/status_output.py:StatusOutputMixin._safe_print
 * PoP: statusout_vprint @ agent/status_output.py:StatusOutputMixin._vprint
 *
 * The oracle drives the real class methods on a fake agent with injected
 * predicate results; these functions are those methods' gate logic.
 */
#include "noc_statusout.h"
#include <string.h>

int statusout_should_emit_quiet_tool_messages(int suppress_status_output, int quiet_mode,
                                              int has_tool_progress_callback, const char *platform) {
    if (suppress_status_output) return 0;                /* strict machine-readable mode wins */
    return quiet_mode && !has_tool_progress_callback &&
           platform && !strcmp(platform, "cli");
}

int statusout_should_start_quiet_spinner(int has_print_fn, int stdout_isatty) {
    return has_print_fn || stdout_isatty;               /* no raw spinner on a non-TTY */
}

int statusout_safe_print(int mute_notification_reply, int diagnostic, int warning_enabled) {
    if (mute_notification_reply) return 0;
    if (diagnostic && !warning_enabled) return 0;
    return 1;
}

int statusout_vprint(int suppress_status_output, int diagnostic, int warning_enabled,
                     int force, int mute_post_response, int has_stream_consumers,
                     int executing_tools, int mute_notification_reply) {
    if (suppress_status_output) return 0;
    if (diagnostic && !warning_enabled) return 0;
    int gate = force || !(mute_post_response || (has_stream_consumers && !executing_tools));
    if (!gate) return 0;
    /* falls through to _safe_print, whose own gate applies (diagnostic=False there) */
    if (mute_notification_reply) return 0;
    return 1;
}
