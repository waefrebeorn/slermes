/*
 * process_registry_stdin.c - C home for upstream Hermes tools/process_registry_stdin.py.
 *
 * PoP: w16_processreg_d10a54_no_pty_reason @ tools/process_registry_stdin.py:_no_pty_reason
 * PoP: w16_processreg_d10a54_already_exited_error @ tools/process_registry_stdin.py:already_exited_error
 * PoP: w16_processreg_d10a54_stdin_unavailable_error @ tools/process_registry_stdin.py:stdin_unavailable_error
 *
 * Fixture seams: process_exists / stdin_open model ``session.process`` and
 * ``session.process.stdin``; pty_fallback and output_log are the session
 * strings; pty_available is the result of ``_pty_available()``.
 */
#include "noc_w16_processreg_d10a54.h"
#include <string.h>

#define NON_INTERACTIVE "Pass input non-interactively instead (arguments, a here-doc, or `printf ... | cmd`)."

/* Builds the reason string into out (capacity >= 512). */
static void reason_buf(char *out, size_t cap, const char *fallback,
                       const char *output_log, int pty_available) {
    out[0] = '\0';
    if (fallback && *fallback) {
        snprintf(out, cap, "pty=true was requested but %s, so it runs without stdin", fallback);
    } else if (output_log && *output_log) {
        snprintf(out, cap, "%s",
                 "background processes here run in an isolated worker that cannot give them a pseudo-terminal");
    } else if (!pty_available) {
        snprintf(out, cap, "%s",
                 "the PTY library (ptyprocess/pywinpty) is not installed");
    }
}

json_t *w16_processreg_d10a54_no_pty_reason(const char *pty_fallback,
                                            const char *output_log, int pty_available) {
    char r[1024];
    reason_buf(r, sizeof(r), pty_fallback, output_log, pty_available);
    return json_string(r);
}

json_t *w16_processreg_d10a54_already_exited_error(int process_exists, int stdin_open,
                                                  const char *pty_fallback,
                                                  const char *output_log, int pty_available) {
    if (!process_exists || stdin_open)
        return json_string("Process has already finished");
    char r[1024], msg[2048];
    reason_buf(r, sizeof(r), pty_fallback, output_log, pty_available);
    if (r[0]) {
        snprintf(msg, sizeof(msg),
                 "Process has already finished. It ran without stdin (%s), so if it read stdin, "
                 "it got end-of-file at once. " NON_INTERACTIVE, r);
    } else {
        snprintf(msg, sizeof(msg),
                 "Process has already finished. It was started without stdin (terminal "
                 "background=true without pty=true), so if it read stdin, it got end-of-file at "
                 "once. Relaunch it with terminal(background=true, pty=true) to send input.");
    }
    return json_string(msg);
}

json_t *w16_processreg_d10a54_stdin_unavailable_error(int process_exists,
                                                     const char *pty_fallback,
                                                     const char *output_log, int pty_available) {
    char msg[2048];
    if (!process_exists) {
        snprintf(msg, sizeof(msg),
                 "Process stdin not available: it runs on a non-local terminal backend (or was "
                 "recovered after a restart), which has no live stdin. " NON_INTERACTIVE);
        return json_string(msg);
    }
    char r[1024];
    reason_buf(r, sizeof(r), pty_fallback, output_log, pty_available);
    if (r[0]) {
        snprintf(msg, sizeof(msg),
                 "This process has no stdin: %s. Relaunching with pty=true would not help. "
                 NON_INTERACTIVE, r);
    } else {
        snprintf(msg, sizeof(msg),
                 "This process was started without stdin (terminal background=true without "
                 "pty=true). Relaunch it with terminal(background=true, pty=true) to send input.");
    }
    return json_string(msg);
}
